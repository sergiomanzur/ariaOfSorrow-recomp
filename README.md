# AriaRecomp

A native PC port of **Castlevania: Aria of Sorrow (USA)** built by **static recompilation**: instead of emulating a Game Boy Advance at runtime, a tool translates the game's own ARM7TDMI machine code, function by function, into native C++ once, ahead of time. The result links into an ordinary executable that runs the game directly on your CPU — no BIOS to dump, no ROM bytes embedded anywhere in this repository.

**Recompilation Status**: Full static ahead-of-time (AOT) recompilation has been completed and verified across all gameplay modes (Soma Campaign, Julius Belmont Mode, Boss Rush Mode, and Endings/Staff Roll). Runtime misses and interpreted instructions have been driven to **zero** (0). See [Recompilation coverage](#recompilation-coverage) for the full measurements and phase breakdown.

**This project is almost entirely AI-driven.** The recompilation pipeline, the runtime engine, the in-game overlay, and every fix and feature described below were built and debugged by an AI coding agent working from the original ROM's reverse-engineered symbol map, with a human reviewing and directing the work.

---

## Legal — read this first

- **You must supply your own legally-dumped copy** of *Castlevania: Aria of Sorrow (USA)*. This repository contains no Nintendo or Konami copyrighted material: no ROM, no BIOS image, no save data, no screenshots of in-game content.
- The BIOS this project boots against is a **cleanroom stub** synthesized at first launch (`src/main.cpp`) — a handful of original ARM instructions implementing reset/SWI-return/IRQ vectors, nothing derived from Nintendo's firmware. Everything else (`Halt`, `IntrWait`, `VBlankIntrWait`, memory copy/decompression routines, sound engine helpers) is a clean-room HLE (high-level emulation) reimplementation in `third_party/gbarecomp`.
- **The core emulation engine (`third_party/gbarecomp`) is licensed [PolyForm Noncommercial 1.0.0](https://polyformproject.org/licenses/noncommercial/1.0.0)** by its author, Matthew Stan. Because AriaRecomp links directly against it, **this project can only be used and distributed noncommercially** — you cannot sell it, bundle it with a paid product, or otherwise use it for commercial advantage. `recomp-ui` (the settings overlay) is MIT-licensed; `cvaos` (the reverse-engineered symbol map) is MIT-licensed. See each submodule's own `LICENSE`.
- The game's own copyright remains with Konami. AriaRecomp is a fan-made engineering project, not affiliated with or endorsed by Konami.

---

## What makes this different from playing on an emulator

| | Emulator | AriaRecomp |
|---|---|---|
| **Execution model** | Interprets or JIT-translates GBA ARM7TDMI instructions every time they run | Game code is translated to native C++ once, ahead of time, and compiled directly into the executable — no per-instruction translation overhead at runtime |
| **Unrecognized code paths** | N/A — a general-purpose CPU core handles everything uniformly | **Pure Static Execution**: All 12,736 reachable functions and 47 dispatch tables are statically compiled with 92,268 interior resume points. Runtime misses and interpreted instructions: **0** |
| **Frame timing** | Usually paced to the GBA's real 59.7275 Hz | Paced to the GBA's real 59.7275 Hz against the actual VBlank hardware event rather than a fixed-duration tick — an early build of this project got this wrong (see the two fixes in the `third_party/gbarecomp` fork this project depends on) and ran ~27% fast with torn raster effects until it was found and corrected |
| **BIOS** | Most emulators require you to supply Nintendo's real BIOS dump (or ship a legally separate HLE BIOS) | Synthesizes its own minimal cleanroom BIOS automatically on first run — nothing to source separately |
| **Settings & assist tools** | A separate frontend/core split (RetroArch cores, standalone emulator UIs) | A single native executable with its own in-game overlay (Esc to open): display, audio, save states, adaptive widescreen, rewind, fast-forward |
| **Extending the game** | Cheats/mods usually go through Game Genie–style codes or Lua scripting bolted onto the core | Cheats and quality-of-life systems are ordinary C++ reading and writing the game's own memory layout directly (`src/gameplay/`) |
| **Distribution model** | One emulator binary + your ROM file, kept separate | One executable that becomes *your* copy of the game once you point it at your own ROM once |

Recompilation is a different engineering approach from emulation, not a strictly "better" one for every player — but it does mean the game runs as native, ahead-of-time-compiled code rather than as a runtime translation of GBA instructions.

---

## Current feature status

**Working today, verified during development:**
- **Full Static Recompilation**: 12,736 emitted C++ functions, 47 formalized jump tables, 92,268 static interior resume points, **0 distinct runtime misses**, and **0 interpreted instructions**.
- **Display & Aspect Ratio**: Authentic Game Boy Advance **3:2 (240×160)** resolution rendered by default at **960×640** (4× integer scale), plus selectable 16:9 widescreen view (284×160 logical) with live switching via the in-game overlay (`Esc`).
- **True Adaptive Widescreen**: Dynamic horizontal camera expansion on multi-screen rooms (`+22px` each side) without pillarboxes or pixel stretching, backed by `cvaos` room boundary decoding and metatile lookups, with clean automatic fallback for single-screen rooms, boss fights, cutscenes, and door transitions.
- **Authentic Display Filters**: Whole-integer scaling (1×–5×), GBA LCD subpixel grid, AGS-001 Frontlit, AGS-101 Backlit, and CRT Aperture Grille scanline filters.
- **HD Atmospheric Background Pipeline**: High-definition painted Gothic Castlevania scenery (the iconic eclipsed moon with glowing celestial corona, Dracula's Castle spires, and dark storm clouds) with real-time parallax scrolling based on EWRAM camera position, seamless 16:9 widescreen border integration, and an instant on/off toggle via in-game settings (`Graphics -> HD Atmospheric Background`), `B` hotkey, or config (`hd_backgrounds = true/false`), strictly preserving faithful native GBA backgrounds by default (Prime Directive 1).
- **HD UI & Vectorized Typography**: Crisp anti-aliased vector fonts for dialogue and menus via ImGui, paired with elegant translucent text boxes while preserving original sprite portraits.
- **Quick Soul / Weapon Loadout Presets ("Dawn of Sorrow" Fix)**: Instant 3-slot loadout switching via `L2/LT`, `Q` hotkey, or `L + Select` without pausing.
- **Luck Stat Bug Fix**: Mathematically corrected drop rate formula resolving retail GBA division-underflow bug where Luck penalized drops.
- **Soul Drop Pity Counter**: Deterministic dry-kill tracking guaranteeing a soul drop after a configurable number of kills per enemy.
- **Transparent Mini-Map & Soul Indicators**: Non-intrusive in-game mini-map HUD with adjustable opacity (`M` key toggle) and live enemy soul possession indicators (`1/9`).
- **Fast Room Transitions & Fast Text**: Bypasses 16-frame screen fade waits for instant room traversal, with instant typewriter text delivery.
- In-game settings overlay (`Esc`) — Display, Graphics, Audio, System, Gameplay, and Assist Tools sections.
- Save states (10 slots, F1–F9 load / Shift+F1–F9 save) and a configurable rewind buffer.
- Fast-forward, authentic pixel grid, and color-correction filters.
- ROM picker on first launch (native file dialog) with the choice persisted next to the executable.
- Modern action-based input remapping (keyboard + gamepad) with automatic controller-glyph detection.
- Cheats: **Infinite HP**, **Infinite MP**, **Infinite Hearts**, **Invincibility**, **One-Hit Kill**, **EXP Multiplier (1×–16×)**, and **Guaranteed Soul Drops**.
- One-shot grants: all bullet / guardian / enchant / ability souls, all weapons, all armor and accessories, all consumables, and a full map reveal (with automated timestamped backup saves).

**Scaffolded for future phases**:
- Modding subsystem (P6), HD asset replacement (P7), replacement soundtrack packs (P8), Steam Deck-specific polish (P9), Android target (P10).

---

## Recompilation coverage

Static recompilation has progressed from a hybrid interpreter-bridged state to **100% pure static execution**:

| Metric | Baseline | Phase 1 (Miss Ingestion) | Phase 2 (Jump Tables) | Phase 3 (Playthrough Sweeps) |
|---|---|---|---|---|
| **Discovered Native Functions** | 12,289 | 12,643 | 12,655 | **12,736** |
| **Static Interior Resume Aliases** | 0 | 0 | 0 | **92,268** |
| **Formalized Jump Tables** | 0 | 0 | 46 (762 targets) | **47 (768 targets)** |
| **Auto Jump Tables** | 0 | 46 (unbounded) | 0 | **0** |
| **Data Ranges Honored** | 0 | 0 | 46 | **47** |
| **Recompiler Warnings** | 1 | 1 | 0 | **0** |
| **Runtime Misses (All Modes)** | 238 | 0 (Boot/Title) | 0 (Boot/Title) | **0 (Full Playthrough)** |
| **Interpreted Instructions Executed** | 63,960,503 | 0 | 0 | **0 (Pure Native)** |
| **Native Dispatch Calls** | N/A | 1,817 | 1,817 | **89,043** |

Detailed documentation of each phase:
- [docs/STATIC_RECOMPILATION_PHASE1.md](docs/STATIC_RECOMPILATION_PHASE1.md): Miss ingestion and native 3:2 aspect ratio restoration.
- [docs/STATIC_RECOMPILATION_PHASE2.md](docs/STATIC_RECOMPILATION_PHASE2.md): Jump table formalization and m4a control-flow warning elimination.
- [docs/STATIC_RECOMPILATION_PHASE3.md](docs/STATIC_RECOMPILATION_PHASE3.md): Multi-mode playthrough sweeps (Soma, Julius, Boss Rush, Endings) and AOT static resume.

---

## Building

### Prerequisites
- CMake ≥ 3.20
- A C++20 compiler (this project is developed against MinGW-w64 g++ on Windows; MSVC and Linux/macOS toolchains should work but are less exercised)
- [Ninja](https://ninja-build.org/) (recommended) or another CMake-supported generator
- SDL2 development libraries
- Git (for submodules)

### Clone and build

```sh
git clone --recurse-submodules <this-repo-url> ariaOfSorrow-recomp
cd ariaOfSorrow-recomp
```

If you cloned without `--recurse-submodules`, run `git submodule update --init --recursive` before continuing.

**Recompile your own ROM.** `recomp_out/` — the native C++ translation of the game's code — is never committed to this repository: it would embed a translation of Konami's copyrighted machine code. You generate it yourself, once, from your own legally-dumped ROM:

```sh
cmake -B build -S . -G Ninja
cmake --build build --target gba_recompile

./build/third_party/gbarecomp/gba_recompile.exe \
    --rom "path\to\Castlevania - Aria of Sorrow (USA).gba" \
    --config game.toml --entry 0x080000C0 --codegen-shards 16 --out recomp_out
```

This only accepts a ROM matching the USA v1.0 revision (SHA-1 `abd71fe0…`) — `game.toml`'s symbol map is specific to that build. Then configure again (CMake needs to see the newly-populated `recomp_out/`) and build the game:

```sh
cmake -B build -S .
cmake --build build --target aria_recomp
```

Without this step, CMake configure still succeeds and `aria_core` plus its unit tests build fine — only the `aria_recomp` game executable itself is skipped, with a clear message saying why.

The BIOS this links against is a placeholder dispatch table by default (the game still boots and runs correctly — it uses the clean-room HLE path). If you want the optional BIOS low-level-emulation path for closer hardware fidelity:

```sh
# 1. Run the game once so it writes bios/cleanroom_bios.bin next to the exe
#    (a canceled ROM prompt is fine — the BIOS is written before that check).
cp build/aria_recomp.exe .
./aria_recomp.exe --no-window --frames 1

# 2. Build the recompiler tool and point it at that BIOS. It writes its
#    output relative to the current directory, so run it from inside
#    third_party/gbarecomp.
cmake --build build --target gba_recompile
(cd third_party/gbarecomp && ../../build/third_party/gbarecomp/gba_recompile.exe --bios ../../bios/cleanroom_bios.bin)

# 3. Reconfigure (picks up the new generated_bios/ sources) and rebuild.
cmake -B build -S .
cmake --build build --target aria_recomp
```

### Running the test suite

```sh
# 1. C++ Host Unit Tests (ROM validator, Config, Actions, Cheats/QoL)
cmake --build build --target test_rom_validator test_config test_action_system test_cheats_qol
ctest --test-dir build -R "RomValidatorTest|ConfigSystemTest|ActionSystemTest|CheatsQolTest"

# 2. Recompilation Coverage & Runtime Misses Oracle (asserts 0 misses, 0 interpreted insns)
python tests/test_misses.py

# 3. Full-Game Playability & Savestate Roundtrip Oracle
python tests/test_playability.py

# 4. Multi-Mode Playthrough Sweeps (Soma Campaign, Julius Mode, Boss Rush, Audio)
python tests/test_playthrough_sweeps.py

# 5. Integration and Synchronization Tests
python tests/test_vblank_sync.py
python tests/test_rom_picker_paths.py
```

(`tests/*.py` expect `aria_recomp.exe` in `build/` or the repo root, alongside `Castlevania - Aria of Sorrow (USA).gba`.)

---

## Launching

Copy the built `aria_recomp.exe` (and `SDL2.dll` on Windows) to wherever you want to keep the game.

1. **First launch:** double-click the executable, or run it from a shortcut/launcher. It creates a default `aria_config.ini` next to itself. If it can't find a ROM (via config, command line, or a `Castlevania - Aria of Sorrow (USA).gba` sitting next to it), it opens a native file picker asking you to locate your own legally-dumped ROM.
2. The ROM is verified against the known USA v1.0 SHA-1 and MD5. If it matches, the chosen path is written back to `aria_config.ini` as an absolute path.
3. **Every launch after that** boots straight into the game — no re-prompting, regardless of what directory the process was started from.
4. Press **Esc** in-game to open the settings overlay.

Command-line overrides remain available for scripted/headless use:

```sh
aria_recomp.exe --rom "path\to\your.gba" --bios "path\to\bios.bin"
aria_recomp.exe --no-window --frames 600   # headless, e.g. for CI
```

---

## Repository layout

```
src/                    AriaRecomp's own code: ROM validation, config system,
                        input remapping, cheats/QoL, main()
symbols/                Reverse-engineered memory-layout symbols for this ROM
recomp_out/             Generated at build time from your own ROM — never committed
third_party/gbarecomp/  The static-recompilation engine + runtime (submodule, fork
                        with two upstreamed-but-not-yet-merged fixes: VBlank sync
                        and wall-clock frame pacing)
third_party/recomp-ui/  The in-game settings overlay (submodule, upstream)
third_party/cvaos/      Reverse-engineering data this ROM's symbol map is built from
                        (submodule, upstream)
tests/                  C++ unit tests + Python integration/regression tests
docs/                   Design specs written during development
```

## Acknowledgments

- [gbarecomp](https://github.com/mstan/gbarecomp) by Matthew Stan — the static-recompilation engine and runtime this project is built on.
- [recomp-ui](https://github.com/mstan/recomp-ui) by Matthew Stan(ley) — the in-game settings overlay.
- [cvaos](https://github.com/testyourmine/cvaos) by testyourmine — reverse-engineering data for *Aria of Sorrow*'s memory layout.
