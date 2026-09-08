#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>

namespace aria::config {

enum class AspectRatioMode {
    Authentic_3_2,      // Standard 240x160 pillarboxed
    Widescreen_16_9,    // Adaptive Widescreen 16:9
    Widescreen_16_10,   // Adaptive Widescreen 16:10
    Ultrawide_21_9,     // Adaptive Ultrawide 21:9
    Stretched_Fill      // Stretched (non-adaptive, not recommended)
};

enum class ColorCorrectionMode {
    None,               // Raw RGB555 to sRGB
    GBA_Original,       // Accurate GBA LCD color curve and gamma
    Vibrant_Modern      // High-contrast vibrant modern display
};

enum class AudioMode {
    Authentic,          // Original GBA sound emulation
    Enhanced,           // Host-side high-res resampling and cubic interpolation
    Replacement         // High quality FLAC/OGG replacement soundtrack
};

enum class GlyphStyle {
    Auto,
    Xbox,
    PlayStation,
    NintendoSwitch,
    SteamDeck,
    Keyboard
};

struct DisplayConfig {
    int windowWidth = 1280;
    int windowHeight = 720;
    bool fullscreen = false;
    bool borderless = false;
    bool vsync = true;
    int targetRefreshRate = 60; // 60, 90, 120, 144, 240
    bool frameInterpolation = false;
    bool integerScaling = false;
    AspectRatioMode aspectRatio = AspectRatioMode::Authentic_3_2;
    ColorCorrectionMode colorMode = ColorCorrectionMode::GBA_Original;
    float brightness = 1.0f;
    float contrast = 1.0f;
};

struct GraphicsConfig {
    bool enableHdPack = false;
    std::string activeHdPackName = "default_hd";
    bool hdSprites = true;
    bool hdBackgrounds = true;
    bool hdUI = true;
    bool hdPortraits = true;
    bool hdFonts = true;
    bool bilinearFiltering = false;
};

struct AudioConfig {
    AudioMode mode = AudioMode::Authentic;
    float masterVolume = 1.0f;
    float musicVolume = 0.8f;
    float sfxVolume = 0.8f;
    bool muteOnUnfocused = true;
    std::string replacementMusicPack = "default_ost";
};

struct ControlsConfig {
    GlyphStyle glyphStyle = GlyphStyle::Auto;
    float analogDeadzone = 0.2f;
    bool analogToDpad = true;
    bool vibration = true;
    float vibrationStrength = 0.7f;
    // Action key bindings mapped as string descriptors
    std::unordered_map<std::string, int> keyboardBindings;
    std::unordered_map<std::string, int> gamepadBindings;
};

struct GameplayConfig {
    bool fastText = false;
    bool skipIntro = false;
    bool skipSeenCutscenes = false;
    bool fastDoorTransitions = false;
    bool deathQuickRetry = false;
    bool bossQuickRetry = false;
    bool autoSaveOnRoomChange = false;
    bool allowSaveAnywhere = false;
    float soulDropMultiplier = 1.0f;
    float itemDropMultiplier = 1.0f;
    bool farmPitySystem = false;
};

struct CheatsConfig {
    bool enableCheats = false;
    bool infiniteHP = false;
    bool infiniteMP = false;
    bool infiniteHearts = false;
    bool invincibility = false;
    bool oneHitKill = false;
    float expMultiplier = 1.0f;
    bool guaranteedSouls = false;
    bool unlockAllSouls = false;
    bool unlockAllItems = false;
    bool unlockFullMap = false;
    bool unlockAllWarps = false;
    bool noclip = false;
    bool freezeEnemies = false;
    float gameSpeed = 1.0f;
};

struct RewindConfig {
    bool enableRewind = true;
    int bufferDurationSeconds = 10;
    bool showRewindOsd = true;
};

struct SaveStateConfig {
    int currentSlot = 0;
    int totalSlots = 10;
    bool generateThumbnails = true;
    bool autoSaveOnExit = true;
};

struct AccessibilityConfig {
    float uiScale = 1.0f;
    float textScale = 1.0f;
    bool reduceFlashing = false;
    bool reduceScreenShake = false;
    bool highContrastMode = false;
    bool holdToTurbo = false;
};

struct DeveloperConfig {
    bool enableDebugOverlay = false;
    bool showFps = false;
    bool showPlayerCoords = false;
    bool showRoomId = false;
    bool showHitboxes = false;
    bool showCameraBounds = false;
    bool showVramViewer = false;
    bool logStateDeltas = false;
};

struct PathsConfig {
    std::string romPath;
    std::string biosPath;
    std::string saveDirectory;
};

struct AriaConfig {
    PathsConfig paths;
    DisplayConfig display;
    GraphicsConfig graphics;
    AudioConfig audio;
    ControlsConfig controls;
    GameplayConfig gameplay;
    CheatsConfig cheats;
    RewindConfig rewind;
    SaveStateConfig saveStates;
    AccessibilityConfig accessibility;
    DeveloperConfig developer;
};

class ConfigSystem {
public:
    static ConfigSystem& Get();

    bool LoadFromFile(const std::string& configPath);
    bool SaveToFile(const std::string& configPath) const;
    void ResetToDefaults();

    AriaConfig& GetConfig() { return m_config; }
    const AriaConfig& GetConfig() const { return m_config; }

private:
    ConfigSystem();
    AriaConfig m_config;
};

} // namespace aria::config
