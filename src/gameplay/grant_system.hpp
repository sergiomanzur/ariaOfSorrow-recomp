#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace aria::gameplay {

// Save-block layout, all offsets relative to EWRAM_BASE (0x02000000).
//
// Every constant here is transcribed from the community decompilation in
// third_party/cvaos (a read-only reference tree); nothing is derived by
// guesswork. Citations are file:line into that tree.
//
// struct EwramData_unk1325C -- third_party/cvaos/include/structs/ewram.h:715-756
namespace grant_offsets {

// ewram.h:718 -- u16 totalNbrSoulsCollected
constexpr uint32_t kTotalSoulsCollected  = 0x13264;
// ewram.h:741 -- u8 itemInventory[0x20]
constexpr uint32_t kItemInventory        = 0x13294;
// ewram.h:742 -- u8 weaponInventory[0x3B]
constexpr uint32_t kWeaponInventory      = 0x132B4;
// ewram.h:743 -- u8 armorInventory[0x19]
constexpr uint32_t kArmorInventory       = 0x132EF;
// ewram.h:744 -- u8 accessoryInventory[0x14]  (== kArmorInventory + 0x19)
constexpr uint32_t kAccessoryInventory   = 0x13308;
// ewram.h:745 -- u8 redSoulInventory[2][0x1C]      ("bullet" souls)
constexpr uint32_t kRedSoulInventory     = 0x1331C;
// ewram.h:746 -- u8 blueSoulInventory[2][0xD]      ("guardian" souls)
constexpr uint32_t kBlueSoulInventory    = 0x13354;
// ewram.h:747 -- u8 yellowSoulInventory[2][0x12]   ("enchant" souls)
constexpr uint32_t kYellowSoulInventory  = 0x1336E;
// ewram.h:748 -- u8 abilitySoulInventory[0x3] (flat, no group dimension)
constexpr uint32_t kAbilitySoulInventory = 0x13392;
// ewram.h:750 -- u8 unk_13396[2]. The ability-soul ENABLED bitfield, indexed
// by ability-soul index: sub_08032ADC(idx, 0|1) sets/clears
// (third_party/cvaos/src/code_08032444.c:718-735), sub_08032AB8(idx) reads
// (:702-710), and the ability menu drives both from the same index it uses for
// the soul nibble (asm/code/code_08040A38.s:21336-21365). Only the
// ability-soul grant may write here -- see SetAbilitySoulEnabledBit in the .cpp.
constexpr uint32_t kAbilitySoulEnabledFlags = 0x13396;

// ewram.h:563 -- struct EwramData_unkB4 unk_B4[2][40], each element
// {u32 unk_B4; u32 unk_B8;} (ewram.h:508-511). unk_B4 is the "visited"
// plane, unk_B8 the "revealed" plane; both are indexed
// [x >> 5][y] with bit (x & 0x1F) -- see
// third_party/cvaos/src/code_080109F4.c:683-684 (the game's own reveal
// of the room the player walks into) and :409/:454 (the map renderer).
constexpr uint32_t kMapGrid              = 0x000B4;
constexpr uint32_t kMapGridElementSize   = 8;
constexpr int      kMapGridRows          = 40;

// Grant counts, verbatim from the game's own debug give-all:
//   s32 sUnk_084F158C[] = {0x37, 0x18, 0x23, 0x8};  (souls)
//   s32 sUnk_084F159C[] = {0x20, 0x3B, 0x2D};       (items)
// third_party/cvaos/src/code_08038A38.c:396-402, consumed at :435-463.
constexpr int kRedSoulCount     = 0x37; // 55
constexpr int kBlueSoulCount    = 0x18; // 24
constexpr int kYellowSoulCount  = 0x23; // 35
constexpr int kAbilitySoulCount = 0x08; // 8  -> 4 bytes, spills into pad_13395
constexpr int kItemCount        = 0x20; // 32
constexpr int kWeaponCount      = 0x3B; // 59
// 45, not 25: the game's loop deliberately runs contiguously out of
// armorInventory[0x19] and on into accessoryInventory
// (0x132EF + 0x19 == 0x13308). See code_08038A38.c:446-460.
constexpr int kArmorCount       = 0x2D; // 45

// The room-occupancy table is ROM data (`sUnk_08116650`, declared
// third_party/cvaos/src/code_080109F4.c:33, defined
// third_party/cvaos/asm/data/data_0E5364.s:46-47), so it lives at ROM
// 0x08116650 == file offset 0x116650. u16 cells, row stride 64,
// cell = table[x + (y << 6)]; 0xFFFF means "no room here".
// The game scans x < 0x40, y < 0x23 (code_080109F4.c:398-402).
constexpr uint32_t kRoomTableFileOffset = 0x116650;
constexpr int      kRoomTableWidth      = 64;   // 0x40
constexpr int      kRoomTableHeight     = 35;   // 0x23
constexpr size_t   kRoomTableCells =
    static_cast<size_t>(kRoomTableWidth) * kRoomTableHeight;
constexpr uint16_t kNoRoom              = 0xFFFF;

// ---- Level-up cheats -------------------------------------------------
//
// Transcribed from the game's own level-up routine, sub_08033CAC
// (third_party/cvaos/src/code_08033CAC.c:61-98). It ADDS per-level
// increments to the player's stats -- it does not recompute them from a
// table -- so a level-up grant must replay that same additive loop rather
// than writing currentLevel alone.
//
// Max level is hard-coded to 99 for both Soma and Julius
// (code_08033CAC.c:66, :71 -- the loop's own bound).
constexpr int kMaxLevel = 99;

// Player-block fields the level-up cheats touch, as absolute offsets from
// EWRAM_BASE (third_party/cvaos/include/structs/ewram.h:715-756,
// struct EwramData_unk1325C, which starts at EWRAM+0x1325C).
//
// CROSS-REFERENCE: symbols/cvaos_symbols.hpp's cvaos_us::PlayerOffsets
// (CURRENT_LEVEL, MAX_HP, MAX_MP, CURRENT_EXP -- there is no BASE_STATS
// entry there) declares the same fields as offsets relative to
// ADDR_PLAYER_ENTITY instead of absolute EWRAM ones. The two are
// numerically equal today (ADDR_PLAYER_ENTITY - EWRAM_BASE == 0x1325C, so
// e.g. PlayerOffsets::MAX_HP (0x22) + 0x1325C == kPlayerMaxHP (0x1327E)),
// and grant_system.cpp's own plausibility gate reads MAX_HP/HP through
// PlayerOffsets while LevelUpTo reads/writes maxHP/maxMP/baseStats/exp
// through these constants -- two independent definitions of the same
// struct. If either file's offsets ever change, check the other.
constexpr uint32_t kPlayerCurrentLevel      = 0x13279; // ewram.h:732, u8
constexpr uint32_t kPlayerMaxHP             = 0x1327E; // ewram.h:735, u16
constexpr uint32_t kPlayerMaxMP             = 0x13280; // ewram.h:736, u16
// ewram.h:737 -- u16 baseStats[4]: 0 STR, 1 CON, 2 INT, 3 LCK.
constexpr uint32_t kPlayerBaseStats         = 0x13282;
constexpr uint32_t kPlayerCurrentExperience = 0x1328C; // ewram.h:739, u32

// Growth tables sUnk_080E1DCC (STR), sUnk_080E1DE0 (CON), sUnk_080E1DF4
// (INT) and sUnk_080E1E08 (MP) (code_08033CAC.c:38-52): 20 bytes of u8
// each, indexed by currentLevel/5, read BEFORE currentLevel is
// incremented (:82-94). LCK is always +1 and maxHP always +12 -- no table
// backs either of those.
constexpr uint32_t kGrowthTableStrFileOffset = 0x0E1DCC;
constexpr uint32_t kGrowthTableConFileOffset = 0x0E1DE0;
constexpr uint32_t kGrowthTableIntFileOffset = 0x0E1DF4;
constexpr uint32_t kGrowthTableMpFileOffset  = 0x0E1E08;
constexpr size_t   kGrowthTableSize          = 20;

} // namespace grant_offsets

enum class GrantKind {
    BulletSouls = 0,
    GuardianSouls,
    EnchantSouls,
    AbilitySouls,
    Weapons,
    ArmorAndAccessories,
    Consumables,
    RevealMap,
    LevelUpOne,
    SetLevelToTarget,
    Count,
};

// One-shot progression grants (souls / equipment / map reveal).
//
// Deliberately not part of CheatSystem: those cheats re-pin a value every
// single frame, while these are events -- a menu press queues exactly one
// application and the flag then clears. Sharing the class would make it
// impossible to tell "still enabled" from "already happened".
class GrantSystem {
public:
    static GrantSystem& Get();

    GrantSystem() = default;

    // UI thread. Only records the request -- guest memory is never touched
    // from here, because the overlay callback runs on the host's event loop
    // while the recompiled game is mid-frame on its own thread.
    void Request(GrantKind kind);

    // UI thread. Records the desired target level (clamped to
    // [1, grant_offsets::kMaxLevel] here as one of the two clamp points --
    // the other is inside ApplyPending itself -- so a value from a
    // hand-edited config file can never drive the level-up loop past the
    // cap) and queues GrantKind::SetLevelToTarget.
    void RequestSetLevel(int targetLevel);

    // Startup: the ROM's room-occupancy table, read once by the host from
    // the ROM file. `count` is expected to be kRoomTableCells; a shorter
    // table simply disables the map grant rather than reading out of bounds.
    void SetRoomTable(const uint16_t* cells, size_t count);

    // Startup: the ROM's four level-up growth tables (STR/CON/INT/MP),
    // read once by the host from the ROM file. `count` is expected to be
    // grant_offsets::kGrowthTableSize for every table; anything else
    // disables both level-up grants rather than reading out of bounds or
    // applying invented growth numbers.
    void SetGrowthTables(const uint8_t* strTable, const uint8_t* conTable,
                         const uint8_t* intTable, const uint8_t* mpTable,
                         size_t count);

    // Startup: where the runtime keeps this ROM's battery save, so the first
    // applied grant can leave a timestamped copy behind.
    void SetSavePath(std::string path);

    // Guest thread, at a frame boundary. Applies (at most) the pending set
    // and clears it. A frame whose player block looks implausible applies
    // nothing and leaves the request queued for the next frame.
    void ApplyPending(uint8_t* ewram, size_t ewramSize);

    bool HasPending() const {
        return m_pending.load(std::memory_order_relaxed) != 0;
    }

private:
    void EnsureBackupOnce();
    void RevealMap(uint8_t* ewram, size_t ewramSize);

    // Shared by both level-up grants. When `oneLevelOnly` is true, `target`
    // is ignored and the effective target becomes currentLevel + 1 (still
    // clamped to kMaxLevel) -- exactly one application of the game's own
    // per-level loop. Writes nothing at all (not even currentExperience) if
    // the effective target ends up at or below the current level, if the
    // growth tables were never loaded, or if any touched field would fall
    // outside `ewram`.
    void LevelUpTo(uint8_t* ewram, size_t ewramSize, int target,
                   bool oneLevelOnly);

    bool GrowthTablesReady() const;

    std::atomic<uint32_t> m_pending{0};
    std::atomic<int> m_targetLevel{0};
    std::vector<uint16_t> m_roomTable;
    std::vector<uint8_t> m_strGrowth;
    std::vector<uint8_t> m_conGrowth;
    std::vector<uint8_t> m_intGrowth;
    std::vector<uint8_t> m_mpGrowth;
    std::string m_savePath;
    bool m_backupAttempted = false;
};

} // namespace aria::gameplay
