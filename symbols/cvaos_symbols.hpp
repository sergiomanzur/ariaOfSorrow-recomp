#pragma once

#include <cstdint>

namespace aria::symbols {

// GBA Memory Map Base Addresses
constexpr uint32_t EWRAM_BASE = 0x02000000;
constexpr uint32_t IWRAM_BASE = 0x03000000;
constexpr uint32_t IO_BASE    = 0x04000000;
constexpr uint32_t PAL_BASE   = 0x05000000;
constexpr uint32_t VRAM_BASE  = 0x06000000;
constexpr uint32_t OAM_BASE   = 0x07000000;
constexpr uint32_t ROM_BASE   = 0x08000000;
constexpr uint32_t SRAM_BASE  = 0x0E000000;

// Memory Sizes
constexpr size_t EWRAM_SIZE = 256 * 1024; // 256 KB
constexpr size_t IWRAM_SIZE = 32 * 1024;  // 32 KB
constexpr size_t VRAM_SIZE  = 96 * 1024;  // 96 KB
constexpr size_t OAM_SIZE   = 1024;       // 1 KB (128 OBJs)
constexpr size_t PAL_SIZE   = 1024;       // 1 KB (512 colors)
constexpr size_t SRAM_SIZE  = 64 * 1024;  // 64 KB (Flash/SRAM)

// Castlevania: Aria of Sorrow (USA) Known Offsets & Symbol Definitions (cvaos)
namespace cvaos_us {

    // Core Game State Offsets (in EWRAM)
    constexpr uint32_t ADDR_GAME_STATE      = 0x02000000;
    constexpr uint32_t ADDR_RNG_SEED        = 0x02000010;
    constexpr uint32_t ADDR_AREA_ID         = 0x02000018;
    constexpr uint32_t ADDR_ROOM_ID         = 0x02000019;
    constexpr uint32_t ADDR_GAME_MODE       = 0x0200001A;

    // Player (Soma Cruz / Julius Belmont) save-relevant stat block, taken
    // from cvaos (third_party/cvaos/include/structs/ewram.h,
    // struct EwramData_unk1325C) -- a from-scratch, community reverse-
    // engineering decomp of this exact game, not a guess. The struct starts
    // at EWRAM+0x1325C; ADDR_PLAYER_ENTITY below is that struct's base
    // address, and PlayerOffsets are relative to it.
    //
    // Cross-checked against a live capture (headless run, frame 11000, the
    // "are we in Europe? / Dracula's Castle?" dialogue just after the
    // eclipse): level=1, currentHP=maxHP=320, currentMP=maxMP=80, exp=0,
    // gold=0 -- internally consistent (current == max, a fresh unhurt
    // character) and structurally sane for this exact point in the story.
    //
    // The previous version of this file placed the player block at EWRAM+
    // 0x20 (i.e. ADDR_PLAYER_ENTITY = 0x02000020) with an invented HEARTS/
    // MAX_HEARTS pair -- that address was never verified against a running
    // game, and Aria of Sorrow has no "Hearts" resource at all (its soul
    // system consumes MP, not a separate currency); both were fabricated.
    constexpr uint32_t ADDR_PLAYER_ENTITY   = symbols::EWRAM_BASE + 0x1325C;

    struct PlayerOffsets {
        static constexpr uint32_t CURRENT_LEVEL = 0x1D;   // u8  (0x13279)
        static constexpr uint32_t HP             = 0x1E;   // s16 (0x1327A)
        static constexpr uint32_t MP             = 0x20;   // s16 (0x1327C)
        static constexpr uint32_t MAX_HP         = 0x22;   // u16 (0x1327E)
        static constexpr uint32_t MAX_MP         = 0x24;   // u16 (0x13280)
        static constexpr uint32_t CURRENT_EXP    = 0x30;   // u32 (0x1328C)
        static constexpr uint32_t CURRENT_GOLD   = 0x34;   // u32 (0x13290)
    };

    // Game Mode Constants
    enum class GameMode : uint8_t {
        Normal      = 0,
        Hard        = 1,
        Julius      = 2,
        BossRush    = 3
    };

    // Castle Area Identifiers
    enum class CastleArea : uint8_t {
        CastleCorridor          = 0,
        Chapel                  = 1,
        Study                   = 2,
        DanceHall               = 3,
        InnerQuarters           = 4,
        FloatingCatacombs       = 5,
        ClockTower              = 6,
        UndergroundReservoir    = 7,
        UndergroundCemetery     = 8,
        TheArena                = 9,
        TopFloor                = 10,
        FloatingGarden          = 11,
        ChaoticRealm            = 12
    };

} // namespace cvaos_us

} // namespace aria::symbols
