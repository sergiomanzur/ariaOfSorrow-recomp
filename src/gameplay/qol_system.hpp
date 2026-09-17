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

private:
    QolSystem();
    ~QolSystem() = default;

    aria::config::GameplayConfig m_config;
    std::unordered_map<uint16_t, uint32_t> m_enemyKillCounts; // Enemy ID -> consecutive kills without soul
};

} // namespace aria::gameplay
