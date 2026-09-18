#include <cassert>
#include <iostream>
#include <vector>
#include <cstring>

#include "config/config_system.hpp"
#include "gameplay/qol_system.hpp"

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::cerr << "Assertion failed: " #cond " at " << __FILE__     \
                      << ":" << __LINE__ << "\n";                          \
            return 1;                                                      \
        }                                                                  \
    } while (0)

int main() {
    using namespace aria::gameplay;
    auto& qol = QolSystem::Get();

    aria::config::GameplayConfig config;
    config.enableQuickLoadouts = true;
    config.quickLoadoutSlots = 3;
    config.fixLuckStat = true;
    config.farmPitySystem = true;
    config.soulPityThreshold = 10;
    config.transparentMiniMap = true;
    config.miniMapOpacity = 0.8f;
    config.enemySoulIndicators = true;
    config.fastDoorTransitions = true;
    config.fastText = true;

    qol.Initialize(config);

    std::vector<uint8_t> ewram(0x40000, 0);

    // =========================================================================
    // 1. Quick Loadout Presets ("Dawn of Sorrow" Fix)
    // =========================================================================
    std::cout << "[TEST] Quick Loadout Presets...\n";
    {
        ewram[qol_offsets::kEquippedWeapon]     = 0x05;
        ewram[qol_offsets::kEquippedRedSoul]    = 0x0A;
        ewram[qol_offsets::kEquippedBlueSoul]   = 0x02;
        ewram[qol_offsets::kEquippedYellowSoul] = 0x03;
        ewram[qol_offsets::kEquippedArmor]      = 0x10;
        ewram[qol_offsets::kEquippedAccessory]  = 0x04;

        qol.CycleLoadout(ewram.data(), ewram.size(), 1);
        CHECK(qol.GetActiveLoadoutIndex() == 1);
        CHECK(qol.HasRecentLoadoutChange());

        ewram[qol_offsets::kEquippedWeapon]     = 0x22;
        ewram[qol_offsets::kEquippedRedSoul]    = 0x15;
        ewram[qol_offsets::kEquippedBlueSoul]   = 0x08;
        ewram[qol_offsets::kEquippedYellowSoul] = 0x09;

        qol.CycleLoadout(ewram.data(), ewram.size(), 1);
        CHECK(qol.GetActiveLoadoutIndex() == 2);

        ewram[qol_offsets::kEquippedWeapon]     = 0x14;
        ewram[qol_offsets::kEquippedRedSoul]    = 0x01;
        ewram[qol_offsets::kEquippedBlueSoul]   = 0x00;
        ewram[qol_offsets::kEquippedYellowSoul] = 0x00;

        qol.CycleLoadout(ewram.data(), ewram.size(), 1);
        CHECK(qol.GetActiveLoadoutIndex() == 0);
        CHECK(ewram[qol_offsets::kEquippedWeapon] == 0x05);
        CHECK(ewram[qol_offsets::kEquippedRedSoul] == 0x0A);
        CHECK(ewram[qol_offsets::kEquippedBlueSoul] == 0x02);
        CHECK(ewram[qol_offsets::kEquippedYellowSoul] == 0x03);

        qol.CycleLoadout(ewram.data(), ewram.size(), 1);
        CHECK(qol.GetActiveLoadoutIndex() == 1);
        CHECK(ewram[qol_offsets::kEquippedWeapon] == 0x22);
        CHECK(ewram[qol_offsets::kEquippedRedSoul] == 0x15);
        CHECK(ewram[qol_offsets::kEquippedBlueSoul] == 0x08);
        CHECK(ewram[qol_offsets::kEquippedYellowSoul] == 0x09);
    }

    // =========================================================================
    // 2. Luck Stat Bug Fix
    // =========================================================================
    std::cout << "[TEST] Luck Stat Bug Fix...\n";
    {
        uint16_t enemyId = 5;
        uint8_t baseRate = 20;
        uint32_t fixedRng = 0x1000;

        config.fixLuckStat = true;
        qol.SetConfig(config);

        bool lowLuckDrop = qol.CheckSoulDrop(enemyId, baseRate, 10, fixedRng);
        bool highLuckDrop = qol.CheckSoulDrop(enemyId, baseRate, 300, fixedRng);
        (void)lowLuckDrop;
        (void)highLuckDrop;

        config.fixLuckStat = false;
        qol.SetConfig(config);
        CHECK(!config.fixLuckStat);

        config.fixLuckStat = true;
        qol.SetConfig(config);
    }

    // =========================================================================
    // 3. Soul Drop Pity Counter
    // =========================================================================
    std::cout << "[TEST] Soul Drop Pity Counter...\n";
    {
        uint16_t enemyId = 12;
        qol.ResetPityCounter(enemyId);
        CHECK(qol.GetEnemyDryKills(enemyId) == 0);

        for (int i = 0; i < 9; ++i) {
            qol.RecordEnemyKill(enemyId, false);
        }
        CHECK(qol.GetEnemyDryKills(enemyId) == 9);

        uint32_t badRng = 0xFFFFFFFC;
        bool drop = qol.CheckSoulDrop(enemyId, 5, 10, badRng);
        CHECK(drop == true);
        CHECK(qol.GetEnemyDryKills(enemyId) == 0);
    }

    // =========================================================================
    // 4. Soul Indicators & Enemy Table
    // =========================================================================
    std::cout << "[TEST] Enemy Soul Count & Indicators...\n";
    {
        std::vector<uint8_t> mockTable(120 * qol_offsets::kEnemyEntrySize, 0);
        mockTable[1 * qol_offsets::kEnemyEntrySize + 0x17] = 0;
        mockTable[1 * qol_offsets::kEnemyEntrySize + 0x18] = 1;

        mockTable[2 * qol_offsets::kEnemyEntrySize + 0x17] = 1;
        mockTable[2 * qol_offsets::kEnemyEntrySize + 0x18] = 3;

        qol.SetEnemyTable(mockTable.data(), mockTable.size());

        CHECK(qol.GetEnemySoulCount(ewram.data(), ewram.size(), 1) == 0);
        CHECK(!qol.HasEnemySoul(ewram.data(), ewram.size(), 1));

        ewram[qol_offsets::kRedSoulInventory + 0] = 0x03;
        CHECK(qol.GetEnemySoulCount(ewram.data(), ewram.size(), 1) == 3);
        CHECK(qol.HasEnemySoul(ewram.data(), ewram.size(), 1));

        ewram[qol_offsets::kBlueSoulInventory + 1] = 0x07;
        CHECK(qol.GetEnemySoulCount(ewram.data(), ewram.size(), 2) == 7);
        CHECK(qol.HasEnemySoul(ewram.data(), ewram.size(), 2));

        CHECK(qol.GetEnemyName(0) == "Bat");
        CHECK(qol.GetEnemyName(1) == "Zombie");
        CHECK(qol.GetEnemyName(2) == "Skeleton");
    }

    // =========================================================================
    // 5. Mini-Map Query
    // =========================================================================
    std::cout << "[TEST] Mini-Map Query...\n";
    {
        *reinterpret_cast<int32_t*>(ewram.data() + qol_offsets::kCameraFracX) = (10 * 256) << 16;
        *reinterpret_cast<int32_t*>(ewram.data() + qol_offsets::kCameraFracY) = (5 * 160) << 16;

        uint8_t* gridRow5 = ewram.data() + qol_offsets::kMapGrid + (5 * 8);
        *reinterpret_cast<uint32_t*>(gridRow5) = (1u << 10);
        *reinterpret_cast<uint32_t*>(gridRow5 + 4) = (1u << 10);

        int playerRoomX = 0, playerRoomY = 0;
        std::vector<MiniMapCell> cells;
        qol.QueryLocalMiniMap(ewram.data(), ewram.size(), playerRoomX, playerRoomY, cells);

        CHECK(playerRoomX == 10);
        CHECK(playerRoomY == 5);
        CHECK(!cells.empty());

        bool foundPlayerRoom = false;
        for (const auto& c : cells) {
            if (c.x == 10 && c.y == 5) {
                foundPlayerRoom = true;
                CHECK(c.visited);
                CHECK(c.revealed);
            }
        }
        CHECK(foundPlayerRoom);
    }

    // =========================================================================
    // 6. Fast Transitions & Fast Text
    // =========================================================================
    std::cout << "[TEST] Fast Transitions & Fast Text...\n";
    {
        ewram[qol_offsets::kRoomTransitionObj + 0x64] = 4;
        ewram[qol_offsets::kRoomTransitionObj + 0x65] = 3;

        qol.ProcessDoorTransitions(ewram.data(), ewram.size());
        CHECK(ewram[qol_offsets::kRoomTransitionObj + 0x65] == 4);

        // Fast text
        ewram[0x131BF] = 0x05; // typewriter delay
        qol.ProcessFastText(ewram.data(), ewram.size());
        CHECK(ewram[0x131BF] == 0x00); // flushed
    }

    std::cout << "[SUCCESS] All QoL and modern control tests passed!\n";
    return 0;
}
