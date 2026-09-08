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

void QolSystem::ApplyDialogueDarkening(uint8_t* io, size_t ioSize) const {
    // BLDCNT (0x050/0x051): bits 6-7 select the blend mode (00=none,
    // 01=alpha, 10=brighten, 11=darken). Only alpha blend represents an
    // actual translucent overlay; brighten/darken are whole-screen fades and
    // transitions, which must render exactly as authored.
    constexpr size_t kBldcnt = 0x50;
    constexpr size_t kBldalpha = 0x52;
    const int percent = m_config.dialogueDarkenPercent;
    if (percent <= 0 || !io || ioSize < kBldalpha + 2) return;

    const uint16_t bldcnt = static_cast<uint16_t>(io[kBldcnt] | (io[kBldcnt + 1] << 8));
    if (((bldcnt >> 6) & 0x3u) != 0x1u) return;  // not alpha-blend mode

    // BLDALPHA (0x052/0x053): EVA in bits 0-4 (foreground weight), EVB in
    // bits 8-12 (background/behind weight), each a 5-bit value clamped to
    // 0..16 by hardware.
    const uint16_t bldalpha = static_cast<uint16_t>(io[kBldalpha] | (io[kBldalpha + 1] << 8));
    int eva = bldalpha & 0x1F;
    int evb = (bldalpha >> 8) & 0x1F;
    if (eva == 0 && evb == 0) return;  // degenerate, nothing actually blended

    // Shift weight from the backdrop toward the foreground box, by `percent`
    // of the backdrop's current contribution. At 100% the overlay becomes
    // fully opaque; at 0% (never reached here) it would be untouched.
    int shift = (evb * percent) / 100;
    if (shift < 1 && evb > 0) shift = 1;
    evb -= shift;
    eva += shift;
    if (eva > 16) eva = 16;
    if (evb < 0) evb = 0;

    const uint16_t newValue = static_cast<uint16_t>((eva & 0x1F) | ((evb & 0x1F) << 8));
    io[kBldalpha] = static_cast<uint8_t>(newValue & 0xFF);
    io[kBldalpha + 1] = static_cast<uint8_t>(newValue >> 8);
}

} // namespace aria::gameplay
