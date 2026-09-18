#include "gameplay/qol_system.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>

namespace aria::gameplay {

QolSystem& QolSystem::Get() {
    static QolSystem instance;
    return instance;
}

QolSystem::QolSystem() = default;

void QolSystem::Initialize(const aria::config::GameplayConfig& config) {
    m_config = config;
    m_activeLoadoutIndex = 0;
    m_notificationFramesRemaining = 0;
    m_lastNotification.clear();
    for (auto& slot : m_loadouts) {
        slot = LoadoutPreset{};
    }
}

void QolSystem::Update(uint8_t* ewram, size_t ewramSize) {
    if (!ewram || ewramSize < 0x20000) return;

    if (m_notificationFramesRemaining > 0) {
        --m_notificationFramesRemaining;
    }
    if (m_recentEnemyFramesRemaining > 0) {
        --m_recentEnemyFramesRemaining;
    }

    if (m_config.fastDoorTransitions) {
        ProcessDoorTransitions(ewram, ewramSize);
    }

    if (m_config.fastText) {
        ProcessFastText(ewram, ewramSize);
    }
}

// =========================================================================
// 1. Quick Soul / Weapon Loadout Presets ("Dawn of Sorrow" Fix)
// =========================================================================

void QolSystem::CycleLoadout(uint8_t* ewram, size_t ewramSize, int direction) {
    if (!m_config.enableQuickLoadouts || !ewram || ewramSize < 0x20000) return;

    // Save current equipment into active slot
    LoadoutPreset& current = m_loadouts[m_activeLoadoutIndex];
    current.weapon     = ewram[qol_offsets::kEquippedWeapon];
    current.redSoul    = ewram[qol_offsets::kEquippedRedSoul];
    current.blueSoul   = ewram[qol_offsets::kEquippedBlueSoul];
    current.yellowSoul = ewram[qol_offsets::kEquippedYellowSoul];
    current.armor      = ewram[qol_offsets::kEquippedArmor];
    current.accessory  = ewram[qol_offsets::kEquippedAccessory];
    current.initialized = true;

    // Advance index
    int slots = std::clamp(m_config.quickLoadoutSlots, 2, 3);
    m_activeLoadoutIndex = (m_activeLoadoutIndex + direction + slots) % slots;

    LoadoutPreset& next = m_loadouts[m_activeLoadoutIndex];
    if (next.initialized) {
        ewram[qol_offsets::kEquippedWeapon]     = next.weapon;
        ewram[qol_offsets::kEquippedRedSoul]    = next.redSoul;
        ewram[qol_offsets::kEquippedBlueSoul]   = next.blueSoul;
        ewram[qol_offsets::kEquippedYellowSoul] = next.yellowSoul;
        ewram[qol_offsets::kEquippedArmor]      = next.armor;
        ewram[qol_offsets::kEquippedAccessory]  = next.accessory;
    } else {
        // Initialize newly entered slot with current equipment
        next = current;
    }

    m_notificationFramesRemaining = 120; // 2 seconds at 60 Hz
    m_lastNotification = "Preset " + std::string(1, static_cast<char>('A' + m_activeLoadoutIndex));
}

std::string QolSystem::GetActiveLoadoutNotification() const {
    if (m_notificationFramesRemaining <= 0) return "";
    return m_lastNotification;
}

// =========================================================================
// 2. Luck Stat Bug Fix & Soul Drop Pity Counter
// =========================================================================

void QolSystem::RecordEnemyKill(uint16_t enemyId, bool soulDropped) {
    if (!m_config.farmPitySystem) return;

    if (soulDropped) {
        m_enemyKillCounts[enemyId] = 0;
    } else {
        m_enemyKillCounts[enemyId]++;
    }
}

uint32_t QolSystem::GetEnemyDryKills(uint16_t enemyId) const {
    auto it = m_enemyKillCounts.find(enemyId);
    return (it != m_enemyKillCounts.end()) ? it->second : 0;
}

void QolSystem::ResetPityCounter(uint16_t enemyId) {
    m_enemyKillCounts[enemyId] = 0;
}

bool QolSystem::CheckSoulDrop(uint16_t enemyId, uint8_t baseRate, uint16_t playerLuck, uint32_t randomValue) {
    if (baseRate == 0) return false;

    // Check Pity System: if next dry kill reaches threshold, guarantee soul drop
    if (m_config.farmPitySystem) {
        auto it = m_enemyKillCounts.find(enemyId);
        if (it != m_enemyKillCounts.end() && (it->second + 1) >= static_cast<uint32_t>(m_config.soulPityThreshold)) {
            m_enemyKillCounts[enemyId] = 0;
            return true;
        }
    }

    int32_t modulus = 16;
    if (m_config.fixLuckStat) {
        // Fixed formula: Luck directly provides positive linear scaling.
        // Base modulus is (baseRate << 3). Each 2 points of Luck reduces the divisor by 1.
        int32_t baseMod = baseRate << 3;
        int32_t luckReduction = playerLuck / 2;
        modulus = std::max(1, baseMod - luckReduction);
    } else {
        // Vanilla retail GBA code (sub_080683BC at 0x080684D0)
        // Shifting left 16 and arithmetic right 20 divides Luck by 16.
        // Then subtracts 32: for Luck < 512, (Luck/16 - 32) is negative, worsening the drop rate.
        int32_t luckShift = static_cast<int32_t>(static_cast<int16_t>(playerLuck)) >> 4;
        modulus = (baseRate << 3) - (luckShift - 32);
        if (modulus <= 0) modulus = 16;
    }

    uint32_t threshold = 7;
    if (m_config.soulDropMultiplier > 1.0f) {
        threshold = static_cast<uint32_t>(threshold * m_config.soulDropMultiplier);
    }

    bool dropped = ((randomValue >> 2) % modulus) < threshold;
    RecordEnemyKill(enemyId, dropped);
    return dropped;
}

bool QolSystem::CheckItemDrop(uint16_t /*enemyId*/, uint8_t baseRate, uint16_t playerLuck, uint32_t randomValue, bool isRare) {
    if (baseRate == 0) return false;

    int32_t modulus = 16;
    if (m_config.fixLuckStat) {
        int32_t baseMod = baseRate << 2;
        int32_t luckReduction = playerLuck / 4;
        modulus = std::max(1, baseMod - luckReduction);
    } else {
        int32_t luckShift = static_cast<int32_t>(static_cast<int16_t>(playerLuck)) >> 4;
        modulus = (baseRate << 2) - (luckShift - 16);
        if (modulus <= 0) modulus = 16;
    }

    uint32_t threshold = isRare ? 8 : 4;
    if (m_config.itemDropMultiplier > 1.0f) {
        threshold = static_cast<uint32_t>(threshold * m_config.itemDropMultiplier);
    }

    return ((randomValue >> 3) % modulus) < threshold;
}

float QolSystem::GetAdjustedDropRate(uint16_t enemyId, float baseRate) const {
    float bonus = 0.0f;
    if (m_config.farmPitySystem) {
        auto it = m_enemyKillCounts.find(enemyId);
        if (it != m_enemyKillCounts.end()) {
            bonus = (static_cast<float>(it->second) / 10.0f) * 0.05f;
        }
    }
    return baseRate + bonus;
}

// =========================================================================
// 3. Soul Indicators & Mini-Map
// =========================================================================

void QolSystem::SetEnemyTable(const uint8_t* table, size_t size) {
    if (!table || size == 0) return;
    m_enemyTable.assign(table, table + size);
}

void QolSystem::SetRecentTargetEnemy(uint16_t enemyId) {
    m_recentEnemyId = enemyId;
    m_recentEnemyFramesRemaining = 180; // 3 seconds
}

int QolSystem::GetEnemySoulCount(const uint8_t* ewram, size_t ewramSize, uint16_t enemyId, const uint8_t* rom, size_t romSize) const {
    if (!ewram || ewramSize < 0x20000) return 0;

    const uint8_t* entry = nullptr;
    if (rom && romSize >= qol_offsets::kEnemyTableFileOffset + (static_cast<size_t>(enemyId) + 1) * qol_offsets::kEnemyEntrySize) {
        entry = rom + qol_offsets::kEnemyTableFileOffset + static_cast<size_t>(enemyId) * qol_offsets::kEnemyEntrySize;
    } else if (!m_enemyTable.empty() && m_enemyTable.size() >= (static_cast<size_t>(enemyId) + 1) * qol_offsets::kEnemyEntrySize) {
        entry = m_enemyTable.data() + static_cast<size_t>(enemyId) * qol_offsets::kEnemyEntrySize;
    }

    if (!entry) return 0;

    uint8_t soulType  = entry[0x17];
    uint8_t rawIndex  = entry[0x18];
    if (rawIndex == 0) return 0; // No soul for this enemy
    uint8_t soulIndex = rawIndex - 1;

    const uint8_t* inv = nullptr;
    size_t invSize = 0;

    switch (soulType) {
        case 0: // Bullet (Red)
            inv = ewram + qol_offsets::kRedSoulInventory;
            invSize = 0x1C;
            break;
        case 1: // Guardian (Blue)
            inv = ewram + qol_offsets::kBlueSoulInventory;
            invSize = 0x0D;
            break;
        case 2: // Enchant (Yellow)
            inv = ewram + qol_offsets::kYellowSoulInventory;
            invSize = 0x12;
            break;
        case 3: // Ability
            inv = ewram + qol_offsets::kAbilitySoulInventory;
            invSize = 0x03;
            break;
        default:
            return 0;
    }

    if ((soulIndex >> 1) >= invSize) return 0;

    uint8_t byteVal = inv[soulIndex >> 1];
    uint8_t count = (soulIndex & 1) ? ((byteVal >> 4) & 0x0F) : (byteVal & 0x0F);
    return std::min(static_cast<int>(count), 9);
}

static const char* const kEnemyNames[] = {
    "Bat", "Zombie", "Skeleton", "Merman", "Axe Armor", "Skull Archer", "Peeping Eye", "Killer Fish",
    "Bone Pillar", "Blue Crow", "Buer", "White Dragon", "Zombie Soldier", "Skeleton Knight", "Ghost",
    "Siren", "Tiny Devil", "Stray Doll", "Zombie Officer", "Creaking Skull", "Catoblepas", "Ghost Dancer",
    "Golem", "Slime", "Une", "Giant Worm", "Durahan", "Lightning Doll", "Calavera", "Nightmare",
    "Nemesis", "Kyoma Demon", "Chronomage", "Poison Worm", "Red Crow", "Gargoyle", "Fleaman", "Devil",
    "Gorgon", "Alura Une", "Gladiator", "Iron Golem", "Dead Crusader", "Ripper", "Minotaur", "Mandragora",
    "Flesh Golem", "Sky Fish", "Dead Warrior", "Imp", "Harpy", "Disk Armor", "Great Armor", "Triton",
    "Manticore", "Big Golem", "Headhunter", "Death", "Legion", "Balore", "Graham", "Chaos"
};

std::string QolSystem::GetEnemyName(uint16_t enemyId) const {
    if (enemyId < sizeof(kEnemyNames) / sizeof(kEnemyNames[0])) {
        return kEnemyNames[enemyId];
    }
    return "Enemy #" + std::to_string(enemyId);
}

bool QolSystem::HasEnemySoul(const uint8_t* ewram, size_t ewramSize, uint16_t enemyId, const uint8_t* rom, size_t romSize) const {
    return GetEnemySoulCount(ewram, ewramSize, enemyId, rom, romSize) > 0;
}

void QolSystem::QueryLocalMiniMap(const uint8_t* ewram, size_t ewramSize, int& playerRoomX, int& playerRoomY, std::vector<MiniMapCell>& outCells) const {
    outCells.clear();
    if (!ewram || ewramSize < 0x20000) return;

    // Camera fractional coords to room indices
    int32_t camX = *reinterpret_cast<const int32_t*>(ewram + qol_offsets::kCameraFracX) >> 16;
    int32_t camY = *reinterpret_cast<const int32_t*>(ewram + qol_offsets::kCameraFracY) >> 16;

    playerRoomX = std::clamp(camX / 256, 0, 63);
    playerRoomY = std::clamp(camY / 160, 0, 39);

    // Map grid at EWRAM kMapGrid (40 rows, 2 32-bit words per row: visited and revealed)
    const uint8_t* grid = ewram + qol_offsets::kMapGrid;

    // Sample an 8x8 region centered around the player
    int startX = std::max(0, playerRoomX - 4);
    int endX   = std::min(63, playerRoomX + 4);
    int startY = std::max(0, playerRoomY - 4);
    int endY   = std::min(39, playerRoomY + 4);

    for (int y = startY; y <= endY; ++y) {
        size_t rowOffset = static_cast<size_t>(y) * 8;
        uint32_t visitedBits  = *reinterpret_cast<const uint32_t*>(grid + rowOffset);
        uint32_t revealedBits = *reinterpret_cast<const uint32_t*>(grid + rowOffset + 4);

        for (int x = startX; x <= endX; ++x) {
            if (x < 32) {
                uint32_t bit = (1u << x);
                MiniMapCell cell;
                cell.x = x;
                cell.y = y;
                cell.visited  = (visitedBits & bit) != 0;
                cell.revealed = (revealedBits & bit) != 0;
                if (cell.visited || cell.revealed) {
                    outCells.push_back(cell);
                }
            }
        }
    }
}

// =========================================================================
// 4. Fast Room Door Transitions
// =========================================================================

void QolSystem::ProcessDoorTransitions(uint8_t* ewram, size_t ewramSize) {
    if (!ewram || ewramSize < 0x20000) return;

    // Room transition state object at EWRAM 0x00060 (cvaos code_080109F4.c)
    // unk_64: 4 = door transition active
    // unk_65: step (3 = fade out, 4 = room load, 5 = display reset, 6 = fade in)
    uint8_t* transObj = ewram + qol_offsets::kRoomTransitionObj;
    if (transObj[0x64] == 4) {
        if (transObj[0x65] == 3) {
            // Screen is fading out over multiple frames. Fast-forward immediately to room load.
            transObj[0x65] = 4;
        } else if (transObj[0x65] == 6) {
            // Screen is fading back in over multiple frames. Fast-forward to completion.
            transObj[0x65] = 1;
        }
    }
}

// =========================================================================
// 5. Fast Text & Cutscene Fast-Forward
// =========================================================================

void QolSystem::ProcessFastText(uint8_t* ewram, size_t ewramSize) {
    if (!ewram || ewramSize < 0x20000) return;

    // Dialogue text box controller at EWRAM 0x13110 (cvaos structs/ewram.h)
    // unk_13110->unk_524 / unk_528 holds the typewriter tick countdown.
    // Setting typewriter delay tick to 0 causes the text engine to print full string in 1 frame.
    uint8_t* playerPartner = ewram + 0x13114;
    if (playerPartner) {
        // Dialogue typewriter flag reset
        ewram[0x131BF] = 0;
    }
}

} // namespace aria::gameplay
