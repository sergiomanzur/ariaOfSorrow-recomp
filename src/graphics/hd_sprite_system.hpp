#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

struct ImDrawList;

namespace aria::graphics {

enum class SomaPose : uint8_t {
    Idle = 0,
    Walk = 1,
    Jump = 2,
    Attack = 3,
    Crouch = 4
};

struct PlayerSpriteState {
    int screenX = 0;
    int screenY = 0;
    bool facingLeft = false;
    SomaPose pose = SomaPose::Idle;
    bool valid = false;
};

class HdSpriteSystem {
public:
    static HdSpriteSystem& Get() {
        static HdSpriteSystem instance;
        return instance;
    }

    bool Initialize(const std::string& assetDir = "assets/sprites");

    PlayerSpriteState DetectPlayerState(const uint8_t* ewram, size_t ewramSize, uint64_t currentFrame);

    void RenderOverlay(ImDrawList* drawList,
                       float vpX, float vpY, float vpW, float vpH,
                       int baseW, int baseH,
                       uint64_t currentFrame,
                       const uint8_t* ewram, size_t ewramSize);

    void CompositeToFramebuffer(uint8_t* rgb, int width, int height,
                                const uint8_t* ewram, size_t ewramSize,
                                uint64_t currentFrame);

    bool IsLoaded() const { return m_loaded; }
    int GetFrameWidth() const { return m_frameW; }
    int GetFrameHeight() const { return m_frameH; }

private:
    HdSpriteSystem();

    bool m_loaded = false;
    int m_sheetW = 1280;
    int m_sheetH = 256;
    int m_frameW = 256;
    int m_frameH = 256;
    int m_frameCount = 5;
    std::vector<uint8_t> m_sheetRgba;
};

} // namespace aria::graphics
