#include "ui/settings_rows.hpp"

#include <cstring>

#include "config/config_system.hpp"
#include "gameplay/cheat_system.hpp"
#include "gameplay/qol_system.hpp"

#include "recomp_runtime_ui.h"

namespace aria::ui {
namespace {

std::string& ConfigSavePath() {
    static std::string path = "aria_config.ini";
    return path;
}

// Pushes the live config into the systems that read it once per frame.
// ui_set's write-through has to reach these directly: gbarecomp never wires
// recomp-ui's own `save` callback (runtime.cpp only sets get/set/action/
// is_enabled and the text pair), so nothing else re-synchronizes them.
void PushConfigToSystems() {
    auto& cfg = aria::config::ConfigSystem::Get().GetConfig();
    aria::gameplay::CheatSystem::Get().SetConfig(cfg.cheats);
    aria::gameplay::QolSystem::Get().SetConfig(cfg.gameplay);
}

void SaveConfig() {
    aria::config::ConfigSystem::Get().SaveToFile(ConfigSavePath());
    PushConfigToSystems();
}

constexpr const char* kKeyCheatsEnable          = "cheats.enable";
constexpr const char* kKeyCheatsInfiniteHp       = "cheats.infinite_hp";
constexpr const char* kKeyCheatsInfiniteMp       = "cheats.infinite_mp";
constexpr const char* kKeyGameplayDialogueDarken = "gameplay.dialogue_darken";

// Rows with no implementation behind them are never added: a row with no
// effect is worse than no row (see AriaUiEnabled's fallback for anything
// this catalog doesn't own, which is a separate, load-bearing concern).
// Aria of Sorrow has no "Hearts" resource -- its soul system consumes MP,
// not a separate currency -- so there is no third cheat row here; an
// earlier version of this project invented one alongside a never-verified
// player-struct address (see symbols/cvaos_symbols.hpp).
const RecompRuntimeUiItem kItems[] = {
    {
        kKeyCheatsEnable, "Cheats", "Enable cheats",
        "Master switch for the cheats below.",
        RECOMP_RUNTIME_UI_BOOL, 0, 0, 0, nullptr, 0, nullptr,
    },
    {
        kKeyCheatsInfiniteHp, "Cheats", "Infinite HP",
        "Keeps HP pinned to its current maximum every frame.",
        RECOMP_RUNTIME_UI_BOOL, 0, 0, 0, nullptr, 0, nullptr,
    },
    {
        kKeyCheatsInfiniteMp, "Cheats", "Infinite MP",
        "Keeps MP pinned to its current maximum every frame.",
        RECOMP_RUNTIME_UI_BOOL, 0, 0, 0, nullptr, 0, nullptr,
    },
    {
        kKeyGameplayDialogueDarken, "Quality of Life", "Dialogue darkness",
        "Biases translucent dialogue boxes toward opaque, for legibility. "
        "0 = authentic GBA look; 100 = fully opaque. Fades/transitions are "
        "a different blend mode and are never affected.",
        RECOMP_RUNTIME_UI_INT, 0, 100, 5, nullptr, 0, nullptr,
    },
};

} // namespace

void SetConfigSavePath(std::string path) { ConfigSavePath() = std::move(path); }

const void* AriaSettingsItems() { return kItems; }
std::size_t AriaSettingsItemCount() { return sizeof(kItems) / sizeof(kItems[0]); }

int AriaUiGet(const char* key, int* valueOut) {
    if (!key || !valueOut) return 0;
    const auto& cheats = aria::config::ConfigSystem::Get().GetConfig().cheats;
    const auto& gameplay = aria::config::ConfigSystem::Get().GetConfig().gameplay;
    if (std::strcmp(key, kKeyCheatsEnable) == 0) {
        *valueOut = cheats.enableCheats ? 1 : 0;
        return 1;
    }
    if (std::strcmp(key, kKeyCheatsInfiniteHp) == 0) {
        *valueOut = cheats.infiniteHP ? 1 : 0;
        return 1;
    }
    if (std::strcmp(key, kKeyCheatsInfiniteMp) == 0) {
        *valueOut = cheats.infiniteMP ? 1 : 0;
        return 1;
    }
    if (std::strcmp(key, kKeyGameplayDialogueDarken) == 0) {
        *valueOut = gameplay.dialogueDarkenPercent;
        return 1;
    }
    return 0;
}

int AriaUiSet(const char* key, int value) {
    if (!key) return 0;
    auto& cheats = aria::config::ConfigSystem::Get().GetConfig().cheats;
    auto& gameplay = aria::config::ConfigSystem::Get().GetConfig().gameplay;
    if (std::strcmp(key, kKeyCheatsEnable) == 0) {
        cheats.enableCheats = value != 0;
    } else if (std::strcmp(key, kKeyCheatsInfiniteHp) == 0) {
        cheats.infiniteHP = value != 0;
    } else if (std::strcmp(key, kKeyCheatsInfiniteMp) == 0) {
        cheats.infiniteMP = value != 0;
    } else if (std::strcmp(key, kKeyGameplayDialogueDarken) == 0) {
        if (value < 0) value = 0;
        if (value > 100) value = 100;
        gameplay.dialogueDarkenPercent = value;
    } else {
        return 0;
    }
    SaveConfig();
    return 1;
}

int AriaUiAction(const char* /*key*/) { return 0; }

int AriaUiEnabled(const char* key) {
    if (!key) return 1;
    // Only the keys this catalog owns are ever gated; every other key
    // (the engine's own display.*/graphics.*/assist.* rows) must fall
    // through as enabled -- runtime_ui_enabled() consults this for every
    // key it doesn't recognize itself, so a default of "disabled" here
    // would silently grey out the engine's own settings.
    const bool ownsKey =
        std::strncmp(key, "cheats.", 7) == 0 ||
        std::strncmp(key, "gameplay.", 9) == 0;
    if (!ownsKey) return 1;
    if (std::strcmp(key, kKeyCheatsEnable) == 0) return 1;
    if (std::strcmp(key, kKeyGameplayDialogueDarken) == 0) return 1;
    // The two real cheats depend on the master switch, so the dependency
    // is visible rather than a silent no-op when it's off.
    const bool cheatsOn =
        aria::config::ConfigSystem::Get().GetConfig().cheats.enableCheats;
    if (std::strcmp(key, kKeyCheatsInfiniteHp) == 0 ||
        std::strcmp(key, kKeyCheatsInfiniteMp) == 0) {
        return cheatsOn ? 1 : 0;
    }
    return 0;
}

} // namespace aria::ui
