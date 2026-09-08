# AriaRecomp

A native PC port of **Castlevania: Aria of Sorrow (USA)** built by **static recompilation**: instead of emulating a Game Boy Advance at runtime, a tool translates the game's own ARM7TDMI machine code, function by function, into native C++ once, ahead of time. The result links into an ordinary executable that runs the game directly on your CPU — no CPU emulation loop, no BIOS to dump, no ROM bytes embedded anywhere in this repository.

**This project is almost entirely AI-driven.** The recompilation pipeline, the runtime engine, the in-game overlay, and every fix and feature described below were built and debugged by an AI coding agent (Claude, via Claude Code) working from the original ROM's reverse-engineered symbol map, with a human reviewing and directing the work. If you're inspecting this code expecting a hand-written emulator project, know that going in.

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
| **Unrecognized code paths** | N/A — a general-purpose CPU core handles everything uniformly | A small, shrinking set of not-yet-statically-recompiled functions bridge through a reference interpreter automatically ("self-healing"), and are logged for promotion to native code — the runtime reports exactly how much of the game is running natively vs. bridged |
| **Frame timing** | Usually paced to the GBA's real 59.7275 Hz | Paced to the GBA's real 59.7275 Hz against the actual VBlank hardware event rather than a fixed-duration tick — an early build of this project got this wrong (see the two fixes in the `third_party/gbarecomp` fork this project depends on) and ran ~27% fast with torn raster effects until it was found and corrected |
| **BIOS** | Most emulators require you to supply Nintendo's real BIOS dump (or ship a legally separate HLE BIOS) | Synthesizes its own minimal cleanroom BIOS automatically on first run — nothing to source separately |
| **Settings & assist tools** | A separate frontend/core split (RetroArch cores, standalone emulator UIs) | A single native executable with its own in-game overlay (Esc to open): display, audio, save states, adaptive widescreen, rewind, fast-forward |
| **Extending the game** | Cheats/mods usually go through Game Genie–style codes or Lua scripting bolted onto the core | Cheats and quality-of-life systems are ordinary C++ reading and writing the game's own memory layout directly (`src/gameplay/`) |
| **Distribution model** | One emulator binary + your ROM file, kept separate | One executable that becomes *your* copy of the game once you point it at your own ROM once |

Recompilation is a different engineering approach from emulation, not a strictly "better" one for every player — but it does mean the game runs as native, ahead-of-time-compiled code rather than as a runtime translation of GBA instructions.

---

## Current feature status

**Working today, verified during development:**
- Full boot-to-gameplay static recompilation, self-healing coverage reporting, zero dispatch misses observed in testing.
- In-game settings overlay (Esc) — Display, Graphics, Audio, System, Assist Tools sections.
- Save states (10 slots, F1–F9 load / Shift+F1–F9 save) and a configurable rewind buffer.
- Fast-forward, adaptive 16:9/16:10/21:9 widescreen, authentic 240×160 mode, color-correction filters.
- ROM picker on first launch (native file dialog) with the choice persisted next to the executable, so it isn't re-asked on later launches — including from a shortcut, Steam, or any other launcher, not just a plain double-click.
- Modern action-based input remapping (keyboard + gamepad) with automatic controller-glyph detection.
- Cheats: **Infinite HP, Infinite MP, and Infinite Hearts** are live and modify the running game's memory in real time.

**Scaffolded but not yet wired to real behavior** (visible in `aria_config.ini` / the overlay's Cheats and Quality of Life sections as clearly disabled rows, not silent no-ops):
- Additional cheats: invincibility, one-hit kill, EXP multiplier, guaranteed soul drops, unlock-all souls/items/map.
- Quality-of-life: fast text, fast door transitions, skip seen cutscenes, quick retry, soul-drop pity system.
- Modding subsystem, HD asset replacement, replacement soundtrack packs, Steam Deck-specific polish, Android target.

The project deliberately never presents a toggle that looks live but does nothing — an unimplemented row shows as disabled rather than lying about what it does.

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
cmake --build build --target test_rom_validator test_config test_action_system test_cheats_qol
ctest --test-dir build -R "RomValidatorTest|ConfigSystemTest|ActionSystemTest|CheatsQolTest"

python tests/test_vblank_sync.py
python tests/test_rom_picker_paths.py
```

(`test_vblank_sync.py` and `test_rom_picker_paths.py` need `aria_recomp.exe` in the repo root, next to a copy of the ROM — copy the built executable there, or point `EXE`/`INSTALL_DIR` at your build output.)

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
