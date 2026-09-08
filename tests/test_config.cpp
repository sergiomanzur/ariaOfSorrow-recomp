#include <iostream>
#include <cassert>
#include <cstdio>
#include "../src/config/config_system.hpp"

void TestDefaultConfig() {
    auto& cfg = aria::config::ConfigSystem::Get();
    cfg.ResetToDefaults();

    assert(cfg.GetConfig().display.windowWidth == 1280);
    assert(cfg.GetConfig().display.windowHeight == 720);
    assert(cfg.GetConfig().display.vsync == true);
    assert(cfg.GetConfig().rewind.enableRewind == true);
    assert(cfg.GetConfig().rewind.bufferDurationSeconds == 10);
    assert(cfg.GetConfig().cheats.enableCheats == false);

    std::cout << "[PASS] TestDefaultConfig\n";
}

void TestConfigSaveLoadRoundtrip() {
    auto& cfg = aria::config::ConfigSystem::Get();
    cfg.ResetToDefaults();

    // Modify some values
    cfg.GetConfig().display.windowWidth = 1920;
    cfg.GetConfig().display.windowHeight = 1080;
    cfg.GetConfig().display.targetRefreshRate = 144;
    cfg.GetConfig().display.frameInterpolation = true;
    cfg.GetConfig().gameplay.fastText = true;
    cfg.GetConfig().gameplay.soulDropMultiplier = 2.5f;

    const std::string tempFile = "test_config_temp.ini";
    bool saved = cfg.SaveToFile(tempFile);
    assert(saved);

    // Reset and reload
    cfg.ResetToDefaults();
    assert(cfg.GetConfig().display.windowWidth == 1280);

    bool loaded = cfg.LoadFromFile(tempFile);
    assert(loaded);

    assert(cfg.GetConfig().display.windowWidth == 1920);
    assert(cfg.GetConfig().display.windowHeight == 1080);
    assert(cfg.GetConfig().display.targetRefreshRate == 144);
    assert(cfg.GetConfig().display.frameInterpolation == true);
    assert(cfg.GetConfig().gameplay.fastText == true);
    assert(cfg.GetConfig().gameplay.soulDropMultiplier == 2.5f);

    std::remove(tempFile.c_str());
    std::cout << "[PASS] TestConfigSaveLoadRoundtrip\n";
}

int main() {
    std::cout << "--- Running ConfigSystem Unit Tests ---\n";
    TestDefaultConfig();
    TestConfigSaveLoadRoundtrip();
    std::cout << "--- All ConfigSystem Tests Passed! ---\n";
    return 0;
}
