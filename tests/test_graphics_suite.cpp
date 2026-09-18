#include <cassert>
#include <iostream>
#include <vector>
#include <cstring>

#include "config/config_system.hpp"
#include "graphics/adaptive_widescreen.hpp"

// Symbols defined in gbarecomp
extern "C" int g_ws_pillarbox;
extern "C" int g_ws_pillarbox_left;
extern "C" int g_ws_pillarbox_right;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::cerr << "Assertion failed: " #cond " at " << __FILE__     \
                      << ":" << __LINE__ << "\n";                          \
            return 1;                                                      \
        }                                                                  \
    } while (0)

int main() {
    using namespace aria::graphics;
    using namespace aria::config;

    std::cout << "=== Running Graphics & Presentation Test Suite ===\n\n";

    // =========================================================================
    // 1. Config System: Display Filter and Graphics Settings Serialization
    // =========================================================================
    std::cout << "[TEST] Display Filters & Graphics Config...\n";
    {
        auto& configSys = ConfigSystem::Get();
        configSys.ResetToDefaults();
        auto& config = configSys.GetConfig();

        // Check defaults
        CHECK(config.display.displayFilter == DisplayFilter::None);
        CHECK(config.graphics.adaptiveWidescreen == true);
        CHECK(config.graphics.hdFonts == true);
        CHECK(config.graphics.hdDialogueBox == true);

        // Modify and test all filter enums
        config.display.displayFilter = DisplayFilter::GbaLcdGrid;
        config.display.integerScaling = true;
        config.graphics.adaptiveWidescreen = true;

        const std::string testIniPath = "test_graphics_config.ini";
        CHECK(configSys.SaveToFile(testIniPath));

        // Reset and reload
        configSys.ResetToDefaults();
        CHECK(configSys.LoadFromFile(testIniPath));
        const auto& loaded = configSys.GetConfig();

        CHECK(loaded.display.displayFilter == DisplayFilter::GbaLcdGrid);
        CHECK(loaded.display.integerScaling == true);
        CHECK(loaded.graphics.adaptiveWidescreen == true);
        CHECK(loaded.graphics.hdFonts == true);
        CHECK(loaded.graphics.hdDialogueBox == true);

        // Test CRT filter selection
        configSys.GetConfig().display.displayFilter = DisplayFilter::CrtApertureGrille;
        CHECK(configSys.SaveToFile(testIniPath));
        configSys.ResetToDefaults();
        CHECK(configSys.LoadFromFile(testIniPath));
        CHECK(configSys.GetConfig().display.displayFilter == DisplayFilter::CrtApertureGrille);

        std::remove(testIniPath.c_str());
        std::cout << "  -> Display Filters & Graphics Config: PASSED\n\n";
    }

    // =========================================================================
    // 2. True Adaptive Widescreen: Single-Screen Room Fallback
    // =========================================================================
    std::cout << "[TEST] Adaptive Widescreen: Single-Screen Room Fallback...\n";
    {
        auto& ws = AdaptiveWidescreen::Get();
        std::vector<uint8_t> ewram(0x40000, 0);

        // Single-screen room metadata structure in EWRAM at 0x10000 (guest 0x02010000)
        // unk_0: 1 (1 screen wide = 240px)
        // unk_1: 1 (1 screen high = 160px)
        uint8_t* meta = ewram.data() + 0x10000;
        meta[0] = 1; // 1 screen wide
        meta[1] = 1; // 1 screen high

        // Set bgInfo[1].pBgMetadata at EWRAM 0x0A094 to guest address 0x02010000
        *reinterpret_cast<uint32_t*>(ewram.data() + 0x0A094) = 0x02010000;
        // Camera at (0, 0)
        *reinterpret_cast<uint16_t*>(ewram.data() + 0x0A094 + 0x06) = 0;
        *reinterpret_cast<uint16_t*>(ewram.data() + 0x0A094 + 0x0A) = 0;

        ws.UpdateFrame(nullptr, ewram.data(), ewram.size());

        // Must enforce full pillarbox (240x160) because room is only 1 screen wide
        CHECK(g_ws_pillarbox == 1);
        CHECK(g_ws_pillarbox_left == 0);
        CHECK(g_ws_pillarbox_right == 0);
        CHECK(ws.IsInMultiScreenRoom() == false);

        std::cout << "  -> Single-Screen Room Fallback: PASSED\n\n";
    }

    // =========================================================================
    // 3. True Adaptive Widescreen: Room Door Transition Fallback
    // =========================================================================
    std::cout << "[TEST] Adaptive Widescreen: Room Door Transition Fallback...\n";
    {
        auto& ws = AdaptiveWidescreen::Get();
        std::vector<uint8_t> ewram(0x40000, 0);

        // Multi-screen room metadata (4 screens wide)
        uint8_t* meta = ewram.data() + 0x10000;
        meta[0] = 4; // 4 screens wide = 1024px
        meta[1] = 1;

        *reinterpret_cast<uint32_t*>(ewram.data() + 0x0A094) = 0x02010000;
        *reinterpret_cast<uint16_t*>(ewram.data() + 0x0A094 + 0x06) = 200; // Middle of room

        // Mark room door transition active at EWRAM 0x00060 + 0x64
        ewram[0x00060 + 0x64] = 4;

        ws.UpdateFrame(nullptr, ewram.data(), ewram.size());

        // Must pillarbox to 240x160 during door transitions to prevent transition glitches
        CHECK(g_ws_pillarbox == 1);
        CHECK(ws.IsAdaptiveActive() == false);

        std::cout << "  -> Door Transition Fallback: PASSED\n\n";
    }

    // =========================================================================
    // 4. True Adaptive Widescreen: Dynamic Camera Margin Extension
    // =========================================================================
    std::cout << "[TEST] Adaptive Widescreen: Dynamic Edge & Center Extension...\n";
    {
        auto& ws = AdaptiveWidescreen::Get();
        std::vector<uint8_t> ewram(0x40000, 0);

        // 3 screens wide = 768px
        uint8_t* meta = ewram.data() + 0x10000;
        meta[0] = 3;
        meta[1] = 1;

        *reinterpret_cast<uint32_t*>(ewram.data() + 0x0A094) = 0x02010000;
        ewram[0x00060 + 0x64] = 0; // Transition inactive

        // Case A: Camera in center of room (x = 200)
        // Both left (x >= 22) and right (x <= 528 - 22) have ample space
        *reinterpret_cast<uint16_t*>(ewram.data() + 0x0A094 + 0x06) = 200;
        ws.UpdateFrame(nullptr, ewram.data(), ewram.size());

        CHECK(ws.IsInMultiScreenRoom() == true);
        CHECK(g_ws_pillarbox == 0);
        CHECK(g_ws_pillarbox_left == 0);
        CHECK(g_ws_pillarbox_right == 0);

        // Case B: Camera against left room wall (x = 5 < 22)
        *reinterpret_cast<uint16_t*>(ewram.data() + 0x0A094 + 0x06) = 5;
        ws.UpdateFrame(nullptr, ewram.data(), ewram.size());

        CHECK(g_ws_pillarbox == 0);
        CHECK(g_ws_pillarbox_left == 1);  // Left wall pillarboxed
        CHECK(g_ws_pillarbox_right == 0); // Right side open

        // Case C: Camera against right room wall (maxCameraX = 768 - 240 = 528, x = 520)
        *reinterpret_cast<uint16_t*>(ewram.data() + 0x0A094 + 0x06) = 520;
        ws.UpdateFrame(nullptr, ewram.data(), ewram.size());

        CHECK(g_ws_pillarbox == 0);
        CHECK(g_ws_pillarbox_left == 0);  // Left side open
        CHECK(g_ws_pillarbox_right == 1); // Right wall pillarboxed

        std::cout << "  -> Dynamic Edge & Center Extension: PASSED\n\n";
    }

    // =========================================================================
    // 5. True Adaptive Widescreen: Tilemap Provider Decoding
    // =========================================================================
    std::cout << "[TEST] Adaptive Widescreen: Tilemap Provider Decoding...\n";
    {
        auto& ws = AdaptiveWidescreen::Get();
        std::vector<uint8_t> ewram(0x40000, 0);

        // 2 screens wide = 512px
        uint8_t* meta = ewram.data() + 0x10000;
        meta[0] = 2; // 2 screens wide
        meta[1] = 1;

        // Block map pointer unk_C at meta + 0x0C -> guest 0x02012000 (offset 0x12000)
        *reinterpret_cast<uint32_t*>(meta + 0x0C) = 0x02012000;
        uint16_t* blockMap = reinterpret_cast<uint16_t*>(ewram.data() + 0x12000);

        // Put a block with metatile ID 5 at block (1, 0)
        // Entry: (metatileId 5) = 0x0005 (1-based, so ID 4)
        blockMap[1] = 0x0005;

        // Metatile table in EWRAM at 0x0A108 (cvaos code_08001194.c:640)
        uint16_t* metatiles = reinterpret_cast<uint16_t*>(ewram.data() + 0x0A108);
        // Metatile 4 (ID 5 - 1 = 4): base offset = 4 * 16 = 64
        // Set tile entry at (subX = 0, subY = 0)
        metatiles[64] = 0x0123; // tile index 0x123

        *reinterpret_cast<uint32_t*>(ewram.data() + 0x0A094) = 0x02010000;
        *reinterpret_cast<uint16_t*>(ewram.data() + 0x0A094 + 0x06) = 0; // cameraX = 0
        *reinterpret_cast<uint16_t*>(ewram.data() + 0x0A094 + 0x0A) = 0; // cameraY = 0

        ws.UpdateFrame(nullptr, ewram.data(), ewram.size());

        // Sample at hw_x = 32 (block 1, subtile 0): worldX = 32, tileX = 4, blockX = 1, subX = 0
        uint16_t outEntry = 0;
        int res = ws.ProvideTilemapEntry(1, 32, 0, &outEntry);

        CHECK(res == 1); // kWsTilemapReplace
        CHECK(outEntry == 0x0123);

        // Sample outside room bounds (worldX = 600 > 512)
        int outOfBoundsRes = ws.ProvideTilemapEntry(1, 600, 0, &outEntry);
        CHECK(outOfBoundsRes == 0); // kWsTilemapUnavailable

        std::cout << "  -> Tilemap Provider Decoding: PASSED\n\n";
    }

    std::cout << "ALL GRAPHICS & PRESENTATION TESTS PASSED (100% SUCCESS)\n";
    return 0;
}
