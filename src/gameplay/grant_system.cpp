#include "gameplay/grant_system.hpp"

#include <cstring>
#include <ctime>
#include <filesystem>
#include <iostream>

#include "symbols/cvaos_symbols.hpp"

namespace aria::gameplay {
namespace {

using namespace grant_offsets;

constexpr size_t kMinEwramSize = 0x40000; // 256 KiB

bool Fits(size_t ewramSize, uint32_t offset, size_t length) {
    return static_cast<size_t>(offset) + length <= ewramSize;
}

uint16_t ReadU16(const uint8_t* p) {
    uint16_t v = 0;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

void WriteU16(uint8_t* p, uint16_t v) { std::memcpy(p, &v, sizeof(v)); }

uint32_t ReadU32(const uint8_t* p) {
    uint32_t v = 0;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

void WriteU32(uint8_t* p, uint32_t v) { std::memcpy(p, &v, sizeof(v)); }

// Same reasoning (and the same band) as CheatSystem's gate,
// src/gameplay/cheat_system.cpp:40-42: the save block's address is
// reverse-engineered, and a write here lands in memory the runtime flushes
// to the player's real .sav. A max stat outside a sane range, or a current
// value already above it, means the struct isn't where this build expects it
// right now -- the title screen, a save not yet loaded, an unexpected state --
// so nothing may be written.
bool PlayerBlockPlausible(const uint8_t* ewram, size_t ewramSize) {
    const uint32_t base =
        symbols::cvaos_us::ADDR_PLAYER_ENTITY - symbols::EWRAM_BASE;
    if (!Fits(ewramSize, base, 0x38)) return false;
    const uint16_t maxHp =
        ReadU16(ewram + base + symbols::cvaos_us::PlayerOffsets::MAX_HP);
    const uint16_t curHp =
        ReadU16(ewram + base + symbols::cvaos_us::PlayerOffsets::HP);
    return maxHp > 0 && maxHp <= 9999 && curHp <= maxHp;
}

// Mirrors SoulInventory_AddAmountToFirstGroupTotal(type, index, 1)
// (third_party/cvaos/src/code_08032444.c:94-160) for indices [0, count):
// a soul is a nibble in group 0, index i at byte (i >> 1), low nibble when i
// is even and high when odd; the amount is clamped to 9 and
// totalNbrSoulsCollected takes the raw addition, capped at 999 (:124-141).
// Amount granted is 1, exactly as the game's give-all passes (:439) -- not 9.
void GrantSoulRange(uint8_t* ewram, size_t ewramSize, uint32_t base, int count) {
    const size_t bytes = static_cast<size_t>((count + 1) / 2);
    if (!Fits(ewramSize, base, bytes)) return;
    if (!Fits(ewramSize, kTotalSoulsCollected, sizeof(uint16_t))) return;

    uint32_t total = ReadU16(ewram + kTotalSoulsCollected);
    for (int i = 0; i < count; ++i) {
        uint8_t& cell = ewram[base + static_cast<size_t>(i >> 1)];
        int amount = (i & 1) ? ((cell >> 4) & 0xF) : (cell & 0xF);
        amount += 1;
        if (amount > 9) amount = 9;
        if (i & 1) {
            cell = static_cast<uint8_t>((cell & 0x0F) | (amount << 4));
        } else {
            cell = static_cast<uint8_t>((cell & 0xF0) | amount);
        }
        // The game increments the running total per call regardless of
        // whether the nibble itself clamped (code_08032444.c:134-141).
        total += 1;
        if (total > 999) total = 999;
    }
    WriteU16(ewram + kTotalSoulsCollected, static_cast<uint16_t>(total));
}

// Mirrors the give-all's item loop (code_08038A38.c:446-462): each entry's
// byte -- which is a quantity, not a flag -- is set to 1.
void GrantItemRange(uint8_t* ewram, size_t ewramSize, uint32_t base, int count) {
    const size_t bytes = static_cast<size_t>(count);
    if (!Fits(ewramSize, base, bytes)) return;
    std::memset(ewram + base, 1, bytes);
}

// Mirrors sub_08032ADC(bit, 1) (code_08032444.c:718-735): a plain bit set in
// the two-byte field at 0x13396.
//
// The field is the ability-soul ENABLED bitfield, indexed by ability-soul
// index -- not, as it first appears from the give-all's literals, a
// per-category flag. The ability menu reads the selected index out of the
// entity, asks SoulInventory_GetSoulTotal(3 /*ability*/, idx), and only if
// that is non-zero does it read sub_08032AB8(idx) and write
// sub_08032ADC(idx, 0|1) -- one index driving both the soul nibble and the bit
// (asm/code/code_08040A38.s:21336-21365, sub_0804BB40). The reset path clears
// exactly bits 0..7, one per ability soul
// (third_party/cvaos/src/code_08012744.c:507-510), and the player entity gates
// individual traversal moves on individual bits
// (asm/code/code_08014548.s:14994 and :16313 read bit 0, :15903 and :16853
// bit 1, :9920 bit 2, :7210 bit 3, :9789 bit 4).
//
// So only the ability-soul grant may touch it. Setting a bit whose nibble is
// still 0 hands the player a traversal move they never earned *and* one they
// cannot clear: the menu's toggle path refuses to flip the bit unless
// GetSoulTotal(3, idx) is non-zero, and 0x13396 lives inside the save block,
// so the next in-game save makes it permanent short of a new game.
void SetAbilitySoulEnabledBit(uint8_t* ewram, size_t ewramSize, int bit) {
    if (bit < 0) return;
    const uint32_t offset = kAbilitySoulEnabledFlags + static_cast<uint32_t>(bit >> 3);
    if (!Fits(ewramSize, offset, 1)) return;
    ewram[offset] |= static_cast<uint8_t>(1 << (bit & 7));
}

// sub_08033CAC_inline_0 (code_08033CAC.c:31-36): x*(x+1)*(3x+8), the EXP
// required to reach level x. Computed in a wider integer so nothing here
// depends on the fact that x <= kMaxLevel keeps every intermediate product
// well inside 32 bits anyway.
uint32_t ExpThresholdForLevel(int level) {
    const int64_t x = level;
    const int64_t value = x * (x + 1) * (3 * x + 8);
    return static_cast<uint32_t>(value);
}

std::string TimestampSuffix() {
    const std::time_t now = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif
    char buf[32] = {};
    std::strftime(buf, sizeof(buf), "%Y%m%d-%H%M%S", &tm);
    return buf;
}

} // namespace

GrantSystem& GrantSystem::Get() {
    static GrantSystem instance;
    return instance;
}

void GrantSystem::Request(GrantKind kind) {
    if (kind >= GrantKind::Count) return;
    // Release: pairs with ApplyPending's acquire-load of m_pending below, so
    // anything a caller (e.g. RequestSetLevel) sequenced before this fetch_or
    // -- on any variable, not just atomics -- is guaranteed visible to the
    // guest thread once it observes the bit this sets.
    m_pending.fetch_or(1u << static_cast<int>(kind), std::memory_order_release);
}

void GrantSystem::RequestSetLevel(int targetLevel) {
    int clamped = targetLevel;
    if (clamped > kMaxLevel) clamped = kMaxLevel;
    if (clamped < 1) clamped = 1;
    // Stored before the request bit is raised, and this is a real
    // happens-before guarantee, not just program order: Request()'s
    // fetch_or below is a release operation, and ApplyPending's read of
    // m_pending that observes the SetLevelToTarget bit is an acquire
    // operation (see both sites) -- that release/acquire pair on m_pending
    // is what guarantees the guest thread sees this store once it sees the
    // bit, regardless of this store's own memory order. It is additionally
    // marked release (paired with an acquire load in ApplyPending) so the
    // guarantee holds by inspection of this variable alone too.
    m_targetLevel.store(clamped, std::memory_order_release);
    Request(GrantKind::SetLevelToTarget);
}

void GrantSystem::SetRoomTable(const uint16_t* cells, size_t count) {
    if (!cells || count == 0) {
        m_roomTable.clear();
        return;
    }
    m_roomTable.assign(cells, cells + count);
}

void GrantSystem::SetGrowthTables(const uint8_t* strTable, const uint8_t* conTable,
                                   const uint8_t* intTable, const uint8_t* mpTable,
                                   size_t count) {
    // All four tables come from the same ROM read; if any one of them isn't
    // exactly kGrowthTableSize long, disable the level-up grants entirely
    // rather than mixing a good table with a missing one.
    if (!strTable || !conTable || !intTable || !mpTable ||
        count != kGrowthTableSize) {
        m_strGrowth.clear();
        m_conGrowth.clear();
        m_intGrowth.clear();
        m_mpGrowth.clear();
        return;
    }
    m_strGrowth.assign(strTable, strTable + count);
    m_conGrowth.assign(conTable, conTable + count);
    m_intGrowth.assign(intTable, intTable + count);
    m_mpGrowth.assign(mpTable, mpTable + count);
}

void GrantSystem::SetSavePath(std::string path) { m_savePath = std::move(path); }

bool GrantSystem::GrowthTablesReady() const {
    return m_strGrowth.size() == kGrowthTableSize &&
           m_conGrowth.size() == kGrowthTableSize &&
           m_intGrowth.size() == kGrowthTableSize &&
           m_mpGrowth.size() == kGrowthTableSize;
}

// One timestamped copy of the battery save per process, taken just before the
// first grant actually lands. Grants are irreversible edits to a real save
// file; the player who tries one out and dislikes it has no other way back.
void GrantSystem::EnsureBackupOnce() {
    if (m_backupAttempted) return;
    if (m_savePath.empty()) return;

    // A save file that does not exist yet does NOT consume the one attempt.
    // On a first boot the .sav is only created once the game itself writes
    // SRAM, so burning the attempt here would mean the player who grants
    // something now, saves in-game, and grants again later gets no backup at
    // all -- exactly the case the backup exists for.
    std::error_code ec;
    if (!std::filesystem::exists(m_savePath, ec) || ec) return;

    // From here on there was a real file to copy, so the attempt is spent
    // whether the copy succeeded or failed.
    m_backupAttempted = true;

    const std::string dest = m_savePath + ".bak-" + TimestampSuffix();
    std::filesystem::copy_file(
        m_savePath, dest, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        // Not fatal: the player asked for the grant, and refusing it because
        // a backup failed would be a worse surprise than the missing backup.
        std::cerr << "[WARN] could not back up save file \"" << m_savePath
                  << "\" to \"" << dest << "\": " << ec.message() << "\n";
    } else {
        std::cout << "[INFO] save backed up to " << dest << "\n";
    }
}

void GrantSystem::ApplyPending(uint8_t* ewram, size_t ewramSize) {
    // Acquire: pairs with Request()'s release fetch_or (see there), so once
    // this observes a bit some Request*() call set, everything that caller
    // sequenced before raising that bit -- in particular RequestSetLevel's
    // store to m_targetLevel below -- is guaranteed visible here too.
    uint32_t pending = m_pending.load(std::memory_order_acquire);
    if (pending == 0) return;
    if (!ewram || ewramSize < kMinEwramSize) return;
    // Keep the request queued and try again next frame rather than writing
    // into a block that isn't the save block yet.
    if (!PlayerBlockPlausible(ewram, ewramSize)) return;

    EnsureBackupOnce();

    // Claim exactly the bits observed above; a Request that arrives from the
    // UI thread in between stays pending for the next frame.
    m_pending.fetch_and(~pending, std::memory_order_relaxed);

    const auto wanted = [pending](GrantKind kind) {
        return (pending & (1u << static_cast<int>(kind))) != 0;
    };

    if (wanted(GrantKind::BulletSouls)) {
        GrantSoulRange(ewram, ewramSize, kRedSoulInventory, kRedSoulCount);
    }
    if (wanted(GrantKind::GuardianSouls)) {
        GrantSoulRange(ewram, ewramSize, kBlueSoulInventory, kBlueSoulCount);
    }
    if (wanted(GrantKind::EnchantSouls)) {
        GrantSoulRange(ewram, ewramSize, kYellowSoulInventory, kYellowSoulCount);
    }
    if (wanted(GrantKind::AbilitySouls)) {
        GrantSoulRange(ewram, ewramSize, kAbilitySoulInventory, kAbilitySoulCount);
        // Literal transcription of the give-all's follow-up,
        // third_party/cvaos/src/code_08038A38.c:465-470: sub_08032ADC(0, 1)
        // through sub_08032ADC(5, 1), i.e. enable ability souls 0-5. Held at
        // 0..5 to match the game exactly -- the reset path clears 0..7, but
        // the game never *sets* 6 or 7 here and neither do we.
        for (int bit = 0; bit <= 5; ++bit) {
            SetAbilitySoulEnabledBit(ewram, ewramSize, bit);
        }
    }
    if (wanted(GrantKind::Weapons)) {
        GrantItemRange(ewram, ewramSize, kWeaponInventory, kWeaponCount);
    }
    if (wanted(GrantKind::ArmorAndAccessories)) {
        GrantItemRange(ewram, ewramSize, kArmorInventory, kArmorCount);
    }
    if (wanted(GrantKind::Consumables)) {
        GrantItemRange(ewram, ewramSize, kItemInventory, kItemCount);
    }
    if (wanted(GrantKind::RevealMap)) {
        RevealMap(ewram, ewramSize);
    }
    if (wanted(GrantKind::LevelUpOne)) {
        LevelUpTo(ewram, ewramSize, /*target=*/0, /*oneLevelOnly=*/true);
    }
    if (wanted(GrantKind::SetLevelToTarget)) {
        // Acquire, paired with RequestSetLevel's release store -- see the
        // comments at both of those sites and at the m_pending acquire-load
        // above for the full chain of guarantees this belongs to.
        LevelUpTo(ewram, ewramSize,
                  m_targetLevel.load(std::memory_order_acquire),
                  /*oneLevelOnly=*/false);
    }
}

// Literal transcription of sub_08033CAC's per-level body
// (third_party/cvaos/src/code_08033CAC.c:81-94): additive growth, not a
// recompute from a table, and the growth-table index uses currentLevel
// BEFORE it is incremented -- recomputed fresh on every loop iteration so a
// multi-level grant does not shift every level band by one (see the boundary
// crossed at level 5 in the "1 -> 10 equals nine successive +1s" test).
void GrantSystem::LevelUpTo(uint8_t* ewram, size_t ewramSize, int target,
                            bool oneLevelOnly) {
    if (!GrowthTablesReady()) return;
    if (!Fits(ewramSize, kPlayerCurrentLevel, 1)) return;

    const int currentLevel = ewram[kPlayerCurrentLevel];
    int effectiveTarget = oneLevelOnly ? currentLevel + 1 : target;
    // Hard cap at 99, independent of whatever a hand-edited config file (or,
    // for the +1 row, a currentLevel already at the cap) asked for.
    if (effectiveTarget > kMaxLevel) effectiveTarget = kMaxLevel;

    // Never level down, and never touch anything -- not even
    // currentExperience -- for a no-op request.
    if (effectiveTarget <= currentLevel) return;

    if (!Fits(ewramSize, kPlayerMaxHP, sizeof(uint16_t)) ||
        !Fits(ewramSize, kPlayerMaxMP, sizeof(uint16_t)) ||
        !Fits(ewramSize, kPlayerBaseStats, 4 * sizeof(uint16_t)) ||
        !Fits(ewramSize, kPlayerCurrentExperience, sizeof(uint32_t))) {
        return;
    }

    uint16_t maxHp = ReadU16(ewram + kPlayerMaxHP);
    uint16_t maxMp = ReadU16(ewram + kPlayerMaxMP);
    uint16_t str = ReadU16(ewram + kPlayerBaseStats + 0 * sizeof(uint16_t));
    uint16_t con = ReadU16(ewram + kPlayerBaseStats + 1 * sizeof(uint16_t));
    uint16_t intStat = ReadU16(ewram + kPlayerBaseStats + 2 * sizeof(uint16_t));
    uint16_t lck = ReadU16(ewram + kPlayerBaseStats + 3 * sizeof(uint16_t));

    int level = currentLevel;
    while (level < effectiveTarget && level < kMaxLevel) {
        const size_t idx = static_cast<size_t>(level) / 5; // level < 99 => idx < 20
        maxHp = static_cast<uint16_t>(maxHp + 12);
        maxMp = static_cast<uint16_t>(maxMp + m_mpGrowth[idx]);
        str = static_cast<uint16_t>(str + m_strGrowth[idx]);
        con = static_cast<uint16_t>(con + m_conGrowth[idx]);
        if (level & 3) {
            intStat = static_cast<uint16_t>(intStat + m_intGrowth[idx]);
        } else {
            intStat = static_cast<uint16_t>(intStat + 1);
        }
        lck = static_cast<uint16_t>(lck + 1);
        level += 1;
    }

    ewram[kPlayerCurrentLevel] = static_cast<uint8_t>(level);
    WriteU16(ewram + kPlayerMaxHP, maxHp);
    WriteU16(ewram + kPlayerMaxMP, maxMp);
    WriteU16(ewram + kPlayerBaseStats + 0 * sizeof(uint16_t), str);
    WriteU16(ewram + kPlayerBaseStats + 1 * sizeof(uint16_t), con);
    WriteU16(ewram + kPlayerBaseStats + 2 * sizeof(uint16_t), intStat);
    WriteU16(ewram + kPlayerBaseStats + 3 * sizeof(uint16_t), lck);

    // Raise currentExperience to at least the new level's threshold so the
    // in-game "EXP to next level" readout stays sane, but never lower a
    // player who already had more (code_08033CAC.c:76's own comparison runs
    // the other direction: it checks experience against the threshold, it
    // never reduces experience).
    const uint32_t exp = ReadU32(ewram + kPlayerCurrentExperience);
    const uint32_t threshold = ExpThresholdForLevel(level);
    if (threshold > exp) {
        WriteU32(ewram + kPlayerCurrentExperience, threshold);
    }
}

void GrantSystem::RevealMap(uint8_t* ewram, size_t ewramSize) {
    if (m_roomTable.size() < kRoomTableCells) return;

    for (int y = 0; y < kRoomTableHeight; ++y) {
        for (int x = 0; x < kRoomTableWidth; ++x) {
            if (m_roomTable[static_cast<size_t>(x) +
                            (static_cast<size_t>(y) << 6)] == kNoRoom) {
                continue;
            }
            const uint32_t element =
                kMapGrid + (static_cast<uint32_t>(x >> 5) * kMapGridRows +
                            static_cast<uint32_t>(y)) *
                               kMapGridElementSize;
            if (!Fits(ewramSize, element, kMapGridElementSize)) continue;
            const uint32_t bit = 1u << (x & 31);
            // +0 is the visited plane, +4 the revealed plane; the game sets
            // both when the player enters a room
            // (third_party/cvaos/src/code_080109F4.c:683-684).
            WriteU32(ewram + element, ReadU32(ewram + element) | bit);
            WriteU32(ewram + element + 4, ReadU32(ewram + element + 4) | bit);
        }
    }
}

} // namespace aria::gameplay
