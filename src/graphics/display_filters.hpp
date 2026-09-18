#pragma once

#include <cstdint>
#include "config/config_system.hpp"

struct ImDrawList;
struct ImVec2;

namespace aria::graphics {

class DisplayFilterRenderer {
public:
    static DisplayFilterRenderer& Get() {
        static DisplayFilterRenderer instance;
        return instance;
    }

    void RenderFilter(ImDrawList* drawList,
                      float screenX, float screenY,
                      float screenW, float screenH,
                      aria::config::DisplayFilter filter,
                      int baseW = 240, int baseH = 160);

private:
    DisplayFilterRenderer() = default;

    void RenderLcdGrid(ImDrawList* drawList, float x, float y, float w, float h, int baseW, int baseH, uint8_t gridAlpha);
    void RenderFrontlitWash(ImDrawList* drawList, float x, float y, float w, float h);
    void RenderCrtAperture(ImDrawList* drawList, float x, float y, float w, float h, int baseH);
};

} // namespace aria::graphics
