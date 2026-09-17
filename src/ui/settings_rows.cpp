#include "ui/settings_rows.hpp"

#include <cstring>

#include "config/config_system.hpp"
#include "gameplay/cheat_system.hpp"
#include "gameplay/grant_system.hpp"
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

constexpr const char* kKeyCheatsEnable     = "cheats.enable";
constexpr const char* kKeyCheatsInfiniteHp = "cheats.infinite_hp";
constexpr const char* kKeyCheatsInfiniteMp = "cheats.infinite_mp";

// Level-up cheats: two one-shot actions sharing a single INT row that holds
// the "set to target" destination level.
constexpr const char* kKeyCheatsLevelUpOne    = "cheats.level_up_one";
constexpr const char* kKeyCheatsTargetLevel   = "cheats.target_level";
constexpr const char* kKeyCheatsSetLevel      = "cheats.set_level";

// One-shot grants. These are action rows, not toggles: activating one queues
// a single edit of the save block, applied on the next game frame.
constexpr const char* kKeyGrantBulletSouls   = "cheats.grant_bullet_souls";
constexpr const char* kKeyGrantGuardianSouls = "cheats.grant_guardian_souls";
constexpr const char* kKeyGrantEnchantSouls  = "cheats.grant_enchant_souls";
constexpr const char* kKeyGrantAbilitySouls  = "cheats.grant_ability_souls";
constexpr const char* kKeyGrantWeapons       = "cheats.grant_weapons";
constexpr const char* kKeyGrantArmor         = "cheats.grant_armor";
constexpr const char* kKeyGrantConsumables   = "cheats.grant_consumables";
constexpr const char* kKeyRevealMap          = "cheats.reveal_map";

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
        kKeyCheatsLevelUpOne, "Cheats", "Level up (+1)",
        "Gains exactly one level, applying the same stat growth the game "
        "itself would.",
        RECOMP_RUNTIME_UI_ACTION, 0, 0, 0, nullptr, 0, nullptr,
    },
    {
        kKeyCheatsTargetLevel, "Cheats", "Target level",
        "Destination level for \"Set level to target\" below.",
        RECOMP_RUNTIME_UI_INT, 1, aria::gameplay::grant_offsets::kMaxLevel, 1,
        nullptr, 0, nullptr,
    },
    {
        kKeyCheatsSetLevel, "Cheats", "Set level to target",
        "Levels up to the target level above in one step. Never lowers the "
        "current level.",
        RECOMP_RUNTIME_UI_ACTION, 0, 0, 0, nullptr, 0, nullptr,
    },
    // Labels and descriptions here are deliberately generic wording of our
    // own -- no in-game item, soul or enemy names appear anywhere in this
    // repository.
    {
        kKeyGrantBulletSouls, "Cheats", "Grant all bullet souls",
        "Adds one of every red (bullet) soul to the current save.",
        RECOMP_RUNTIME_UI_ACTION, 0, 0, 0, nullptr, 0, nullptr,
    },
    {
        kKeyGrantGuardianSouls, "Cheats", "Grant all guardian souls",
        "Adds one of every blue (guardian) soul to the current save.",
        RECOMP_RUNTIME_UI_ACTION, 0, 0, 0, nullptr, 0, nullptr,
    },
    {
        kKeyGrantEnchantSouls, "Cheats", "Grant all enchant souls",
        "Adds one of every yellow (enchant) soul to the current save.",
        RECOMP_RUNTIME_UI_ACTION, 0, 0, 0, nullptr, 0, nullptr,
    },
    {
        kKeyGrantAbilitySouls, "Cheats", "Grant all ability souls",
        "Adds every traversal ability soul to the current save.",
        RECOMP_RUNTIME_UI_ACTION, 0, 0, 0, nullptr, 0, nullptr,
    },
    {
        kKeyGrantWeapons, "Cheats", "Grant all weapons",
        "Puts one of every weapon in the inventory.",
        RECOMP_RUNTIME_UI_ACTION, 0, 0, 0, nullptr, 0, nullptr,
    },
    {
        kKeyGrantArmor, "Cheats", "Grant all armor and accessories",
        "Puts one of every body armor and accessory in the inventory.",
        RECOMP_RUNTIME_UI_ACTION, 0, 0, 0, nullptr, 0, nullptr,
    },
    {
        kKeyGrantConsumables, "Cheats", "Grant all consumables",
        "Puts one of every consumable item in the inventory.",
        RECOMP_RUNTIME_UI_ACTION, 0, 0, 0, nullptr, 0, nullptr,
    },
    {
        kKeyRevealMap, "Cheats", "Reveal full map",
        "Marks every room in the castle as visited on the map screen.",
        RECOMP_RUNTIME_UI_ACTION, 0, 0, 0, nullptr, 0, nullptr,
    },
};

} // namespace

void SetConfigSavePath(std::string path) { ConfigSavePath() = std::move(path); }

const void* AriaSettingsItems() { return kItems; }
std::size_t AriaSettingsItemCount() { return sizeof(kItems) / sizeof(kItems[0]); }

int AriaUiGet(const char* key, int* valueOut) {
    if (!key || !valueOut) return 0;
    const auto& cheats = aria::config::ConfigSystem::Get().GetConfig().cheats;
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
    if (std::strcmp(key, kKeyCheatsTargetLevel) == 0) {
        // Clamped on the way out too: a hand-edited aria_config.ini can hold
        // targetLevel = 250, and the row must never display a value outside
        // its own [1, kMaxLevel] range even before the user first touches it.
        int clamped = cheats.targetLevel;
        if (clamped > aria::gameplay::grant_offsets::kMaxLevel) {
            clamped = aria::gameplay::grant_offsets::kMaxLevel;
        }
        if (clamped < 1) clamped = 1;
        *valueOut = clamped;
        return 1;
    }
    return 0;
}

int AriaUiSet(const char* key, int value) {
    if (!key) return 0;
    auto& cheats = aria::config::ConfigSystem::Get().GetConfig().cheats;
    if (std::strcmp(key, kKeyCheatsEnable) == 0) {
        cheats.enableCheats = value != 0;
    } else if (std::strcmp(key, kKeyCheatsInfiniteHp) == 0) {
        cheats.infiniteHP = value != 0;
    } else if (std::strcmp(key, kKeyCheatsInfiniteMp) == 0) {
        cheats.infiniteMP = value != 0;
    } else if (std::strcmp(key, kKeyCheatsTargetLevel) == 0) {
        // Clamped here too (not just the INT row's minimum/maximum, which a
        // host could ignore) so this field can never hold anything the
        // level-up loop would need to re-clamp downstream.
        int clamped = value;
        if (clamped > aria::gameplay::grant_offsets::kMaxLevel) {
            clamped = aria::gameplay::grant_offsets::kMaxLevel;
        }
        if (clamped < 1) clamped = 1;
        cheats.targetLevel = clamped;
    } else {
        return 0;
    }
    SaveConfig();
    return 1;
}

// The overlay callback runs on the host's event loop while the recompiled
// game is running on its own thread, so nothing here may write guest memory:
// Request only raises a flag, and the per-frame EWRAM hook in main.cpp
// (AriaEwramFrameWrite) applies it at a frame boundary.
int AriaUiAction(const char* key) {
    if (!key) return 0;
    using aria::gameplay::GrantKind;
    struct Entry { const char* key; GrantKind kind; };
    static const Entry kGrants[] = {
        {kKeyGrantBulletSouls,   GrantKind::BulletSouls},
        {kKeyGrantGuardianSouls, GrantKind::GuardianSouls},
        {kKeyGrantEnchantSouls,  GrantKind::EnchantSouls},
        {kKeyGrantAbilitySouls,  GrantKind::AbilitySouls},
        {kKeyGrantWeapons,       GrantKind::Weapons},
        {kKeyGrantArmor,         GrantKind::ArmorAndAccessories},
        {kKeyGrantConsumables,   GrantKind::Consumables},
        {kKeyRevealMap,          GrantKind::RevealMap},
        {kKeyCheatsLevelUpOne,   GrantKind::LevelUpOne},
    };
    // A grant with the master switch off would be a silent no-op (the row is
    // greyed out, but a host that ignores is_enabled could still get here).
    if (!aria::config::ConfigSystem::Get().GetConfig().cheats.enableCheats) {
        return 0;
    }
    // "Set level to target" needs the configured target level alongside the
    // request, unlike every other action row above, so it isn't in the
    // table-driven dispatch.
    if (std::strcmp(key, kKeyCheatsSetLevel) == 0) {
        const int target =
            aria::config::ConfigSystem::Get().GetConfig().cheats.targetLevel;
        aria::gameplay::GrantSystem::Get().RequestSetLevel(target);
        return 1;
    }
    for (const auto& entry : kGrants) {
        if (std::strcmp(key, entry.key) == 0) {
            aria::gameplay::GrantSystem::Get().Request(entry.kind);
            return 1;
        }
    }
    return 0;
}

int AriaUiEnabled(const char* key) {
    if (!key) return 1;
    // Only the keys this catalog owns are ever gated; every other key
    // (the engine's own display.*/graphics.*/assist.* rows) must fall
    // through as enabled -- runtime_ui_enabled() consults this for every
    // key it doesn't recognize itself, so a default of "disabled" here
    // would silently grey out the engine's own settings.
    const bool ownsKey = std::strncmp(key, "cheats.", 7) == 0;
    if (!ownsKey) return 1;
    if (std::strcmp(key, kKeyCheatsEnable) == 0) return 1;
    // Every other cheat row depends on the master switch, so the dependency
    // is visible rather than a silent no-op when it's off. Listed key by key
    // instead of "any cheats.* key": an unowned cheats.* key still falls
    // through to the fail-closed 0 below, so a row can never look live
    // without something behind it.
    const bool cheatsOn =
        aria::config::ConfigSystem::Get().GetConfig().cheats.enableCheats;
    static const char* const kGatedKeys[] = {
        kKeyCheatsInfiniteHp,   kKeyCheatsInfiniteMp,
        kKeyGrantBulletSouls,   kKeyGrantGuardianSouls,
        kKeyGrantEnchantSouls,  kKeyGrantAbilitySouls,
        kKeyGrantWeapons,       kKeyGrantArmor,
        kKeyGrantConsumables,   kKeyRevealMap,
        kKeyCheatsLevelUpOne,   kKeyCheatsTargetLevel,
        kKeyCheatsSetLevel,
    };
    for (const char* gated : kGatedKeys) {
        if (std::strcmp(key, gated) == 0) return cheatsOn ? 1 : 0;
    }
    return 0;
}

} // namespace aria::ui
