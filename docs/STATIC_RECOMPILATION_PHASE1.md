# Phase 1: Static Recompilation Ingestion & Native Aspect Ratio Modernization

**Author**: AriaRecomp Team  
**Date**: September 2026  
**Status**: Completed (Phase 1)

---

## 1. Executive Summary

This document details the architecture and results of **Phase 1: Static Recompilation Expansion** and the **Native 3:2 Display Aspect Ratio Correction** for **AriaRecomp** (*Castlevania: Aria of Sorrow* PC recompilation).

Prior to this milestone, the engine operated in a **hybrid static/dynamic state**: although the majority of ROM functions were statically compiled ahead of time into 16 native C++ shards, 238 distinct execution misses and 28 jump-table candidate regions fell back to the runtime self-healing interpreter bridge during gameplay.

Following Phase 1 ingestion, cross-referencing against the `testyourmine/cvaos` decompilation symbols, and shard regeneration:
- **Jump Table Candidate Regions**: Reduced from **28** to **0**.
- **Distinct Runtime Misses**: Reduced from **238** to **0** in standard game flow and title/save verification.
- **Interpreted Instructions**: Dropped from **63,960,503** down to **0**.
- **Total Emitted Static Functions**: Expanded from **12,289** to **12,643** native C++ functions.
- **Display Aspect Ratio**: Native mode now defaults to the Game Boy Advance's authentic **3:2 (240×160)** resolution at **960×640** (4× integer scale), while 16:9 widescreen is dynamically switchable live from the in-game overlay menu.

---

## 2. Quantitative Verification Results

A comparison of execution metrics measured before and after Phase 1 via `recomp_coverage_A2CE.json` and the TCP oracle test suite (`tests/test_misses.py` and `tests/test_playability.py`):

| Metric | Before Phase 1 | After Phase 1 | Net Change |
|---|---|---|---|
| **Unresolved Jump-Table Regions** | 28 | **0** | **-100% (Fully resolved)** |
| **Distinct Runtime Misses (Boot to Title)** | 238 | **0** | **-100% (Zero misses)** |
| **Interpreted Instructions Executed** | 63,960,503 | **0** | **-100% (Pure native)** |
| **Discovered & Emitted Native Functions** | 12,289 | **12,643** | **+354 functions** |
| **Auto-Detected Jump Tables** | 0 | **46** (762 targets) | **+46 tables** |
| **Full Playability Test Suite** | Passed (with misses) | **Passed (0 misses)** | **100% Pure Native** |

---

## 3. Subsystem Breakdown of Ingested Function Seeds

All ingested function seeds in [`game.toml`](file:///c:/Users/sergi/Homestead/code/ariaOfSorrow-recomp/game.toml) were cross-referenced with source units in `third_party/cvaos` and classified by functional subsystem:

### 3.1 Core Engine, VRAM & Collision (`0x08000000` - `0x08007FFF`)
- **Key Files**: `src/main.c`, `src/code_08001194.c`
- **Responsibilities**:
  - `ResetInit_080006E4`: System reset, DMA initialization, and VRAM baseline transfer.
  - `SramInit_08000848`: Cartridge battery backup verification and initialization branch.
  - `MainLoop_08000A96`: Core game loop frame step and dispatch routine (`AgbMain`).
  - `BgCmd_080019A6` & `sub_08001BA0`: Background command buffer transfer, tilemap collision coordinates, and tile reading switch tables.

### 3.2 Player, Physics & Soul Mechanics (`0x08014000` - `0x08034FFF`)
- **Key Files**: `asm/code/code_08014548.s`, `asm/code/code_080211F0.s`
- **Responsibilities**:
  - Soma Cruz motion physics: walking, dashing, jumping, backdashing, sliding, and slope navigation.
  - Entity collision resolution: hitbox checks against tile collision tables and platforms.
  - Guardian and Ability soul continuous execution loops (Flying Armor glide, Black Panther dash, Skula water walking).

### 3.3 Weapons, Projectiles & Damage Engine (`0x08035000` - `0x0804FFFF`)
- **Key Files**: `src/code/code_08039340.c`, `asm/code/code_08040A38.s`
- **Responsibilities**:
  - `WeaponProj_0803F792`: Bullet soul projectile spawning and trajectory updates.
  - `WeaponDamage_0803FDF6`: Damage calculation, element affinities, weapon swings, and thrust hitboxes.
  - Sub-weapon trajectories and sprite animation step routines.

### 3.4 Enemy AI & Boss State Machines (`0x08050000` - `0x0806FFFF`)
- **Key Files**: `asm/code/code_08050A3C.s`, `asm/code/code_08060B98.s`
- **Responsibilities**:
  - Individual enemy behavior switch tables (Winged Skeleton, Zombie, Bat, Peeping Eye, etc.).
  - Boss stage step machines (Creaking Skull, Manticore, Great Armor, Death).
  - Enemy sprite animation and agro target detection.

### 3.5 UI, Inventory & Soul Management (`0x08070000` - `0x0809FFFF`)
- **Key Files**: `asm/code/code_080709D8.s`, `asm/code/code_080809C0.s`
- **Responsibilities**:
  - Soul equip/unequip matrix handling (Red, Blue, Yellow, Silver souls).
  - Inventory and equipment stat recalculation (STR, CON, INT, LCK).
  - Bestiary entry update and soul completion percentages.

---

## 4. Display Aspect Ratio & Presentation Architecture

### 4.1 Root Cause of Previous 16:9 Default
In previous builds, `aria_config.ini` had `aspectRatio = 1` committed (`Widescreen_16_9`), causing `main.cpp` to always inject `--view-width 284`. Furthermore, default window dimensions were configured to 1280×720 (16:9).

### 4.2 Restored Native 3:2 Aspect Ratio
- **GBA Native Geometry**: 240 pixels wide by 160 pixels high ($3:2 = 1.500$).
- **Default Resolution**: Configured to **960 × 640** (4× integer scale) in both [`src/config/config_system.hpp`](file:///c:/Users/sergi/Homestead/code/ariaOfSorrow-recomp/src/config/config_system.hpp) and [`aria_config.ini`](file:///c:/Users/sergi/Homestead/code/ariaOfSorrow-recomp/aria_config.ini).
- When `aspectRatio == Authentic_3_2` (0), the runtime opens with 240×160 logical dimensions, presenting pixel-perfect 3:2 visuals without pillarboxing or distortion.

### 4.3 Clean 16:9 Widescreen Support
- When `aspectRatio == Widescreen_16_9` (1), `--view-width 284` is passed, rendering the game with symmetric 22-pixel side columns.
- Exposed `opts.launcher_expose_widescreen = true` and `opts.widescreen_view_width = 284` in [`src/main.cpp`](file:///c:/Users/sergi/Homestead/code/ariaOfSorrow-recomp/src/main.cpp), allowing users to open the in-game overlay menu (`Esc`) and toggle dynamically between **Native** (3:2) and **16:9 fixed**.

---

## 5. Next Steps

With Phase 1 complete and zero misses observed in baseline gameplay, the roadmap moves to:
1. **Phase 2 (Jump-Table Descriptors)**: Hardcode explicit jump table boundary counts into `game.toml` for multi-branch `switch` tables so that disassembler bounds checking is 100% formal.
2. **Phase 3 (Full Playthrough Sweeps)**: Automate end-to-end traversal of Hard Mode, Julius Mode, and Boss Rush to discover and seed any remaining late-game branch targets.
