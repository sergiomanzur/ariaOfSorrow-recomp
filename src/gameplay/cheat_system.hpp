#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include "config/config_system.hpp"
#include "symbols/cvaos_symbols.hpp"

namespace aria::gameplay {

class CheatSystem {
public:
    static CheatSystem& Get();

    void Initialize(const aria::config::CheatsConfig& config);
    void SetConfig(const aria::config::CheatsConfig& config) { m_config = config; }
    const aria::config::CheatsConfig& GetConfig() const { return m_config; }

    // Applied every game frame boundary on EWRAM/IWRAM pointers
    void ApplyFrameCheats(uint8_t* ewram, size_t ewramSize, uint8_t* iwram, size_t iwramSize);

    // Individual Cheat Toggles
    void ToggleInfiniteHP(bool enable)     { m_config.infiniteHP = enable; }
    void ToggleInfiniteMP(bool enable)     { m_config.infiniteMP = enable; }
    void ToggleInfiniteHearts(bool enable) { m_config.infiniteHearts = enable; }
    void ToggleInvincibility(bool enable)  { m_config.invincibility = enable; }
    void ToggleOneHitKill(bool enable)     { m_config.oneHitKill = enable; }
    void SetExpMultiplier(float mult)      { m_config.expMultiplier = mult; }
    void ToggleGuaranteedSouls(bool enable){ m_config.guaranteedSouls = enable; }

    // One-shot Progression Cheats
    bool UnlockAllSouls(uint8_t* ewram, size_t ewramSize);
    bool UnlockAllItems(uint8_t* ewram, size_t ewramSize);
    bool UnlockFullMap(uint8_t* ewram, size_t ewramSize);

private:
    CheatSystem();
    ~CheatSystem() = default;

    aria::config::CheatsConfig m_config;
};

} // namespace aria::gameplay
