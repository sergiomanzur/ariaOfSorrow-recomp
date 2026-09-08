#pragma once

#include <cstdint>
#include <unordered_map>
#include "config/config_system.hpp"

namespace aria::gameplay {

class QolSystem {
public:
    static QolSystem& Get();

    void Initialize(const aria::config::GameplayConfig& config);
    void SetConfig(const aria::config::GameplayConfig& config) { m_config = config; }
    const aria::config::GameplayConfig& GetConfig() const { return m_config; }

    // Applied every game frame
    void Update(uint8_t* ewram, size_t ewramSize);

    // Soul Pity System
    void RecordEnemyKill(uint16_t enemyId, bool soulDropped);
    float GetAdjustedDropRate(uint16_t enemyId, float baseRate) const;
    void ResetPityCounter(uint16_t enemyId);

    // Fast text & transitions
    bool IsFastTextEnabled() const { return m_config.fastText; }
    bool IsFastDoorTransitionsEnabled() const { return m_config.fastDoorTransitions; }
    bool IsBossQuickRetryEnabled() const { return m_config.bossQuickRetry; }
    bool IsDeathQuickRetryEnabled() const { return m_config.deathQuickRetry; }

    // Darkens semi-transparent overlays (dialogue boxes, etc.) by biasing the
    // GBA's own alpha-blend weights toward the foreground layer, whenever
    // BLDCNT reports alpha-blend mode. Screen fades/transitions use the
    // darken/brighten modes instead, which this leaves untouched. `io` is
    // the live IO register page (0x04000000-based); `ioSize` its byte size.
    void ApplyDialogueDarkening(uint8_t* io, size_t ioSize) const;

private:
    QolSystem();
    ~QolSystem() = default;

    aria::config::GameplayConfig m_config;
    std::unordered_map<uint16_t, uint32_t> m_enemyKillCounts; // Enemy ID -> consecutive kills without soul
};

} // namespace aria::gameplay
