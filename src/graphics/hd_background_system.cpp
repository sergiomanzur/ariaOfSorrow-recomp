#include "graphics/hd_background_system.hpp"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <cmath>
#include <algorithm>

namespace aria::graphics {

HdBackgroundSystem::HdBackgroundSystem() {
    LoadBackgroundAssets();
}

bool HdBackgroundSystem::LoadBackgroundAssets(const std::string& basePath) {
    if (m_isLoaded) return true;

    std::vector<std::string> searchPaths = {
        basePath + "assets/backgrounds/castle_eclipsed_moon_hd.bin",
        "assets/backgrounds/castle_eclipsed_moon_hd.bin",
        "../assets/backgrounds/castle_eclipsed_moon_hd.bin"
    };

    std::string foundPath;
    for (const auto& p : searchPaths) {
        if (!p.empty() && std::filesystem::exists(p)) {
            foundPath = p;
            break;
        }
    }

    if (foundPath.empty()) {
        std::cerr << "[WARN] HD Background asset not found: castle_eclipsed_moon_hd.bin\n";
        return false;
    }

    std::ifstream file(foundPath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    file.seekg(0, std::ios::end);
    size_t fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    char magic[4] = {0};
    file.read(magic, 4);
    if (magic[0] == 'H' && magic[1] == 'D' && magic[2] == 'B' && magic[3] == 'G') {
        uint32_t w = 0, h = 0, c = 0;
        file.read(reinterpret_cast<char*>(&w), sizeof(w));
        file.read(reinterpret_cast<char*>(&h), sizeof(h));
        file.read(reinterpret_cast<char*>(&c), sizeof(c));
        if (w > 0 && h > 0 && c == 3) {
            m_bgWidth = static_cast<int>(w);
            m_bgHeight = static_cast<int>(h);
        }
    } else {
        file.seekg(0, std::ios::beg);
        if (fileSize == 1024 * 640 * 3) {
            m_bgWidth = 1024;
            m_bgHeight = 640;
        } else if (fileSize == 480 * 270 * 3) {
            m_bgWidth = 480;
            m_bgHeight = 270;
        }
    }

    size_t expectedBytes = static_cast<size_t>(m_bgWidth * m_bgHeight * 3);
    m_bgRgb.resize(expectedBytes);
    file.read(reinterpret_cast<char*>(m_bgRgb.data()), expectedBytes);

    if (file.gcount() == static_cast<std::streamsize>(expectedBytes)) {
        m_isLoaded = true;
        std::cout << "[INFO] Loaded authentic HD Background asset: " << foundPath << " (" 
                  << m_bgWidth << "x" << m_bgHeight << ")\n";
        return true;
    }

    m_bgRgb.clear();
    return false;
}

void HdBackgroundSystem::RenderBackground(
#if defined(GBARECOMP_RUNTIME_UI)
    ImDrawList* bgDrawList,
#else
    void* bgDrawList,
#endif
    float vpX, float vpY, float vpW, float vpH,
    int baseW, int baseH,
    const uint8_t* ewram, size_t ewramSize) {
#if defined(GBARECOMP_RUNTIME_UI)
    if (!bgDrawList) return;

    // Outer margin fill or subtle ambient vignette
    ImVec2 minPos(vpX, vpY);
    ImVec2 maxPos(vpX + vpW, vpY + vpH);

    // Subtle atmospheric deep-indigo background tint
    bgDrawList->AddRectFilled(minPos, maxPos, IM_COL32(10, 14, 28, 255));
#else
    (void)bgDrawList; (void)vpX; (void)vpY; (void)vpW; (void)vpH;
    (void)baseW; (void)baseH; (void)ewram; (void)ewramSize;
#endif
}

void HdBackgroundSystem::CompositeToFramebuffer(uint8_t* rgb, int width, int height,
                                                const uint8_t* ewram, size_t ewramSize,
                                                uint64_t /*frameCount*/) {
    if (!m_isLoaded || !rgb || width <= 0 || height <= 0 || m_bgRgb.empty()) {
        return;
    }

    int camX = 0;
    int camY = 0;
    if (ewram && ewramSize >= 0x0A094 + 12) {
        camX = *reinterpret_cast<const uint16_t*>(ewram + 0x0A094 + 0x06);
        camY = *reinterpret_cast<const uint16_t*>(ewram + 0x0A094 + 0x0A);
    }

    // Parallax scrolling
    int scrollX = static_cast<int>(camX * 0.12f);
    int scrollY = static_cast<int>(camY * 0.06f);

    bool is16x9 = (width > 240);
    int marginW = is16x9 ? (width - 240) / 2 : 0;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int pxIndex = (y * width + x) * 3;
            uint8_t r = rgb[pxIndex + 0];
            uint8_t g = rgb[pxIndex + 1];
            uint8_t b = rgb[pxIndex + 2];

            // 1. Widescreen side margins: fill with authentic HD background
            bool inMargin = (marginW > 0 && (x < marginW || x >= width - marginW));

            // 2. Open sky backdrop (exact GBA backdrop palette and atmospheric sky gradient):
            bool isSkyBackdrop = (y < 42) && ((r == 99 && g == 107 && b == 132) ||
                                             (r >= 95 && r <= 130 && g >= 100 && g <= 135 && b >= 125 && b <= 175));

            if (inMargin || isSkyBackdrop) {
                int srcX = ((x * m_bgWidth) / width + scrollX) % m_bgWidth;
                if (srcX < 0) srcX += m_bgWidth;

                int srcY = ((y * m_bgHeight) / height + scrollY) % m_bgHeight;
                if (srcY < 0) srcY += m_bgHeight;

                int bgIndex = (srcY * m_bgWidth + srcX) * 3;
                uint8_t bgR = m_bgRgb[bgIndex + 0];
                uint8_t bgG = m_bgRgb[bgIndex + 1];
                uint8_t bgB = m_bgRgb[bgIndex + 2];

                if (inMargin) {
                    rgb[pxIndex + 0] = bgR;
                    rgb[pxIndex + 1] = bgG;
                    rgb[pxIndex + 2] = bgB;
                } else {
                    // Smooth celestial blend of authentic upscaled sky and moon
                    rgb[pxIndex + 0] = static_cast<uint8_t>((bgR * 92 + r * 8) / 100);
                    rgb[pxIndex + 1] = static_cast<uint8_t>((bgG * 92 + g * 8) / 100);
                    rgb[pxIndex + 2] = static_cast<uint8_t>((bgB * 92 + b * 8) / 100);
                }
            }
        }
    }
}

} // namespace aria::graphics
