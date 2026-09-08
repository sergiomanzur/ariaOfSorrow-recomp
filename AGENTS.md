# AriaRecomp Agent Guidelines & Operational Protocols

Welcome to **AriaRecomp**, the native modernized PC recompilation of **Castlevania: Aria of Sorrow (USA, Game Boy Advance)**.

Every agent working on this codebase must adhere strictly to the rules and conventions defined in this document.

---

## 1. Prime Directives

1. **Faithful Core vs Optional Modernization**:
   - The project is split into two layers:
     1. **FAITHFUL CORE**: Exact execution, original 240×160 resolution, original timings, original SRAM save mechanics, cycle accuracy.
     2. **OPTIONAL MODERNIZATION**: Widescreen, 60/120/144+ Hz interpolation, rewind, save states, cheats, QoL, HD assets, enhanced audio, mods.
   - The game must always remain 100% playable and testable with all enhancements disabled.
   - Enhancements must **never** become dependencies for core game correctness.

2. **Legal and Redistribution Boundary**:
   - The repository must **never** contain copyrighted ROM data, BIOS binaries, or extracted Nintendo/Konami assets.
   - A legally obtained ROM (`cvaos_us.gba`) is provided locally by the user.
   - Validate the ROM cryptographically (SHA-1: `abd71fe01ebb201bcc133074db1dd8c5253776c7`) before applying hooks or booting.
   - Never commit generated recompilation shards or ROM-derived assets.

3. **Recompilation Integrity**:
   - Primary framework: **mstan/GBARecomp**.
   - Reference decompilation: **testyourmine/cvaos** for symbols, structures, constants, data layouts, and function signatures.
   - **Never manually edit generated recompilation shards**. If generated code is flawed, fix the underlying symbol definition, analyzer, runtime hook, or hook integration.

4. **No Guessed Addresses or Offsets**:
   - Never invent addresses, struct field offsets, or function semantics.
   - Cross-reference with `cvaos` symbols, Ghidra / IDA reverse engineering, or verified symbol tables in `symbols/`.

---

## 2. Agent Workflow Order

Before modifying code:
1. Read `AGENTS.md` (this file).
2. Read `ARCHITECTURE.md`.
3. Read `ROADMAP.md`.
4. Inspect the codebase and existing tests.
5. Check relevant GBARecomp APIs & runtime hooks.
6. Locate relevant `cvaos` symbols or structures.
7. Determine the smallest correct implementation.
8. Implement, test, and update `ROADMAP.md`.

---

## 3. Code Quality & Conventions

- **Language**: Modern C++ (C++20) for core runtime and host systems; C99/C++ for GBARecomp interfaces.
- **Ownership**: Explicit RAII ownership; prefer `std::unique_ptr` / `std::shared_ptr` where appropriate, avoid raw owning pointers.
- **Error Handling**: Explicit return types / `std::expected` / result structures; do not silently swallow errors or corrupt states.
- **Centralized Configuration**: All settings must go through `ConfigSystem` (`src/config/config_system.hpp`). No scattered global booleans or ad-hoc environment variables.
- **Platform Separation**: Abstract platform-specific code (SDL3 / Windows / Linux / Steam Deck) behind clean interfaces. No Windows API calls (`windows.h`) inside game logic or recomp core.
- **Input**: Game code consumes abstract actions (`Action::Attack`, `Action::Jump`, `Action::Soul`, etc.), never raw controller or keyboard scancodes directly.

---

## 4. Subsystem Guidelines

### Core & ROM Validation
- Always verify ROM checksum upon launch.
- If checksum fails or ROM is missing, report a clear user-friendly diagnostic and exit safely.

### Save States & Rewind
- Deterministic rewind buffer target: 10 seconds of ring-buffered state.
- Include CPU registers, IWRAM, EWRAM, VRAM, OAM, Palettes, Timers, DMA, RNG state, Sound registers, and registered mod states.
- State load failures must be atomic: if loading a corrupt or invalid state fails, the currently running frame must remain intact without partial corruption.

### Cheats & QoL
- Cheats and QoL must use stable IDs and typed APIs.
- Clearly differentiate session-only memory modifications from persistent progression changes.
- All gameplay-altering QoL enhancements must be strictly opt-in.

### Widescreen
- Adaptive widescreen must reveal actual game world (extended tilemap rendering / camera extents).
- Never stretch pixels. If a room or cutscene is not validated for extended view, fall back cleanly to 240×160 pillarboxed.

---

## 5. Priority Matrix

- **P0**: Build & Recomp correctness (ROM validation, runtime linkage, booting).
- **P1**: Full-game compatibility (All modes: Normal, Hard, Julius, Boss Rush, NG+, all endings).
- **P2**: PC platform, UI & Action-based input.
- **P3**: Save states & Deterministic 10s rewind.
- **P4**: Adaptive widescreen rendering.
- **P5**: Cheats & Quality of Life systems.
- **P6**: Modding architecture.
- **P7**: HD Asset replacement pipeline.
- **P8**: Enhanced audio & Replacement soundtrack packs.
- **P9**: Release polish & Steam Deck optimization.
- **P10**: Android ARM64 port.
