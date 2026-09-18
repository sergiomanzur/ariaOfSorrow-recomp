#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

#if defined(GBARECOMP_RUNTIME_UI)
#include "imgui.h"
#endif

namespace aria::graphics {

class HdBackgroundSystem {
public:
    static HdBackgroundSystem& Get() {
        static HdBackgroundSystem instance;
        return instance;
    }

    bool LoadBackgroundAssets(const std::string& basePath = "");

    void RenderBackground(
#if defined(GBARECOMP_RUNTIME_UI)
        ImDrawList* bgDrawList,
#else
        void* bgDrawList,
#endif
        float vpX, float vpY, float vpW, float vpH,
        int baseW, int baseH,
        const uint8_t* ewram, size_t ewramSize);

    void CompositeToFramebuffer(uint8_t* rgb, int width, int height,
                                const uint8_t* ewram, size_t ewramSize,
                                uint64_t frameCount);

    bool IsLoaded() const { return m_isLoaded; }

private:
    HdBackgroundSystem();
    ~HdBackgroundSystem() = default;

    bool m_isLoaded = false;
    int m_bgWidth = 480;
    int m_bgHeight = 270;
    std::vector<uint8_t> m_bgRgb;
};

} // namespace aria::graphics
