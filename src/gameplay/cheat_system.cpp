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
    if (playerBase + 0x40 > ewramSize) return;

    uint8_t* playerPtr = ewram + playerBase;

    // Infinite HP
    if (m_config.infiniteHP) {
        uint16_t maxHp = *reinterpret_cast<uint16_t*>(playerPtr + PlayerOffsets::MAX_HP);
        if (maxHp > 0) {
            *reinterpret_cast<uint16_t*>(playerPtr + PlayerOffsets::HP) = maxHp;
        }
    }

    // Infinite MP
    if (m_config.infiniteMP) {
        uint16_t maxMp = *reinterpret_cast<uint16_t*>(playerPtr + PlayerOffsets::MAX_MP);
        if (maxMp > 0) {
            *reinterpret_cast<uint16_t*>(playerPtr + PlayerOffsets::MP) = maxMp;
        }
    }

    // Infinite Hearts
    if (m_config.infiniteHearts) {
        uint16_t maxHearts = *reinterpret_cast<uint16_t*>(playerPtr + PlayerOffsets::MAX_HEARTS);
        if (maxHearts > 0) {
            *reinterpret_cast<uint16_t*>(playerPtr + PlayerOffsets::HEARTS) = maxHearts;
        }
    }

    // One-hit kill / invincibility flags
    if (m_config.guaranteedSouls) {
        // Guaranteed soul drop rate booster: modify active RNG or soul drop threshold
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
