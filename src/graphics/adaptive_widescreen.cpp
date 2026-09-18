#include "graphics/adaptive_widescreen.hpp"
#include <algorithm>
#include <cstring>
#include <cstdio>

// Symbols from GBARecomp PPU
extern "C" int g_ws_pillarbox;
extern "C" int g_ws_pillarbox_left;
extern "C" int g_ws_pillarbox_right;

enum WsTilemapProviderResult : int {
    kWsTilemapUnavailable = 0,
    kWsTilemapReplace = 1,
    kWsTilemapKeepWrapped = 2,
};

namespace aria::graphics {

void AdaptiveWidescreen::SetRomData(const uint8_t* romData, size_t romSize) {
    m_romData = romData;
    m_romSize = romSize;
}

const uint8_t* AdaptiveWidescreen::ResolveGuestPointer(uint32_t guestPtr,
                                                      const uint8_t* ewram,
                                                      size_t ewramSize) const {
    if (guestPtr >= 0x02000000 && guestPtr < 0x02040000) {
        size_t offset = guestPtr - 0x02000000;
        if (offset < ewramSize) {
            return ewram + offset;
        }
    } else if (guestPtr >= 0x08000000 && guestPtr < 0x0A000000 && m_romData) {
        size_t offset = guestPtr - 0x08000000;
        if (offset < m_romSize) {
            return m_romData + offset;
        }
    }
    return nullptr;
}

void AdaptiveWidescreen::UpdateFrame(const gbarecomp::ExtendedViewFrameInfo* /*frame*/,
                                     const uint8_t* ewram,
                                     size_t ewramSize) {
    m_activeEwram = ewram;
    m_activeEwramSize = ewramSize;

    if (!ewram || ewramSize < 0x20000) {
        g_ws_pillarbox = 1;
        g_ws_pillarbox_left = 0;
        g_ws_pillarbox_right = 0;
        m_adaptiveActive = false;
        m_multiScreenRoom = false;
        return;
    }

    // bgInfo[1] is located at EWRAM 0x0A094 (cvaos structs/ewram.h)
    // 0x00: struct EwramData_unkA078_0 *pBgMetadata (4 bytes)
    // 0x04: union CameraPart xPos (fraction 2B at +0x04, integer 2B at +0x06)
    // 0x08: union CameraPart yPos (fraction 2B at +0x08, integer 2B at +0x0A)
    uint32_t bg1MetaGuest = *reinterpret_cast<const uint32_t*>(ewram + 0x0A094);
    m_pBg1MetadataGuest = bg1MetaGuest;

    if (bg1MetaGuest == 0) {
        // No room background metadata loaded (menus, title screen, BIOS)
        g_ws_pillarbox = 1;
        g_ws_pillarbox_left = 0;
        g_ws_pillarbox_right = 0;
        m_adaptiveActive = false;
        m_multiScreenRoom = false;
        return;
    }

    const uint8_t* bgMeta = ResolveGuestPointer(bg1MetaGuest, ewram, ewramSize);
    if (!bgMeta) {
        g_ws_pillarbox = 1;
        g_ws_pillarbox_left = 0;
        g_ws_pillarbox_right = 0;
        m_adaptiveActive = false;
        m_multiScreenRoom = false;
        return;
    }

    // Read room dimensions from pBgMetadata
    // unk_0: room width in screens / 256px blocks
    // unk_1: room height in screens / 256px blocks
    uint8_t roomWidthScreens = bgMeta[0];
    m_roomWidthScreens = roomWidthScreens;
    m_roomWidthPixels = roomWidthScreens * 256;

    // Read camera position
    m_cameraX = *reinterpret_cast<const uint16_t*>(ewram + 0x0A094 + 0x06);
    m_cameraY = *reinterpret_cast<const uint16_t*>(ewram + 0x0A094 + 0x0A);

    // Check if room door transition is active (EWRAM 0x00060)
    const uint8_t* transObj = ewram + 0x00060;
    bool transitionActive = (transObj[0x64] == 4);

    // Rooms with width <= 1 (single 240px screen), transitions, or menus
    // must strictly pillarbox to 240x160
    if (roomWidthScreens <= 1 || transitionActive) {
        g_ws_pillarbox = 1;
        g_ws_pillarbox_left = 0;
        g_ws_pillarbox_right = 0;
        m_adaptiveActive = false;
        m_multiScreenRoom = false;
        return;
    }

    // Multi-screen room: determine whether left and right margins can show true room tiles
    m_multiScreenRoom = true;
    m_adaptiveActive = true;

    constexpr int kExtraMargin = 22; // 16:9 view adds 22px on each side (240 -> 284)
    int maxCameraX = (roomWidthScreens * 256) - 240;

    // Left margin: can we expand 22px to the left of the camera?
    if (m_cameraX >= kExtraMargin) {
        g_ws_pillarbox_left = 0; // Space available to the left
    } else {
        g_ws_pillarbox_left = 1; // Pressed against left room wall
    }

    // Right margin: can we expand 22px to the right of the viewport?
    if (m_cameraX + kExtraMargin <= maxCameraX) {
        g_ws_pillarbox_right = 0; // Space available to the right
    } else {
        g_ws_pillarbox_right = 1; // Pressed against right room wall
    }

    // If both edges are blocked, enforce full pillarbox
    if (g_ws_pillarbox_left && g_ws_pillarbox_right) {
        g_ws_pillarbox = 1;
        g_ws_pillarbox_left = 0;
        g_ws_pillarbox_right = 0;
    } else {
        g_ws_pillarbox = 0;
    }
}

int AdaptiveWidescreen::ProvideTilemapEntry(int bg, int hw_x, int screen_y, uint16_t* out_entry) {
    if (!out_entry) return kWsTilemapUnavailable;

    // Only BG1 (main room layer) and BG2 are handled
    if (bg != 1 && bg != 2) {
        return kWsTilemapKeepWrapped;
    }

    if (!m_adaptiveActive || !m_activeEwram || !m_pBg1MetadataGuest) {
        return kWsTilemapUnavailable;
    }

    const uint8_t* bgMeta = ResolveGuestPointer(m_pBg1MetadataGuest, m_activeEwram, m_activeEwramSize);
    if (!bgMeta) return kWsTilemapUnavailable;

    // Calculate world pixel coordinate for the sample
    int worldX = m_cameraX + hw_x;
    int worldY = m_cameraY + screen_y;

    if (worldX < 0 || worldY < 0 || worldX >= m_roomWidthPixels) {
        return kWsTilemapUnavailable;
    }

    // Convert to tile coordinates (8x8 pixels per tile)
    uint16_t tileX = static_cast<uint16_t>(worldX >> 3);
    uint16_t tileY = static_cast<uint16_t>(worldY >> 3);

    // Read block map pointer (unk_C at bgMeta + 0x0C)
    uint32_t blockMapGuest = *reinterpret_cast<const uint32_t*>(bgMeta + 0x0C);
    const uint16_t* blockMap = reinterpret_cast<const uint16_t*>(
        ResolveGuestPointer(blockMapGuest, m_activeEwram, m_activeEwramSize));
    if (!blockMap) return kWsTilemapUnavailable;

    uint8_t roomWidthScreens = bgMeta[0];
    uint8_t blockX = tileX >> 2;
    uint8_t blockY = tileY >> 2;

    // Metatile block index in room
    size_t blockStride = static_cast<size_t>(roomWidthScreens) * 8;
    size_t blockIdx = blockX + (static_cast<size_t>(blockY) * blockStride);

    uint16_t blockEntry = blockMap[blockIdx];
    uint16_t flags = blockEntry & 0xC000;
    uint16_t metaId = blockEntry & 0x3FFF;

    if (metaId == 0) {
        return kWsTilemapUnavailable;
    }
    metaId -= 1;

    // Subtile within block (4x4 tiles per block)
    uint8_t subX = tileX & 3;
    if (flags & 0x4000) subX = 3 - subX;
    uint8_t subY = tileY & 3;
    if (flags & 0x8000) subY = 3 - subY;

    size_t metaOffset = (static_cast<size_t>(metaId) << 4) + subX + (static_cast<size_t>(subY) << 2);

    // Metatile table in EWRAM at 0x0A108 (cvaos code_08001194.c:640)
    const uint16_t* metatiles = reinterpret_cast<const uint16_t*>(m_activeEwram + 0x0A108);
    uint16_t rawEntry = metatiles[metaOffset];

    *out_entry = ((flags >> 12) << 8) ^ rawEntry;
    return kWsTilemapReplace;
}

} // namespace aria::graphics
