#include "gameplay/qol_system.hpp"
#include <algorithm>

namespace aria::gameplay {

QolSystem& QolSystem::Get() {
    static QolSystem instance;
    return instance;
}

QolSystem::QolSystem() = default;

void QolSystem::Initialize(const aria::config::GameplayConfig& config) {
    m_config = config;
}

void QolSystem::Update(uint8_t* /*ewram*/, size_t /*ewramSize*/) {
    // Per-frame QoL monitoring (e.g. fast text state, door transitions)
}

void QolSystem::RecordEnemyKill(uint16_t enemyId, bool soulDropped) {
    if (!m_config.farmPitySystem) return;

    if (soulDropped) {
        m_enemyKillCounts[enemyId] = 0;
    } else {
        m_enemyKillCounts[enemyId]++;
    }
}

float QolSystem::GetAdjustedDropRate(uint16_t enemyId, float baseRate) const {
    float adjusted = baseRate * m_config.soulDropMultiplier;
    if (!m_config.farmPitySystem) return adjusted;

    auto it = m_enemyKillCounts.find(enemyId);
    if (it != m_enemyKillCounts.end()) {
        uint32_t dryKills = it->second;
        // Every 10 dry kills adds +5% base drop chance
        adjusted += (dryKills / 10) * 0.05f;
    }

    return std::min(1.0f, adjusted);
}

void QolSystem::ResetPityCounter(uint16_t enemyId) {
    m_enemyKillCounts[enemyId] = 0;
}

} // namespace aria::gameplay
