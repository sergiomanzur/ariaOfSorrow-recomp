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

    // Invincibility: flag 0x2000 at EWRAM 0x13260 prevents all player damage
    if (m_config.invincibility) {
        if (0x13260 + sizeof(uint32_t) <= ewramSize) {
            auto* flags = reinterpret_cast<uint32_t*>(ewram + 0x13260);
            *flags |= 0x2000;
        }
    }

    // One-Hit Kill: clamps all active enemy HP to 1 so the next hit defeats them
    if (m_config.oneHitKill) {
        constexpr uint32_t kEntityArrayBase = 0x004E4;
        constexpr size_t kEntitySize = 0x84;
        constexpr size_t kEntityCount = 0xE0;
        for (size_t i = 1; i < kEntityCount; ++i) {
            uint32_t offset = kEntityArrayBase + i * kEntitySize;
            if (offset + kEntitySize > ewramSize) break;
            uint8_t* entity = ewram + offset;
            uint32_t updateFunc = *reinterpret_cast<uint32_t*>(entity + 0x00);
            if (updateFunc == 0) continue; // Inactive entity
            uint8_t enemyId = entity[0x36];
            if (enemyId >= 1 && enemyId <= 120) {
                uint16_t* enemyHp = reinterpret_cast<uint16_t*>(entity + 0x2E);
                if (*enemyHp > 1) {
                    *enemyHp = 1;
                }
            }
        }
    }
}

} // namespace aria::gameplay
