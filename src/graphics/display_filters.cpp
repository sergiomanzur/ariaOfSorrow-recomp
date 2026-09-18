#include "graphics/display_filters.hpp"
#include <imgui.h>
#include <cmath>
#include <algorithm>

namespace aria::graphics {

void DisplayFilterRenderer::RenderFilter(ImDrawList* drawList,
                                        float screenX, float screenY,
                                        float screenW, float screenH,
                                        aria::config::DisplayFilter filter,
                                        int baseW, int baseH) {
    if (!drawList || filter == aria::config::DisplayFilter::None) {
        return;
    }

    switch (filter) {
        case aria::config::DisplayFilter::GbaLcdGrid:
            RenderLcdGrid(drawList, screenX, screenY, screenW, screenH, baseW, baseH, 40);
            break;

        case aria::config::DisplayFilter::Ags001Frontlit:
            RenderLcdGrid(drawList, screenX, screenY, screenW, screenH, baseW, baseH, 30);
            RenderFrontlitWash(drawList, screenX, screenY, screenW, screenH);
            break;

        case aria::config::DisplayFilter::Ags101Backlit:
            // High-contrast, sharp dark pixel matrix
            RenderLcdGrid(drawList, screenX, screenY, screenW, screenH, baseW, baseH, 55);
            break;

        case aria::config::DisplayFilter::CrtApertureGrille:
            RenderCrtAperture(drawList, screenX, screenY, screenW, screenH, baseH);
            break;

        default:
            break;
    }
}

void DisplayFilterRenderer::RenderLcdGrid(ImDrawList* drawList,
                                         float x, float y,
                                         float w, float h,
                                         int baseW, int baseH,
                                         uint8_t gridAlpha) {
    if (baseW <= 0 || baseH <= 0 || w <= 0.0f || h <= 0.0f) return;

    float pixelW = w / static_cast<float>(baseW);
    float pixelH = h / static_cast<float>(baseH);

    // If pixels are smaller than 2 physical screen pixels, don't draw grid (avoid moire)
    if (pixelW < 2.0f || pixelH < 2.0f) return;

    ImU32 gridColor = IM_COL32(0, 0, 0, gridAlpha);

    // Horizontal grid lines between GBA pixel rows
    for (int r = 1; r < baseH; ++r) {
        float lineY = y + std::floor(r * pixelH);
        drawList->AddLine(ImVec2(x, lineY), ImVec2(x + w, lineY), gridColor, 1.0f);
    }

    // Vertical grid lines between GBA pixel columns
    for (int c = 1; c < baseW; ++c) {
        float lineX = x + std::floor(c * pixelW);
        drawList->AddLine(ImVec2(lineX, y), ImVec2(lineX, y + h), gridColor, 1.0f);
    }

    // Subtle subpixel triads if scale is 4x or higher
    if (pixelW >= 4.0f) {
        float subpixelW = pixelW / 3.0f;
        ImU32 rTint = IM_COL32(255, 0, 0, 10);
        ImU32 bTint = IM_COL32(0, 0, 255, 10);

        for (int c = 0; c < baseW; ++c) {
            float colLeft = x + c * pixelW;
            // Red subpixel slit
            drawList->AddRectFilled(
                ImVec2(colLeft, y),
                ImVec2(colLeft + subpixelW * 0.8f, y + h),
                rTint);
            // Blue subpixel slit
            drawList->AddRectFilled(
                ImVec2(colLeft + subpixelW * 2.0f, y),
                ImVec2(colLeft + pixelW, y + h),
                bTint);
        }
    }
}

void DisplayFilterRenderer::RenderFrontlitWash(ImDrawList* drawList,
                                              float x, float y,
                                              float w, float h) {
    // AGS-001 frontlight characteristic: very slight ambient cyan/white surface reflection
    ImU32 washColor = IM_COL32(220, 240, 255, 18);
    drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), washColor);
}

void DisplayFilterRenderer::RenderCrtAperture(ImDrawList* drawList,
                                             float x, float y,
                                             float w, float h,
                                             int baseH) {
    if (baseH <= 0 || h <= 0.0f) return;

    float scanlineH = h / static_cast<float>(baseH);
    if (scanlineH < 2.0f) return;

    // Dark scanline gaps between active beam rows
    ImU32 scanlineColor = IM_COL32(0, 0, 0, 70);
    for (int r = 0; r < baseH; ++r) {
        float topY = y + r * scanlineH + (scanlineH * 0.65f);
        float bottomY = y + (r + 1) * scanlineH;
        drawList->AddRectFilled(ImVec2(x, topY), ImVec2(x + w, bottomY), scanlineColor);
    }

    // Aperture grille vertical phosphor stripe modulation
    float triadPitch = std::max(3.0f, scanlineH * 0.75f);
    int numTriads = static_cast<int>(w / triadPitch);
    ImU32 stripeColor = IM_COL32(0, 0, 0, 30);
    for (int i = 0; i < numTriads; ++i) {
        float stripeX = x + i * triadPitch;
        drawList->AddLine(ImVec2(stripeX, y), ImVec2(stripeX, y + h), stripeColor, 1.0f);
    }
}

} // namespace aria::graphics
