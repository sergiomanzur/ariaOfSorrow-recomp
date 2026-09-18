#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace gbarecomp {
struct ExtendedViewFrameInfo;
}

namespace aria::graphics {

class AdaptiveWidescreen {
public:
    static AdaptiveWidescreen& Get() {
        static AdaptiveWidescreen instance;
        return instance;
    }

    void SetRomData(const uint8_t* romData, size_t romSize);

    void UpdateFrame(const gbarecomp::ExtendedViewFrameInfo* frame,
                     const uint8_t* ewram, size_t ewramSize);

    int ProvideTilemapEntry(int bg, int hw_x, int screen_y, uint16_t* out_entry);

    bool IsAdaptiveActive() const { return m_adaptiveActive; }
    bool IsInMultiScreenRoom() const { return m_multiScreenRoom; }
    int GetRoomWidthScreens() const { return m_roomWidthScreens; }
    int GetRoomWidthPixels() const { return m_roomWidthPixels; }
    int GetCameraX() const { return m_cameraX; }
    int GetCameraY() const { return m_cameraY; }

private:
    AdaptiveWidescreen() = default;

    const uint8_t* ResolveGuestPointer(uint32_t guestPtr, const uint8_t* ewram, size_t ewramSize) const;

    const uint8_t* m_romData = nullptr;
    size_t m_romSize = 0;

    bool m_adaptiveActive = false;
    bool m_multiScreenRoom = false;
    int m_roomWidthScreens = 1;
    int m_roomWidthPixels = 240;
    int m_cameraX = 0;
    int m_cameraY = 0;

    const uint8_t* m_activeEwram = nullptr;
    size_t m_activeEwramSize = 0;
    uint32_t m_pBg1MetadataGuest = 0;
};

} // namespace aria::graphics
