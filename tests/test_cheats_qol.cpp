#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "gameplay/cheat_system.hpp"
#include "gameplay/grant_system.hpp"
#include "gameplay/qol_system.hpp"
#include "symbols/cvaos_symbols.hpp"

using namespace aria::gameplay;
using namespace aria::symbols::cvaos_us;

// This project configures CMAKE_BUILD_TYPE=Release, whose default flags
// include -DNDEBUG -- which makes <cassert>'s assert() expand to nothing.
// Every check in this file used to be an assert(), so the binary printed
// "[SUCCESS]" without evaluating a single one. CHECK() is unconditional.
#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__          \
                      << ": " #cond "\n";                                  \
            std::exit(1);                                                  \
        }                                                                  \
    } while (0)

namespace {

using namespace aria::gameplay::grant_offsets;

constexpr uint32_t kPlayerBase =
    ADDR_PLAYER_ENTITY - aria::symbols::EWRAM_BASE;

// A frame the grant system is allowed to write on: see the plausibility gate
// in grant_system.cpp (mirroring cheat_system.cpp:40-42).
void MakePlausible(std::vector<uint8_t>& ewram) {
    uint8_t* p = ewram.data() + kPlayerBase;
    *reinterpret_cast<uint16_t*>(p + PlayerOffsets::MAX_HP) = 500;
    *reinterpret_cast<uint16_t*>(p + PlayerOffsets::HP) = 320;
}

std::vector<uint8_t> FreshEwram() {
    std::vector<uint8_t> ewram(aria::symbols::EWRAM_SIZE, 0);
    MakePlausible(ewram);
    return ewram;
}

uint16_t SoulTotal(const std::vector<uint8_t>& ewram) {
    uint16_t v = 0;
    std::memcpy(&v, ewram.data() + kTotalSoulsCollected, sizeof(v));
    return v;
}

uint32_t MapElement(uint32_t x, uint32_t y) {
    return kMapGrid + ((x >> 5) * kMapGridRows + y) * kMapGridElementSize;
}

uint32_t ReadU32(const std::vector<uint8_t>& ewram, uint32_t offset) {
    uint32_t v = 0;
    std::memcpy(&v, ewram.data() + offset, sizeof(v));
    return v;
}

uint16_t ReadU16At(const std::vector<uint8_t>& ewram, uint32_t offset) {
    uint16_t v = 0;
    std::memcpy(&v, ewram.data() + offset, sizeof(v));
    return v;
}

void WriteU32At(std::vector<uint8_t>& ewram, uint32_t offset, uint32_t v) {
    std::memcpy(ewram.data() + offset, &v, sizeof(v));
}

// Independent transcription of sub_08033CAC_inline_0
// (third_party/cvaos/src/code_08033CAC.c:31-36), used only to compute the
// value test 4.6 expects -- not the code under test.
uint32_t ExpectedExpThreshold(int level) {
    const int64_t x = level;
    return static_cast<uint32_t>(x * (x + 1) * (3 * x + 8));
}

// Every byte in [begin, end) equals `value`.
bool AllEqual(const std::vector<uint8_t>& ewram, uint32_t begin, uint32_t end,
              uint8_t value) {
    for (uint32_t i = begin; i < end; ++i) {
        if (ewram[i] != value) return false;
    }
    return true;
}

// How many ".sav.bak-*" files the grant system has left in `dir`.
int CountBackups(const std::filesystem::path& dir) {
    int n = 0;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (entry.path().filename().string().find(".bak-") != std::string::npos) {
            ++n;
        }
    }
    return n;
}

// One grant, applied on one plausible frame.
void ApplyOne(GrantSystem& grants, std::vector<uint8_t>& ewram, GrantKind kind) {
    grants.Request(kind);
    grants.ApplyPending(ewram.data(), ewram.size());
}

// A soul range granted once is all-1 nibbles: every full byte 0x11, plus a
// trailing 0x01 when the count is odd (the high nibble of the last byte
// belongs to no soul).
void CheckSoulRangeGrantedOnce(const std::vector<uint8_t>& ewram, uint32_t base,
                               int count) {
    const uint32_t bytes = static_cast<uint32_t>((count + 1) / 2);
    for (uint32_t i = 0; i < bytes; ++i) {
        const bool lastAndOdd = (count & 1) && (i == bytes - 1);
        CHECK(ewram[base + i] == (lastAndOdd ? 0x01 : 0x11));
    }
    // The byte before the range is never touched.
    CHECK(ewram[base - 1] == 0);
}

} // namespace

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

    // 1. Test Cheat System
    auto& cheats = CheatSystem::Get();
    aria::config::CheatsConfig cheatCfg;
    cheatCfg.enableCheats = true;
    cheatCfg.infiniteHP = true;
    cheatCfg.infiniteMP = true;
    cheats.Initialize(cheatCfg);

    cheats.ApplyFrameCheats(ewram.data(), ewram.size(), iwram.data(), iwram.size());

    CHECK(*reinterpret_cast<uint16_t*>(playerPtr + PlayerOffsets::HP) == 500);
    CHECK(*reinterpret_cast<uint16_t*>(playerPtr + PlayerOffsets::MP) == 300);

    // The plausibility gate must reject an implausible max (a wrong address
    // must stay inert, never destructive -- see cheat_system.cpp).
    *reinterpret_cast<uint16_t*>(playerPtr + PlayerOffsets::MAX_HP) = 20000; // implausible
    *reinterpret_cast<uint16_t*>(playerPtr + PlayerOffsets::HP) = 42;
    cheats.ApplyFrameCheats(ewram.data(), ewram.size(), iwram.data(), iwram.size());
    CHECK(*reinterpret_cast<uint16_t*>(playerPtr + PlayerOffsets::HP) == 42);

    // 2. Test QoL System (Pity Rate Booster)
    auto& qol = QolSystem::Get();
    aria::config::GameplayConfig gameCfg;
    gameCfg.farmPitySystem = true;
    gameCfg.soulDropMultiplier = 1.0f;
    qol.Initialize(gameCfg);

    uint16_t batEnemyId = 1;
    float baseRate = 0.05f; // 5% base drop chance

    // Baseline drop rate
    CHECK(qol.GetAdjustedDropRate(batEnemyId, baseRate) == 0.05f);

    // Record 20 dry kills without soul
    for (int i = 0; i < 20; ++i) {
        qol.RecordEnemyKill(batEnemyId, false);
    }

    // 20 dry kills -> (20/10)*0.05 = +0.10 bonus -> total 0.15 (15%)
    float boostedRate = qol.GetAdjustedDropRate(batEnemyId, baseRate);
    CHECK(boostedRate >= 0.149f && boostedRate <= 0.151f);

    // Record a soul drop -> resets pity counter
    qol.RecordEnemyKill(batEnemyId, true);
    CHECK(qol.GetAdjustedDropRate(batEnemyId, baseRate) == 0.05f);

    // ------------------------------------------------------------------
    // 3. GrantSystem
    // ------------------------------------------------------------------

    // 3.1 Each inventory grant writes exactly its documented range, and the
    //     bytes on either side of it stay untouched.
    {
        // Consumables: itemInventory[0x20] at 0x13294.
        auto mem = FreshEwram();
        GrantSystem g;
        ApplyOne(g, mem, GrantKind::Consumables);
        CHECK(AllEqual(mem, kItemInventory, kItemInventory + kItemCount, 1));
        CHECK(mem[kItemInventory - 1] == 0);
        CHECK(mem[kItemInventory + kItemCount] == 0); // weaponInventory[0]
        // Only the ability-soul grant may touch the ability-soul enabled
        // bitfield; a bit set here with a zero soul nibble would hand the
        // player an unclearable traversal move.
        CHECK(mem[kAbilitySoulEnabledFlags] == 0);
        CHECK(mem[kAbilitySoulEnabledFlags + 1] == 0);
    }
    {
        // Weapons: weaponInventory[0x3B] at 0x132B4.
        auto mem = FreshEwram();
        GrantSystem g;
        ApplyOne(g, mem, GrantKind::Weapons);
        CHECK(AllEqual(mem, kWeaponInventory, kWeaponInventory + kWeaponCount, 1));
        CHECK(mem[kWeaponInventory - 1] == 0);
        CHECK(mem[kWeaponInventory + kWeaponCount] == 0); // armorInventory[0]
        CHECK(mem[kAbilitySoulEnabledFlags] == 0);
        CHECK(mem[kAbilitySoulEnabledFlags + 1] == 0);
    }
    {
        // Armor + accessories: 45 bytes from 0x132EF, deliberately running
        // out of armorInventory[0x19] straight into accessoryInventory
        // (0x132EF + 0x19 == 0x13308) exactly as the game's own give-all
        // does (third_party/cvaos/src/code_08038A38.c:446-460).
        auto mem = FreshEwram();
        GrantSystem g;
        ApplyOne(g, mem, GrantKind::ArmorAndAccessories);
        CHECK(kArmorInventory + 0x19 == kAccessoryInventory);
        CHECK(AllEqual(mem, kArmorInventory, kArmorInventory + kArmorCount, 1));
        CHECK(mem[kArmorInventory - 1] == 0);
        // The byte after is redSoulInventory[0][0] == 0x1331C.
        CHECK(kArmorInventory + kArmorCount == kRedSoulInventory);
        CHECK(mem[kRedSoulInventory] == 0);
        CHECK(mem[kAbilitySoulEnabledFlags] == 0);
        CHECK(mem[kAbilitySoulEnabledFlags + 1] == 0);
    }
    {
        // Bullet (red) souls: 55 nibbles = 28 bytes from 0x1331C.
        auto mem = FreshEwram();
        GrantSystem g;
        ApplyOne(g, mem, GrantKind::BulletSouls);
        CheckSoulRangeGrantedOnce(mem, kRedSoulInventory, kRedSoulCount);
        // Group 1 of redSoulInventory[2][0x1C] is never written.
        CHECK(mem[kRedSoulInventory + 0x1C] == 0);
        CHECK(SoulTotal(mem) == kRedSoulCount);
        CHECK(mem[kAbilitySoulEnabledFlags] == 0);
        CHECK(mem[kAbilitySoulEnabledFlags + 1] == 0);
    }
    {
        // Guardian (blue) souls: 24 nibbles = 12 bytes from 0x13354.
        auto mem = FreshEwram();
        GrantSystem g;
        ApplyOne(g, mem, GrantKind::GuardianSouls);
        CheckSoulRangeGrantedOnce(mem, kBlueSoulInventory, kBlueSoulCount);
        CHECK(mem[kBlueSoulInventory + (kBlueSoulCount + 1) / 2] == 0);
        CHECK(SoulTotal(mem) == kBlueSoulCount);
        CHECK(mem[kAbilitySoulEnabledFlags] == 0);
        CHECK(mem[kAbilitySoulEnabledFlags + 1] == 0);
    }
    {
        // Enchant (yellow) souls: 35 nibbles = 18 bytes from 0x1336E.
        auto mem = FreshEwram();
        GrantSystem g;
        ApplyOne(g, mem, GrantKind::EnchantSouls);
        CheckSoulRangeGrantedOnce(mem, kYellowSoulInventory, kYellowSoulCount);
        CHECK(mem[kYellowSoulInventory + (kYellowSoulCount + 1) / 2] == 0);
        CHECK(SoulTotal(mem) == kYellowSoulCount);
        CHECK(mem[kAbilitySoulEnabledFlags] == 0);
        CHECK(mem[kAbilitySoulEnabledFlags + 1] == 0);
    }

    // 3.2 Nibble packing: soul 0 is the low nibble of the first byte, soul 1
    //     the high nibble of that same byte.
    {
        auto mem = FreshEwram();
        GrantSystem g;
        ApplyOne(g, mem, GrantKind::BulletSouls);
        CHECK((mem[kRedSoulInventory] & 0x0F) == 1);        // soul index 0
        CHECK(((mem[kRedSoulInventory] >> 4) & 0x0F) == 1); // soul index 1
        // Those two alone cannot fail: with both nibbles at 1 the byte is
        // 0x11, which a transposed odd/even branch would produce as well. The
        // binding case is the asymmetric one -- 55 is odd, so the last byte's
        // 28th slot (soul index 54, even) must land in the LOW nibble and
        // leave the high nibble clear. Transposition makes this 0x10.
        const uint32_t last = kRedSoulInventory + (kRedSoulCount + 1) / 2 - 1;
        CHECK((kRedSoulCount & 1) == 1);
        CHECK(mem[last] == 0x01);
    }

    // 3.3 Repeating a grant clamps each soul at 9 and caps the running total
    //     at 999 (third_party/cvaos/src/code_08032444.c:124-141).
    {
        auto mem = FreshEwram();
        GrantSystem g;
        for (int repeat = 0; repeat < 20; ++repeat) {
            ApplyOne(g, mem, GrantKind::BulletSouls);
        }
        for (int i = 0; i < (kRedSoulCount + 1) / 2 - 1; ++i) {
            CHECK(mem[kRedSoulInventory + i] == 0x99);
        }
        // 55 is odd, so the last byte's high nibble belongs to no soul.
        CHECK(mem[kRedSoulInventory + (kRedSoulCount + 1) / 2 - 1] == 0x09);
        CHECK(SoulTotal(mem) == 999); // 20 * 55 = 1100, capped
    }

    // 3.4 Ability souls: 8 nibbles across 4 bytes, 0x13392..0x13395 -- the
    //     fourth byte being pad_13395, which the game's own loop writes too
    //     (ewram.h:748-749, code_08038A38.c:435-441 with count 0x8).
    {
        auto mem = FreshEwram();
        GrantSystem g;
        ApplyOne(g, mem, GrantKind::AbilitySouls);
        CHECK(kAbilitySoulCount == 8);
        for (uint32_t i = 0; i < 4; ++i) {
            CHECK(mem[kAbilitySoulInventory + i] == 0x11);
        }
        CHECK(mem[kAbilitySoulInventory - 1] == 0);
        // The byte right after the 4 written bytes is the ability-soul enabled
        // bitfield, which this grant -- and only this grant -- sets on
        // purpose: bits 0..5, a literal transcription of the give-all's
        // sub_08032ADC(0..5, 1) at code_08038A38.c:465-470. Bits 6 and 7 stay
        // clear because the game never sets them there either.
        CHECK(kAbilitySoulInventory + 4 == kAbilitySoulEnabledFlags);
        CHECK(mem[kAbilitySoulEnabledFlags] == 0x3F);
        CHECK(mem[kAbilitySoulEnabledFlags + 1] == 0);
        CHECK(SoulTotal(mem) == kAbilitySoulCount);
    }

    // 3.5 Map reveal: both planes set for occupied cells only, and no element
    //     touched for rows >= 35 (the game scans y < 0x23,
    //     third_party/cvaos/src/code_080109F4.c:398).
    {
        std::vector<uint16_t> table(kRoomTableCells, kNoRoom);
        struct Cell { uint32_t x, y; };
        const Cell occupied[] = {{0, 0}, {5, 3}, {31, 3}, {33, 7}, {63, 34}};
        for (const auto& c : occupied) {
            table[c.x + (static_cast<size_t>(c.y) << 6)] = 0x0000;
        }
        // A cell in a row the game never scans: it must be ignored even
        // though the grid has 40 rows allocated.
        table[10 + (static_cast<size_t>(36) << 6)] = 0x0000;

        auto mem = FreshEwram();
        GrantSystem g;
        g.SetRoomTable(table.data(), table.size());
        ApplyOne(g, mem, GrantKind::RevealMap);

        for (const auto& c : occupied) {
            const uint32_t element = MapElement(c.x, c.y);
            const uint32_t bit = 1u << (c.x & 31);
            CHECK((ReadU32(mem, element) & bit) != 0);      // visited plane
            CHECK((ReadU32(mem, element + 4) & bit) != 0);  // revealed plane
        }
        // Exactly the requested bits, nothing more: plane words hold only the
        // bits for the occupied cells in their own half-row.
        CHECK(ReadU32(mem, MapElement(0, 0)) == (1u << 0));
        CHECK(ReadU32(mem, MapElement(5, 3)) == ((1u << 5) | (1u << 31)));
        CHECK(ReadU32(mem, MapElement(33, 7)) == (1u << 1));
        CHECK(ReadU32(mem, MapElement(63, 34)) == (1u << 31));
        // An unoccupied row is left entirely alone.
        CHECK(ReadU32(mem, MapElement(0, 1)) == 0);
        CHECK(ReadU32(mem, MapElement(0, 1) + 4) == 0);
        // Rows 35..39 of both halves are untouched.
        for (uint32_t y = 35; y < 40; ++y) {
            for (uint32_t half = 0; half < 2; ++half) {
                const uint32_t element = MapElement(half * 32, y);
                CHECK(ReadU32(mem, element) == 0);
                CHECK(ReadU32(mem, element + 4) == 0);
            }
        }
    }

    // 3.6 The plausibility gate blocks every write, and keeps the request
    //     queued for a later frame rather than dropping it.
    {
        std::vector<uint8_t> mem(aria::symbols::EWRAM_SIZE, 0);
        uint8_t* p = mem.data() + kPlayerBase;
        *reinterpret_cast<uint16_t*>(p + PlayerOffsets::MAX_HP) = 20000;
        *reinterpret_cast<uint16_t*>(p + PlayerOffsets::HP) = 42;

        std::vector<uint16_t> table(kRoomTableCells, 0x0000);
        GrantSystem g;
        g.SetRoomTable(table.data(), table.size());
        for (int k = 0; k < static_cast<int>(GrantKind::Count); ++k) {
            g.Request(static_cast<GrantKind>(k));
        }
        g.ApplyPending(mem.data(), mem.size());

        CHECK(g.HasPending());
        CHECK(AllEqual(mem, kMapGrid, kMapGrid + 2 * kMapGridRows * kMapGridElementSize, 0));
        CHECK(AllEqual(mem, kTotalSoulsCollected, kTotalSoulsCollected + 2, 0));
        CHECK(AllEqual(mem, kItemInventory, kAbilitySoulEnabledFlags + 2, 0));

        // ...and once the block looks sane, the same queued request applies.
        MakePlausible(mem);
        g.ApplyPending(mem.data(), mem.size());
        CHECK(!g.HasPending());
        CHECK(mem[kItemInventory] == 1);
        CHECK(mem[kRedSoulInventory] == 0x11);
        CHECK(ReadU32(mem, MapElement(0, 0)) == 0xFFFFFFFFu);
    }

    // 3.7 A save file that does not exist yet must not consume the process's
    //     one backup attempt: on a first boot the .sav appears only after the
    //     game writes SRAM, and the player who grants now, saves in-game, then
    //     grants again must still get a backup that second time.
    {
        const std::filesystem::path dir =
            std::filesystem::temp_directory_path() / "aria_grant_backup_test";
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
        std::filesystem::create_directories(dir, ec);
        const std::filesystem::path save = dir / "aria.sav";

        GrantSystem g;
        g.SetSavePath(save.string());

        // First grant, no save file on disk yet -> nothing to copy.
        auto mem = FreshEwram();
        ApplyOne(g, mem, GrantKind::Consumables);
        CHECK(mem[kItemInventory] == 1);          // the grant still applied
        CHECK(CountBackups(dir) == 0);

        // The game now creates the save, and a second grant follows.
        {
            std::ofstream out(save, std::ios::binary);
            out << "save";
        }
        ApplyOne(g, mem, GrantKind::Weapons);
        CHECK(mem[kWeaponInventory] == 1);
        CHECK(CountBackups(dir) == 1);            // the attempt was still available

        // ...and it really is only one attempt: a third grant adds no more.
        ApplyOne(g, mem, GrantKind::BulletSouls);
        CHECK(mem[kRedSoulInventory] == 0x11);
        CHECK(CountBackups(dir) == 1);

        std::filesystem::remove_all(dir, ec);
    }

    // ------------------------------------------------------------------
    // 4. Level-up cheats
    // ------------------------------------------------------------------
    //
    // Synthetic growth tables, per the task brief -- GrantSystem never bakes
    // real table values in, they arrive at runtime via SetGrowthTables, so
    // any distinguishable values exercise the same code paths. Each stat's
    // table holds a different value at every index (str[i]=i+1, con[i]=i+2,
    // int[i]=i+3, mp[i]=i+4) so a swapped table shows up immediately, and
    // idx 0 != idx 1 so a bug that hoists the growth-table index out of the
    // level-up loop -- reusing the level *before* any of a multi-level
    // grant's increments, instead of recomputing it every iteration -- is
    // distinguishable from the correct behavior.
    std::vector<uint8_t> strTable(kGrowthTableSize), conTable(kGrowthTableSize),
        intTable(kGrowthTableSize), mpTable(kGrowthTableSize);
    for (size_t i = 0; i < kGrowthTableSize; ++i) {
        strTable[i] = static_cast<uint8_t>(i + 1); // idx0=1, idx1=2
        conTable[i] = static_cast<uint8_t>(i + 2); // idx0=2, idx1=3
        intTable[i] = static_cast<uint8_t>(i + 3); // idx0=3, idx1=4
        mpTable[i]  = static_cast<uint8_t>(i + 4); // idx0=4, idx1=5
    }
    // GrantSystem holds std::atomic members, so it is neither copyable nor
    // movable -- this configures one in place rather than returning one by
    // value.
    auto SetupLevelGrantSystem = [&](GrantSystem& g) {
        g.SetGrowthTables(strTable.data(), conTable.data(), intTable.data(),
                           mpTable.data(), kGrowthTableSize);
    };

    // 4.1 +1 from level 1 adds exactly +12 maxHP, +table[0] to maxMP/STR/CON,
    //     +1 LCK, and leaves level 2. (INT is deliberately not asserted here
    //     -- level 1 & 3 == 1 is the *nonzero* branch, covered in 4.2 --
    //     alongside the level & 3 == 0 branch, so both are unambiguous.)
    {
        auto mem = FreshEwram();
        mem[kPlayerCurrentLevel] = 1;
        const uint16_t hp0 = ReadU16At(mem, kPlayerMaxHP);
        const uint16_t mp0 = ReadU16At(mem, kPlayerMaxMP);
        const uint16_t str0 = ReadU16At(mem, kPlayerBaseStats + 0);
        const uint16_t con0 = ReadU16At(mem, kPlayerBaseStats + 2);
        const uint16_t lck0 = ReadU16At(mem, kPlayerBaseStats + 6);

        GrantSystem g; SetupLevelGrantSystem(g);
        ApplyOne(g, mem, GrantKind::LevelUpOne);

        CHECK(mem[kPlayerCurrentLevel] == 2);
        CHECK(ReadU16At(mem, kPlayerMaxHP) == static_cast<uint16_t>(hp0 + 12));
        CHECK(ReadU16At(mem, kPlayerMaxMP) == static_cast<uint16_t>(mp0 + mpTable[0]));
        CHECK(ReadU16At(mem, kPlayerBaseStats + 0) == static_cast<uint16_t>(str0 + strTable[0]));
        CHECK(ReadU16At(mem, kPlayerBaseStats + 2) == static_cast<uint16_t>(con0 + conTable[0]));
        CHECK(ReadU16At(mem, kPlayerBaseStats + 6) == static_cast<uint16_t>(lck0 + 1));
    }

    // 4.2 The INT special case: at a level where level & 3 == 0, INT gains
    //     exactly 1; at a level where it is nonzero, INT gains the table
    //     value (code_08033CAC.c:85-92).
    {
        // level & 3 == 0: level 4 (4 & 3 == 0).
        auto mem = FreshEwram();
        mem[kPlayerCurrentLevel] = 4;
        const uint16_t int0 = ReadU16At(mem, kPlayerBaseStats + 4);
        GrantSystem g; SetupLevelGrantSystem(g);
        ApplyOne(g, mem, GrantKind::LevelUpOne);
        CHECK(mem[kPlayerCurrentLevel] == 5);
        CHECK(ReadU16At(mem, kPlayerBaseStats + 4) == static_cast<uint16_t>(int0 + 1));
    }
    {
        // level & 3 != 0: level 1 (1 & 3 == 1) -- table[1/5] == table[0].
        auto mem = FreshEwram();
        mem[kPlayerCurrentLevel] = 1;
        const uint16_t int0 = ReadU16At(mem, kPlayerBaseStats + 4);
        GrantSystem g; SetupLevelGrantSystem(g);
        ApplyOne(g, mem, GrantKind::LevelUpOne);
        CHECK(mem[kPlayerCurrentLevel] == 2);
        CHECK(ReadU16At(mem, kPlayerBaseStats + 4) == static_cast<uint16_t>(int0 + intTable[0]));
    }

    // 4.3 Setting 1 -> 10 in one action produces a byte-for-byte identical
    //     player block to applying +1 nine times. Levels 1-9 span both
    //     growth-table indices (1-4 use idx 0, 5-9 use idx 1), so this is
    //     exactly the band-boundary case the index must be recomputed for on
    //     every iteration rather than hoisted from the starting level.
    //
    //     Comparing the two paths against each other catches an index
    //     hoisted OUT of the loop (the batch path would then use idx 0 for
    //     all nine levels, while the sequential path -- nine independent
    //     calls, each re-reading currentLevel from ewram -- still crosses
    //     into idx 1 at level 5, so the two would disagree). It would NOT
    //     catch the index being read one level late (idx = (level+1)/5):
    //     both paths call the identical LevelUpTo, so a mutation there
    //     shifts both by the same amount and they'd still agree with each
    //     other. Guard against that with an absolute assertion computed
    //     independently from the known band split (4 levels at idx 0, 5 at
    //     idx 1), not by comparing the two paths.
    {
        auto memBatch = FreshEwram();
        memBatch[kPlayerCurrentLevel] = 1;
        const uint16_t mp0 = ReadU16At(memBatch, kPlayerMaxMP);
        const uint16_t str0 = ReadU16At(memBatch, kPlayerBaseStats + 0);
        GrantSystem gBatch; SetupLevelGrantSystem(gBatch);
        gBatch.RequestSetLevel(10);
        gBatch.ApplyPending(memBatch.data(), memBatch.size());
        CHECK(!gBatch.HasPending());
        CHECK(memBatch[kPlayerCurrentLevel] == 10);

        // Absolute check, independent of the sequential path below: levels
        // 1-4 (4 levels) index growth-table slot 0, levels 5-9 (5 levels)
        // index slot 1.
        const uint16_t expectedMp =
            static_cast<uint16_t>(mp0 + 4 * mpTable[0] + 5 * mpTable[1]);
        const uint16_t expectedStr =
            static_cast<uint16_t>(str0 + 4 * strTable[0] + 5 * strTable[1]);
        CHECK(ReadU16At(memBatch, kPlayerMaxMP) == expectedMp);
        CHECK(ReadU16At(memBatch, kPlayerBaseStats + 0) == expectedStr);

        auto memSeq = FreshEwram();
        memSeq[kPlayerCurrentLevel] = 1;
        GrantSystem gSeq; SetupLevelGrantSystem(gSeq);
        for (int i = 0; i < 9; ++i) {
            ApplyOne(gSeq, memSeq, GrantKind::LevelUpOne);
        }
        CHECK(memSeq[kPlayerCurrentLevel] == 10);

        CHECK(memBatch == memSeq);
    }

    // 4.4 The 99 cap: 98 -> target 99 works; at 99 a further +1 writes
    //     nothing; a config value above 99 clamps to 99 rather than running
    //     away.
    {
        auto mem = FreshEwram();
        mem[kPlayerCurrentLevel] = 98;
        GrantSystem g; SetupLevelGrantSystem(g);
        g.RequestSetLevel(99);
        g.ApplyPending(mem.data(), mem.size());
        CHECK(mem[kPlayerCurrentLevel] == 99);

        const uint16_t hpAt99 = ReadU16At(mem, kPlayerMaxHP);
        const uint16_t mpAt99 = ReadU16At(mem, kPlayerMaxMP);
        const uint32_t expAt99 = ReadU32(mem, kPlayerCurrentExperience);

        ApplyOne(g, mem, GrantKind::LevelUpOne);
        CHECK(mem[kPlayerCurrentLevel] == 99);
        CHECK(ReadU16At(mem, kPlayerMaxHP) == hpAt99);
        CHECK(ReadU16At(mem, kPlayerMaxMP) == mpAt99);
        CHECK(ReadU32(mem, kPlayerCurrentExperience) == expAt99);

        // RequestSetLevel(250) with an aria_config.ini-style out-of-range
        // value must clamp to kMaxLevel, not attempt levels 100..250.
        auto mem2 = FreshEwram();
        mem2[kPlayerCurrentLevel] = 50;
        GrantSystem g2; SetupLevelGrantSystem(g2);
        g2.RequestSetLevel(250);
        g2.ApplyPending(mem2.data(), mem2.size());
        CHECK(mem2[kPlayerCurrentLevel] == 99);
    }

    // 4.5 A target at or below the current level writes nothing at all:
    //     every player field, and the bytes neighbouring the player block,
    //     stay untouched.
    {
        auto mem = FreshEwram();
        mem[kPlayerCurrentLevel] = 10;
        const auto snapshot = mem;

        GrantSystem g; SetupLevelGrantSystem(g);
        g.RequestSetLevel(10); // equal to current
        g.ApplyPending(mem.data(), mem.size());
        CHECK(mem == snapshot);
        CHECK(!g.HasPending());

        g.RequestSetLevel(5); // below current
        g.ApplyPending(mem.data(), mem.size());
        CHECK(mem == snapshot);
    }

    // 4.6 currentExperience lands on threshold(newLevel), and is never
    //     reduced when the player already exceeds it.
    {
        // Starts at 0 (below the level-2 threshold): raised to it exactly.
        auto mem = FreshEwram();
        mem[kPlayerCurrentLevel] = 1;
        GrantSystem g; SetupLevelGrantSystem(g);
        ApplyOne(g, mem, GrantKind::LevelUpOne);
        CHECK(mem[kPlayerCurrentLevel] == 2);
        CHECK(ReadU32(mem, kPlayerCurrentExperience) == ExpectedExpThreshold(2));

        // Starts well above the level-2 threshold: left exactly alone.
        auto mem2 = FreshEwram();
        mem2[kPlayerCurrentLevel] = 1;
        const uint32_t bigExp = ExpectedExpThreshold(2) + 100000;
        WriteU32At(mem2, kPlayerCurrentExperience, bigExp);
        GrantSystem g2; SetupLevelGrantSystem(g2);
        ApplyOne(g2, mem2, GrantKind::LevelUpOne);
        CHECK(mem2[kPlayerCurrentLevel] == 2);
        CHECK(ReadU32(mem2, kPlayerCurrentExperience) == bigExp);
    }

    // 4.7 The plausibility gate blocks both level-up grants, and leaves the
    //     request queued for a later frame rather than dropping it.
    {
        std::vector<uint8_t> mem(aria::symbols::EWRAM_SIZE, 0);
        uint8_t* p = mem.data() + kPlayerBase;
        *reinterpret_cast<uint16_t*>(p + PlayerOffsets::MAX_HP) = 20000; // implausible
        *reinterpret_cast<uint16_t*>(p + PlayerOffsets::HP) = 42;
        mem[kPlayerCurrentLevel] = 1;

        GrantSystem g; SetupLevelGrantSystem(g);
        g.Request(GrantKind::LevelUpOne);
        g.ApplyPending(mem.data(), mem.size());
        CHECK(g.HasPending());
        CHECK(mem[kPlayerCurrentLevel] == 1); // untouched while implausible

        MakePlausible(mem);
        g.ApplyPending(mem.data(), mem.size());
        CHECK(!g.HasPending());
        CHECK(mem[kPlayerCurrentLevel] == 2);
    }

    // 4.8 (supplementary, beyond the 7 tests the brief lists) Growth tables
    //     never loaded -> the level-up grants are inert rather than using
    //     invented numbers (task brief requirement 9).
    {
        auto mem = FreshEwram();
        mem[kPlayerCurrentLevel] = 1;
        GrantSystem g; // SetGrowthTables never called
        ApplyOne(g, mem, GrantKind::LevelUpOne);
        CHECK(mem[kPlayerCurrentLevel] == 1);
    }

    std::cout << "[SUCCESS] CheatSystem, QolSystem & GrantSystem Unit Tests Passed!\n";
    return 0;
}
