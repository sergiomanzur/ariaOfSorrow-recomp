#include <cassert>
#include <iostream>
#include <vector>
#include "gameplay/cheat_system.hpp"
#include "gameplay/qol_system.hpp"
#include "symbols/cvaos_symbols.hpp"

using namespace aria::gameplay;
using namespace aria::symbols::cvaos_us;

int main() {
    std::cout << "[TEST] Running CheatSystem & QolSystem Unit Tests...\n";

    // Allocate simulated EWRAM buffer
    std::vector<uint8_t> ewram(aria::symbols::EWRAM_SIZE, 0);
    std::vector<uint8_t> iwram(aria::symbols::IWRAM_SIZE, 0);

    // Setup player entity data in simulated EWRAM
    uint32_t playerOffset = ADDR_PLAYER_ENTITY - aria::symbols::EWRAM_BASE;
    uint8_t* playerPtr = ewram.data() + playerOffset;

    *reinterpret_cast<uint16_t*>(playerPtr + PlayerOffsets::MAX_HP) = 500;
    *reinterpret_cast<uint16_t*>(playerPtr + PlayerOffsets::HP) = 50; // Low HP

    *reinterpret_cast<uint16_t*>(playerPtr + PlayerOffsets::MAX_MP) = 300;
    *reinterpret_cast<uint16_t*>(playerPtr + PlayerOffsets::MP) = 20; // Low MP

    *reinterpret_cast<uint16_t*>(playerPtr + PlayerOffsets::MAX_HEARTS) = 100;
    *reinterpret_cast<uint16_t*>(playerPtr + PlayerOffsets::HEARTS) = 5; // Low Hearts

    // 1. Test Cheat System
    auto& cheats = CheatSystem::Get();
    aria::config::CheatsConfig cheatCfg;
    cheatCfg.enableCheats = true;
    cheatCfg.infiniteHP = true;
    cheatCfg.infiniteMP = true;
    cheatCfg.infiniteHearts = true;
    cheats.Initialize(cheatCfg);

    cheats.ApplyFrameCheats(ewram.data(), ewram.size(), iwram.data(), iwram.size());

    assert(*reinterpret_cast<uint16_t*>(playerPtr + PlayerOffsets::HP) == 500);
    assert(*reinterpret_cast<uint16_t*>(playerPtr + PlayerOffsets::MP) == 300);
    assert(*reinterpret_cast<uint16_t*>(playerPtr + PlayerOffsets::HEARTS) == 100);

    // 2. Test QoL System (Pity Rate Booster)
    auto& qol = QolSystem::Get();
    aria::config::GameplayConfig gameCfg;
    gameCfg.farmPitySystem = true;
    gameCfg.soulDropMultiplier = 1.0f;
    qol.Initialize(gameCfg);

    uint16_t batEnemyId = 1;
    float baseRate = 0.05f; // 5% base drop chance

    // Baseline drop rate
    assert(qol.GetAdjustedDropRate(batEnemyId, baseRate) == 0.05f);

    // Record 20 dry kills without soul
    for (int i = 0; i < 20; ++i) {
        qol.RecordEnemyKill(batEnemyId, false);
    }

    // 20 dry kills -> (20/10)*0.05 = +0.10 bonus -> total 0.15 (15%)
    float boostedRate = qol.GetAdjustedDropRate(batEnemyId, baseRate);
    assert(boostedRate >= 0.149f && boostedRate <= 0.151f);

    // Record a soul drop -> resets pity counter
    qol.RecordEnemyKill(batEnemyId, true);
    assert(qol.GetAdjustedDropRate(batEnemyId, baseRate) == 0.05f);

    std::cout << "[SUCCESS] CheatSystem & QolSystem Unit Tests Passed!\n";
    return 0;
}
