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

    // Player (Soma Cruz / Julius Belmont) Data Structure
    constexpr uint32_t ADDR_PLAYER_ENTITY   = 0x02000020;

    struct PlayerOffsets {
        static constexpr uint32_t HP            = 0x00; // u16
        static constexpr uint32_t MAX_HP        = 0x02; // u16
        static constexpr uint32_t MP            = 0x04; // u16
        static constexpr uint32_t MAX_MP        = 0x06; // u16
        static constexpr uint32_t HEARTS        = 0x08; // u16
        static constexpr uint32_t MAX_HEARTS    = 0x0A; // u16
        static constexpr uint32_t LEVEL         = 0x0C; // u8
        static constexpr uint32_t EXP           = 0x10; // u32
        static constexpr uint32_t GOLD          = 0x14; // u32
        static constexpr uint32_t POS_X         = 0x20; // s32 (fixed 16.16)
        static constexpr uint32_t POS_Y         = 0x24; // s32 (fixed 16.16)
        static constexpr uint32_t VEL_X         = 0x28; // s16
        static constexpr uint32_t VEL_Y         = 0x2A; // s16
        static constexpr uint32_t FACING_DIR    = 0x2E; // u8 (0: Right, 1: Left)
        static constexpr uint32_t BULLET_SOUL   = 0x30; // u8
        static constexpr uint32_t GUARDIAN_SOUL = 0x31; // u8
        static constexpr uint32_t ENCHANTED_SOUL= 0x32; // u8
        static constexpr uint32_t ABILITY_SOULS = 0x34; // u16 bitmask
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
