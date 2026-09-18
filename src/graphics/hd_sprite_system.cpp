#include "graphics/hd_sprite_system.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <iostream>

#if defined(GBARECOMP_RUNTIME_UI)
#include <imgui.h>
#endif

namespace aria::graphics {

HdSpriteSystem::HdSpriteSystem() {
    Initialize();
}

bool HdSpriteSystem::Initialize(const std::string& assetDir) {
    if (m_loaded) return true;

    std::vector<std::string> candidatePaths = {
        assetDir + "/soma_hd_spritesheet.bin",
        "assets/sprites/soma_hd_spritesheet.bin",
        "../assets/sprites/soma_hd_spritesheet.bin",
        "../../assets/sprites/soma_hd_spritesheet.bin"
    };

    std::string foundPath;
    for (const auto& p : candidatePaths) {
        if (std::filesystem::exists(p)) {
            foundPath = p;
            break;
        }
    }

    if (foundPath.empty()) {
        std::cerr << "[HD_SPRITES] soma_hd_spritesheet.bin not found in candidate paths.\n";
        return false;
    }

    std::ifstream file(foundPath, std::ios::binary | std::ios::ate);
    if (!file) return false;

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    constexpr size_t expectedSize = 1280 * 256 * 4;
    if (size != static_cast<std::streamsize>(expectedSize)) {
        std::cerr << "[HD_SPRITES] Unexpected file size: " << size << " (expected " << expectedSize << ")\n";
        return false;
    }

    m_sheetRgba.resize(expectedSize);
    if (!file.read(reinterpret_cast<char*>(m_sheetRgba.data()), expectedSize)) {
        m_sheetRgba.clear();
        return false;
    }

    m_sheetW = 1280;
    m_sheetH = 256;
    m_frameW = 256;
    m_frameH = 256;
    m_frameCount = 5;
    m_loaded = true;

    std::cout << "[HD_SPRITES] Successfully loaded Soma Cruz HD spritesheet (" 
              << m_sheetW << "x" << m_sheetH << ", " << m_frameCount << " poses)\n";
    return true;
}

PlayerSpriteState HdSpriteSystem::DetectPlayerState(const uint8_t* ewram, size_t ewramSize, uint64_t currentFrame) {
    PlayerSpriteState state{};
    if (!ewram || ewramSize < 0x20000) return state;

    uint32_t playerPtr = *reinterpret_cast<const uint32_t*>(ewram + 0x13110);
    uint32_t offset = 0x004E4; // Default to Entity 0
    if (playerPtr >= 0x02000000 && playerPtr < 0x02040000) {
        offset = playerPtr - 0x02000000;
    }

    if (offset + 0x84 > ewramSize) return state;

    // Check if player entity is active
    uint32_t updateFunc = *reinterpret_cast<const uint32_t*>(ewram + offset);
    if (updateFunc == 0) return state;

    // In Aria of Sorrow, entity positions are fixed-point integers
    // unk_524 (0x524 relative to entity base 0x4E4 = offset 0x40): X coordinate
    // unk_528 (0x528 relative to entity base 0x4E4 = offset 0x44): Y coordinate
    int32_t worldX = *reinterpret_cast<const int16_t*>(ewram + offset + 0x42);
    int32_t worldY = *reinterpret_cast<const int16_t*>(ewram + offset + 0x46);

    // Camera position at EWRAM 0x0A078
    int32_t camX = *reinterpret_cast<const int16_t*>(ewram + 0x0A078 + 6);
    int32_t camY = *reinterpret_cast<const int16_t*>(ewram + 0x0A078 + 10);

    int screenX = worldX - camX;
    int screenY = worldY - camY;

    // Sane fallback if camera values are uninitialized (e.g. intro scene)
    if (screenX <= 0 || screenX >= 240 || screenY <= 0 || screenY >= 160) {
        screenX = 120;
        screenY = 112;
    }

    // Facing direction: entity offset + 0x58 or velocity
    int16_t xVel = *reinterpret_cast<const int16_t*>(ewram + offset + 0x48);
    int16_t yVel = *reinterpret_cast<const int16_t*>(ewram + offset + 0x4C);
    uint8_t flags = ewram[offset + 0x58];
    bool facingLeft = (flags & 1) != 0 || (xVel < 0);

    // Determine Action Pose
    uint16_t heldInput = *reinterpret_cast<const uint16_t*>(ewram + 0x00014);
    uint8_t attacking = ewram[0x131BF]; // lastUsedRedSoul / attack state

    SomaPose pose = SomaPose::Idle;
    if (attacking != 0 || (heldInput & 0x0001)) { // Attack button active
        pose = SomaPose::Attack;
    } else if (yVel != 0) { // Airborne
        pose = SomaPose::Jump;
    } else if ((heldInput & 0x0080) != 0) { // Down / Crouch
        pose = SomaPose::Crouch;
    } else if (xVel != 0 || (heldInput & 0x0030) != 0) { // Moving Left/Right
        pose = SomaPose::Walk;
    } else {
        pose = SomaPose::Idle;
    }

    state.screenX = screenX;
    state.screenY = screenY;
    state.facingLeft = facingLeft;
    state.pose = pose;
    state.valid = true;
    return state;
}

void HdSpriteSystem::RenderOverlay(ImDrawList* drawList,
                                  float vpX, float vpY, float vpW, float vpH,
                                  int baseW, int baseH,
                                  uint64_t currentFrame,
                                  const uint8_t* ewram, size_t ewramSize) {
#if defined(GBARECOMP_RUNTIME_UI)
    if (!drawList || baseW <= 0 || baseH <= 0) return;

    auto playerState = DetectPlayerState(ewram, ewramSize, currentFrame);
    if (!playerState.valid) return;

    float scaleX = vpW / static_cast<float>(baseW);
    float scaleY = vpH / static_cast<float>(baseH);

    // Screen coordinates on viewport
    float posX = vpX + playerState.screenX * scaleX;
    float posY = vpY + playerState.screenY * scaleY;

    // Render high-definition stylized marker / indicator for Soma's HD sprite
    float spriteH = 48.0f * scaleY;
    float spriteW = 32.0f * scaleX;
    float left = posX - spriteW * 0.5f;
    float top = posY - spriteH;

    // Distinctive color accents for Soma Cruz:
    // White trench coat, blue trim, crimson soul glow
    ImU32 coatColor = IM_COL32(245, 248, 255, 240);
    ImU32 trimColor = IM_COL32(50, 110, 220, 230);
    ImU32 shadowColor = IM_COL32(20, 20, 30, 180);
    ImU32 glowColor = (playerState.pose == SomaPose::Attack) ? IM_COL32(80, 200, 255, 200) : IM_COL32(255, 255, 255, 0);

    // Ground shadow
    drawList->AddEllipseFilled(ImVec2(posX, posY), ImVec2(spriteW * 0.45f, 4.0f * scaleY), IM_COL32(10, 10, 15, 140));

    // Attack slash energy trail
    if (playerState.pose == SomaPose::Attack) {
        float slashDir = playerState.facingLeft ? -1.0f : 1.0f;
        ImVec2 arcCenter(posX + slashDir * spriteW * 0.6f, top + spriteH * 0.45f);
        drawList->AddCircle(arcCenter, spriteW * 0.8f, glowColor, 16, 2.5f * scaleX);
    }
#else
    (void)drawList; (void)vpX; (void)vpY; (void)vpW; (void)vpH;
    (void)baseW; (void)baseH; (void)currentFrame; (void)ewram; (void)ewramSize;
#endif
}

void HdSpriteSystem::CompositeToFramebuffer(uint8_t* rgb, int width, int height,
                                            const uint8_t* ewram, size_t ewramSize,
                                            uint64_t currentFrame) {
    if (!rgb || width <= 0 || height <= 0 || !m_loaded) return;

    auto player = DetectPlayerState(ewram, ewramSize, currentFrame);
    if (!player.valid) return;

    // Sprite destination size on GBA native canvas: 40x48 pixels
    constexpr int targetW = 40;
    constexpr int targetH = 48;

    int dstMinX = player.screenX - targetW / 2;
    int dstMinY = player.screenY - targetH;

    // Frame column in spritesheet (256x256 per frame)
    int frameIndex = static_cast<int>(player.pose);
    if (frameIndex < 0 || frameIndex >= m_frameCount) frameIndex = 0;

    int srcBaseX = frameIndex * m_frameW;
    int srcBaseY = 0;

    for (int dy = 0; dy < targetH; ++dy) {
        int screenY = dstMinY + dy;
        if (screenY < 0 || screenY >= height) continue;

        // Sample Y in 256x256 frame
        int sy = (dy * m_frameH) / targetH;

        for (int dx = 0; dx < targetW; ++dx) {
            int screenX = dstMinX + dx;
            if (screenX < 0 || screenX >= width) continue;

            // Sample X in 256x256 frame (flipped horizontally if facing left)
            int sampleDx = player.facingLeft ? (targetW - 1 - dx) : dx;
            int sx = srcBaseX + (sampleDx * m_frameW) / targetW;

            size_t srcIdx = (sy * m_sheetW + sx) * 4;
            if (srcIdx + 3 >= m_sheetRgba.size()) continue;

            uint8_t a = m_sheetRgba[srcIdx + 3];
            if (a < 10) continue; // transparent pixel

            uint8_t r = m_sheetRgba[srcIdx + 0];
            uint8_t g = m_sheetRgba[srcIdx + 1];
            uint8_t b = m_sheetRgba[srcIdx + 2];

            size_t dstIdx = (screenY * width + screenX) * 3;
            // Alpha blend with background framebuffer
            rgb[dstIdx + 0] = static_cast<uint8_t>((r * a + rgb[dstIdx + 0] * (255 - a)) / 255);
            rgb[dstIdx + 1] = static_cast<uint8_t>((g * a + rgb[dstIdx + 1] * (255 - a)) / 255);
            rgb[dstIdx + 2] = static_cast<uint8_t>((b * a + rgb[dstIdx + 2] * (255 - a)) / 255);
        }
    }
}

} // namespace aria::graphics
