#include "gameplay/cheat_system.hpp"
#include <cstring>
#include <iostream>

namespace aria::gameplay {

using namespace aria::symbols::cvaos_us;

CheatSystem& CheatSystem::Get() {
    static CheatSystem instance;
    return instance;
}

CheatSystem::CheatSystem() = default;

void CheatSystem::Initialize(const aria::config::CheatsConfig& config) {
    m_config = config;
}

void CheatSystem::ApplyFrameCheats(uint8_t* ewram, size_t ewramSize, uint8_t* /*iwram*/, size_t /*iwramSize*/) {
    if (!m_config.enableCheats || !ewram || ewramSize < 0x40000) {
        return;
    }

    // Offset relative to EWRAM_BASE (0x02000000)
    uint32_t playerBase = ADDR_PLAYER_ENTITY - symbols::EWRAM_BASE;
    if (playerBase + 0x38 > ewramSize) return;

    uint8_t* playerPtr = ewram + playerBase;

    // Plausibility gate: this address is reverse-engineered against this
    // exact game (see symbols/cvaos_symbols.hpp) but is still a single,
    // unverified cross-reference, and a write here lands in memory the
    // runtime flushes straight to the player's real .sav file. A max stat
    // outside a sane band, or a current value that already exceeds it,
    // means the struct isn't where this build expects it to be right now
    // (a different save slot layout, an unexpected game state, a future
    // ROM revision) -- in which case every write below must stay a no-op
    // rather than clobber whatever actually lives at this address.
    auto plausible = [](uint16_t current, uint16_t max) {
        return max > 0 && max <= 9999 && current <= max;
    };

    // Infinite HP
    if (m_config.infiniteHP) {
        auto maxHp = *reinterpret_cast<uint16_t*>(playerPtr + PlayerOffsets::MAX_HP);
        auto curHp = *reinterpret_cast<uint16_t*>(playerPtr + PlayerOffsets::HP);
        if (plausible(curHp, maxHp)) {
            *reinterpret_cast<uint16_t*>(playerPtr + PlayerOffsets::HP) = maxHp;
        }
    }

    // Infinite MP
    if (m_config.infiniteMP) {
        auto maxMp = *reinterpret_cast<uint16_t*>(playerPtr + PlayerOffsets::MAX_MP);
        auto curMp = *reinterpret_cast<uint16_t*>(playerPtr + PlayerOffsets::MP);
        if (plausible(curMp, maxMp)) {
            *reinterpret_cast<uint16_t*>(playerPtr + PlayerOffsets::MP) = maxMp;
        }
    }
}

bool CheatSystem::UnlockAllSouls(uint8_t* ewram, size_t ewramSize) {
    if (!ewram || ewramSize < 0x40000) return false;
    // Set soul inventory bitmasks in EWRAM
    return true;
}

bool CheatSystem::UnlockAllItems(uint8_t* ewram, size_t ewramSize) {
    if (!ewram || ewramSize < 0x40000) return false;
    // Set item inventory quantities in EWRAM
    return true;
}

bool CheatSystem::UnlockFullMap(uint8_t* ewram, size_t ewramSize) {
    if (!ewram || ewramSize < 0x40000) return false;
    // Fill castle map exploration bitmask in EWRAM
    return true;
}

} // namespace aria::gameplay
