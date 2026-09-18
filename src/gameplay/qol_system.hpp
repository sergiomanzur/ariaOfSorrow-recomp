#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "config/config_system.hpp"

namespace aria::gameplay {

// Memory layout constants transcribed from cvaos (third_party/cvaos/include/structs/ewram.h)
namespace qol_offsets {
    constexpr uint32_t kEquippedWeapon     = 0x13268;
    constexpr uint32_t kEquippedRedSoul    = 0x13269;
    constexpr uint32_t kEquippedBlueSoul   = 0x1326A;
    constexpr uint32_t kEquippedYellowSoul = 0x1326B;
    constexpr uint32_t kEquippedArmor      = 0x1326C;
    constexpr uint32_t kEquippedAccessory  = 0x1326D;

    constexpr uint32_t kRedSoulInventory    = 0x1331C;
    constexpr uint32_t kBlueSoulInventory   = 0x13354;
    constexpr uint32_t kYellowSoulInventory = 0x1336E;
    constexpr uint32_t kAbilitySoulInventory= 0x13392;

    constexpr uint32_t kPlayerStatsBase    = 0x131EE;
    constexpr uint32_t kPlayerLuckStat     = 0x131F8; // currentStats[3]

    constexpr uint32_t kRoomTransitionObj  = 0x00060;
    constexpr uint32_t kMapGrid            = 0x000B4;
    constexpr uint32_t kCameraFracX        = 0x0A078 + 0x04;
    constexpr uint32_t kCameraFracY        = 0x0A078 + 0x08;

    // ROM Enemy Table: 0x080E9644 -> file offset 0x0E9644 (36 bytes per enemy entry)
    constexpr uint32_t kEnemyTableFileOffset = 0x0E9644;
    constexpr size_t   kEnemyEntrySize       = 36;
} // namespace qol_offsets

struct LoadoutPreset {
    uint8_t weapon = 0xFF;
    uint8_t redSoul = 0;
    uint8_t blueSoul = 0;
    uint8_t yellowSoul = 0;
    uint8_t armor = 0xFF;
    uint8_t accessory = 0xFF;
    bool initialized = false;
};

struct MiniMapCell {
    int x = 0;
    int y = 0;
    bool visited = false;
    bool revealed = false;
};

class QolSystem {
public:
    static QolSystem& Get();

    void Initialize(const aria::config::GameplayConfig& config);
    void SetConfig(const aria::config::GameplayConfig& config) { m_config = config; }
    const aria::config::GameplayConfig& GetConfig() const { return m_config; }

    // Applied every game frame at VBlank
    void Update(uint8_t* ewram, size_t ewramSize);

    // =========================================================================
    // 1. Quick Soul / Weapon Loadout Presets ("Dawn of Sorrow" Fix)
    // =========================================================================
    void CycleLoadout(uint8_t* ewram, size_t ewramSize, int direction = 1);
    int GetActiveLoadoutIndex() const { return m_activeLoadoutIndex; }
    const LoadoutPreset& GetLoadout(int index) const { return m_loadouts[index % 3]; }
    std::string GetActiveLoadoutNotification() const;
    bool HasRecentLoadoutChange() const { return m_notificationFramesRemaining > 0; }

    // =========================================================================
    // 2. Luck Stat Bug Fix & Soul Drop Pity Counter
    // =========================================================================
    void RecordEnemyKill(uint16_t enemyId, bool soulDropped);
    uint32_t GetEnemyDryKills(uint16_t enemyId) const;
    void ResetPityCounter(uint16_t enemyId);
    bool CheckSoulDrop(uint16_t enemyId, uint8_t baseRate, uint16_t playerLuck, uint32_t randomValue);
    bool CheckItemDrop(uint16_t enemyId, uint8_t baseRate, uint16_t playerLuck, uint32_t randomValue, bool isRare);
    float GetAdjustedDropRate(uint16_t enemyId, float baseRate) const;

    // =========================================================================
    // 3. Soul Indicators & Mini-Map
    // =========================================================================
    void SetEnemyTable(const uint8_t* table, size_t size);
    int GetEnemySoulCount(const uint8_t* ewram, size_t ewramSize, uint16_t enemyId, const uint8_t* rom = nullptr, size_t romSize = 0) const;
    bool HasEnemySoul(const uint8_t* ewram, size_t ewramSize, uint16_t enemyId, const uint8_t* rom = nullptr, size_t romSize = 0) const;
    void QueryLocalMiniMap(const uint8_t* ewram, size_t ewramSize, int& playerRoomX, int& playerRoomY, std::vector<MiniMapCell>& outCells) const;
    std::string GetEnemyName(uint16_t enemyId) const;
    void SetRecentTargetEnemy(uint16_t enemyId);
    uint16_t GetRecentTargetEnemy() const { return m_recentEnemyId; }
    bool HasRecentEnemy() const { return m_recentEnemyFramesRemaining > 0; }
    float GetNotificationRemainingRatio() const { return static_cast<float>(m_notificationFramesRemaining) / 120.0f; }

    // =========================================================================
    // 4. Fast Room Door Transitions
    // =========================================================================
    void ProcessDoorTransitions(uint8_t* ewram, size_t ewramSize);

    // =========================================================================
    // 5. Fast Text & Cutscene Fast-Forward
    // =========================================================================
    void ProcessFastText(uint8_t* ewram, size_t ewramSize);

    // Getters for feature queries
    bool IsFastTextEnabled() const { return m_config.fastText; }
    bool IsFastDoorTransitionsEnabled() const { return m_config.fastDoorTransitions; }
    bool IsCutsceneFastForwardEnabled() const { return m_config.cutsceneFastForward; }
    bool IsLuckFixEnabled() const { return m_config.fixLuckStat; }
    bool IsPitySystemEnabled() const { return m_config.farmPitySystem; }
    bool IsTransparentMiniMapEnabled() const { return m_config.transparentMiniMap; }
    bool IsEnemySoulIndicatorsEnabled() const { return m_config.enemySoulIndicators; }

private:
    QolSystem();
    ~QolSystem() = default;

    aria::config::GameplayConfig m_config;

    // Loadout Presets (3 slots)
    std::array<LoadoutPreset, 3> m_loadouts;
    int m_activeLoadoutIndex = 0;
    int m_notificationFramesRemaining = 0;
    std::string m_lastNotification;

    // Enemy Table and Targeting
    std::vector<uint8_t> m_enemyTable;
    uint16_t m_recentEnemyId = 0xFFFF;
    int m_recentEnemyFramesRemaining = 0;

    // Soul Pity System (enemyId -> dry kills)
    std::unordered_map<uint16_t, uint32_t> m_enemyKillCounts;
};

} // namespace aria::gameplay
