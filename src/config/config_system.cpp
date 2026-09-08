#include "config_system.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace aria::config {

ConfigSystem::ConfigSystem() {
    ResetToDefaults();
}

ConfigSystem& ConfigSystem::Get() {
    static ConfigSystem instance;
    return instance;
}

void ConfigSystem::ResetToDefaults() {
    m_config = AriaConfig{};

    // Default keyboard bindings
    m_config.controls.keyboardBindings["dpad_up"] = 82;     // Up
    m_config.controls.keyboardBindings["dpad_down"] = 81;   // Down
    m_config.controls.keyboardBindings["dpad_left"] = 80;   // Left
    m_config.controls.keyboardBindings["dpad_right"] = 79;  // Right
    m_config.controls.keyboardBindings["jump"] = 29;        // Z (A button)
    m_config.controls.keyboardBindings["attack"] = 27;      // X (B button)
    m_config.controls.keyboardBindings["soul"] = 4;         // A (R shoulder)
    m_config.controls.keyboardBindings["backdash"] = 22;    // S (L shoulder)
    m_config.controls.keyboardBindings["start"] = 40;       // Enter
    m_config.controls.keyboardBindings["select"] = 42;      // Backspace
    m_config.controls.keyboardBindings["rewind"] = 21;      // R
    m_config.controls.keyboardBindings["menu"] = 41;        // Escape
}

namespace {
std::string Trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}
} // namespace

bool ConfigSystem::LoadFromFile(const std::string& configPath) {
    std::ifstream file(configPath);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    std::string currentSection;

    while (std::getline(file, line)) {
        std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') continue;

        if (trimmed.front() == '[' && trimmed.back() == ']') {
            currentSection = trimmed.substr(1, trimmed.size() - 2);
            continue;
        }

        auto eqPos = trimmed.find('=');
        if (eqPos != std::string::npos) {
            std::string key = Trim(trimmed.substr(0, eqPos));
            std::string val = Trim(trimmed.substr(eqPos + 1));

            if (currentSection == "Paths") {
                if (key == "romPath") m_config.paths.romPath = val;
                else if (key == "biosPath") m_config.paths.biosPath = val;
                else if (key == "saveDirectory") m_config.paths.saveDirectory = val;
            } else if (currentSection == "Display") {
                if (key == "windowWidth") m_config.display.windowWidth = std::stoi(val);
                else if (key == "windowHeight") m_config.display.windowHeight = std::stoi(val);
                else if (key == "fullscreen") m_config.display.fullscreen = (val == "true" || val == "1");
                else if (key == "borderless") m_config.display.borderless = (val == "true" || val == "1");
                else if (key == "vsync") m_config.display.vsync = (val == "true" || val == "1");
                else if (key == "targetRefreshRate") m_config.display.targetRefreshRate = std::stoi(val);
                else if (key == "frameInterpolation") m_config.display.frameInterpolation = (val == "true" || val == "1");
                else if (key == "integerScaling") m_config.display.integerScaling = (val == "true" || val == "1");
                else if (key == "aspectRatio") m_config.display.aspectRatio = static_cast<AspectRatioMode>(std::stoi(val));
                else if (key == "colorMode") m_config.display.colorMode = static_cast<ColorCorrectionMode>(std::stoi(val));
            } else if (currentSection == "Audio") {
                if (key == "mode") m_config.audio.mode = static_cast<AudioMode>(std::stoi(val));
                else if (key == "masterVolume") m_config.audio.masterVolume = std::stof(val);
                else if (key == "musicVolume") m_config.audio.musicVolume = std::stof(val);
                else if (key == "sfxVolume") m_config.audio.sfxVolume = std::stof(val);
                else if (key == "muteOnUnfocused") m_config.audio.muteOnUnfocused = (val == "true" || val == "1");
            } else if (currentSection == "Gameplay") {
                if (key == "fastText") m_config.gameplay.fastText = (val == "true" || val == "1");
                else if (key == "skipIntro") m_config.gameplay.skipIntro = (val == "true" || val == "1");
                else if (key == "skipSeenCutscenes") m_config.gameplay.skipSeenCutscenes = (val == "true" || val == "1");
                else if (key == "fastDoorTransitions") m_config.gameplay.fastDoorTransitions = (val == "true" || val == "1");
                else if (key == "soulDropMultiplier") m_config.gameplay.soulDropMultiplier = std::stof(val);
                else if (key == "farmPitySystem") m_config.gameplay.farmPitySystem = (val == "true" || val == "1");
                else if (key == "dialogueDarkenPercent") m_config.gameplay.dialogueDarkenPercent = std::stoi(val);
            } else if (currentSection == "Cheats") {
                if (key == "enableCheats") m_config.cheats.enableCheats = (val == "true" || val == "1");
                else if (key == "infiniteHP") m_config.cheats.infiniteHP = (val == "true" || val == "1");
                else if (key == "infiniteMP") m_config.cheats.infiniteMP = (val == "true" || val == "1");
                else if (key == "expMultiplier") m_config.cheats.expMultiplier = std::stof(val);
            } else if (currentSection == "Rewind") {
                if (key == "enableRewind") m_config.rewind.enableRewind = (val == "true" || val == "1");
                else if (key == "bufferDurationSeconds") m_config.rewind.bufferDurationSeconds = std::stoi(val);
            }
        }
    }

    return true;
}

bool ConfigSystem::SaveToFile(const std::string& configPath) const {
    std::ofstream file(configPath);
    if (!file.is_open()) return false;

    file << "# AriaRecomp Configuration File\n\n";

    file << "[Paths]\n";
    file << "romPath = " << m_config.paths.romPath << "\n";
    file << "biosPath = " << m_config.paths.biosPath << "\n";
    file << "saveDirectory = " << m_config.paths.saveDirectory << "\n\n";

    file << "[Display]\n";
    file << "windowWidth = " << m_config.display.windowWidth << "\n";
    file << "windowHeight = " << m_config.display.windowHeight << "\n";
    file << "fullscreen = " << (m_config.display.fullscreen ? "true" : "false") << "\n";
    file << "borderless = " << (m_config.display.borderless ? "true" : "false") << "\n";
    file << "vsync = " << (m_config.display.vsync ? "true" : "false") << "\n";
    file << "targetRefreshRate = " << m_config.display.targetRefreshRate << "\n";
    file << "frameInterpolation = " << (m_config.display.frameInterpolation ? "true" : "false") << "\n";
    file << "integerScaling = " << (m_config.display.integerScaling ? "true" : "false") << "\n";
    file << "aspectRatio = " << static_cast<int>(m_config.display.aspectRatio) << "\n";
    file << "colorMode = " << static_cast<int>(m_config.display.colorMode) << "\n\n";

    file << "[Audio]\n";
    file << "mode = " << static_cast<int>(m_config.audio.mode) << "\n";
    file << "masterVolume = " << m_config.audio.masterVolume << "\n";
    file << "musicVolume = " << m_config.audio.musicVolume << "\n";
    file << "sfxVolume = " << m_config.audio.sfxVolume << "\n";
    file << "muteOnUnfocused = " << (m_config.audio.muteOnUnfocused ? "true" : "false") << "\n\n";

    file << "[Gameplay]\n";
    file << "fastText = " << (m_config.gameplay.fastText ? "true" : "false") << "\n";
    file << "skipIntro = " << (m_config.gameplay.skipIntro ? "true" : "false") << "\n";
    file << "skipSeenCutscenes = " << (m_config.gameplay.skipSeenCutscenes ? "true" : "false") << "\n";
    file << "fastDoorTransitions = " << (m_config.gameplay.fastDoorTransitions ? "true" : "false") << "\n";
    file << "soulDropMultiplier = " << m_config.gameplay.soulDropMultiplier << "\n";
    file << "farmPitySystem = " << (m_config.gameplay.farmPitySystem ? "true" : "false") << "\n";
    file << "dialogueDarkenPercent = " << m_config.gameplay.dialogueDarkenPercent << "\n\n";

    file << "[Cheats]\n";
    file << "enableCheats = " << (m_config.cheats.enableCheats ? "true" : "false") << "\n";
    file << "infiniteHP = " << (m_config.cheats.infiniteHP ? "true" : "false") << "\n";
    file << "infiniteMP = " << (m_config.cheats.infiniteMP ? "true" : "false") << "\n";
    file << "expMultiplier = " << m_config.cheats.expMultiplier << "\n\n";

    file << "[Rewind]\n";
    file << "enableRewind = " << (m_config.rewind.enableRewind ? "true" : "false") << "\n";
    file << "bufferDurationSeconds = " << m_config.rewind.bufferDurationSeconds << "\n";

    return true;
}

} // namespace aria::config
