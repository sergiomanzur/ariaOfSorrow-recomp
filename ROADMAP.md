# AriaRecomp Development Roadmap & Burndown

This roadmap outlines the prioritized phases (P0 to P10) for building the definitive PC recompilation of **Castlevania: Aria of Sorrow (USA)**.

---

## Priority Tracking Summary

| Priority | Feature / Subsystem | Status |
|---|---|---|
| **P0** | **Build System, ROM Validation & GBARecomp Core Linkage** | 🟢 Complete |
| **P1** | **Full-Game Compatibility & Faithfulness** | 🟢 Complete |
| **P2** | **PC Platform Frontend, Overlay Menu & Modern Input** | 🟢 Complete |
| **P3** | **Save States & Deterministic 10s Rewind** | 🟢 Complete |
| **P4** | **Adaptive Widescreen & High-Refresh Interpolation** | 🟢 Complete |
| **P5** | **Game-Aware Cheats & Quality-of-Life Enhancements** | 🟢 Complete |
| **P6** | **Modding Engine & Mod Manifests** | 🟡 In Progress |
| **P7** | **Modular HD Graphics Replacement Pipeline** | ⚪ Pending |
| **P8** | **Enhanced Audio & Replacement Soundtrack Packs** | ⚪ Pending |
| **P9** | **Steam Deck Optimization, Achievements & Polish** | ⚪ Pending |
| **P10**| **Android ARM64 Architecture & Touch Overlay** | ⚪ Future Target |

---

## Detailed Task Breakdown

### P0 — Build & Core Recompilation Correctness
- [x] Create project documentation (`AGENTS.md`, `ARCHITECTURE.md`, `ROADMAP.md`).
- [x] Configure `.gitignore` to prevent ROM, BIOS, and generated asset leaks.
- [x] Implement cryptographic ROM validator (`rom_validator.cpp`) for USA SHA-1 `abd71fe01ebb201bcc133074db1dd8c5253776c7`.
- [x] Establish centralized typed configuration system (`config_system.cpp`).
- [x] Set up CMake build system with cross-platform targets (`aria_recomp`, `test_suite`).
- [x] Integrate GBARecomp symbol mapping table and reverse-engineered structures from `cvaos`.
- [x] Complete Phase 1 static recompilation ingestion: ingested 244 function seeds, reducing runtime misses to 0 and auto-resolving 46 jump tables.
- [x] Complete Phase 2 static recompilation formalization: modeled all 46 jump tables (762 entries), eliminated m4a 0x080DA558 overlap warning, verified 0 auto jump tables and 0 compiler warnings.
- [x] Complete Phase 3 static recompilation sweeps: enabled `static_resume_all` (92,268 interior resume aliases), added 4 in-game function roots and 47th jump table, verified 0 misses across Soma, Julius, Boss Rush, and Audio sweeps.
- [x] Establish basic ROM boot cycle, interrupt dispatch, and PPU scanline sync.

### P1 — Full-Game Compatibility
- [x] Verify cartridge backup memory (SRAM / Flash 64K) save/load compatibility.
- [x] Validate standard playthrough from Start to Chaos ending.
- [x] Validate Hard Mode completion.
- [x] Validate Julius Mode completion.
- [x] Validate Boss Rush Mode and reward handling.
- [x] Validate New Game+ progression and item carryover.
- [x] Verify all 3 endings (Bad ending, Good ending, True Chaos ending).

### P2 — PC Platform, UI & Action Input System
- [x] Setup modern windowing frontend with SDL2/SDL3 backend.
- [x] Implement action-based input mapper (`action_system.cpp`) for Keyboard & Gamepad (Attack, Jump, Soul, Backdash, Start, Select).
- [x] Implement controller glyph auto-detection and custom action bindings.
- [x] Implement hotkey triggers (Quick Save, Quick Load, Rewind, Turbo).
- [x] Add display scaling modes (Authentic 3:2 at 960×640 4× integer scale, 16:9 Widescreen, Fullscreen, Borderless).

### P3 — Save States & Deterministic Rewind
- [x] Implement atomic save state manager with SHA-1 validation and state integrity.
- [x] Implement 10-second deterministic ring buffer rewind engine (600 frames at 60 Hz).
- [x] Add state restoration verification ensuring zero state corruption.

### P4 — Adaptive Widescreen & High-Refresh Presentation
- [x] Implement authentic 3:2 Native presentation (240×160 GBA geometry) and selectable 16:9 view (284×160 logical view).
- [x] Expose View Mode toggle in recomp-ui overlay menu.
- [x] Implement high-refresh display rendering decoupled from 60 Hz core logic.
- [x] Add GBA color correction filters and gamma modes.
- [x] Implement True Adaptive Widescreen (`adaptive_widescreen.cpp`): dynamic horizontal room camera expansion via `cvaos` room boundary decoding (`bgInfo[1]`, `pBgMetadata`, metatiles), revealing additional room geometry seamlessly on multi-screen rooms with automatic fallback to 240×160 pillarbox for single-screen rooms, boss arenas, door transitions, and cutscenes.
- [x] Implement Authentic Display Filters (`display_filters.cpp`): Whole-integer scaling (1×–5×), GBA LCD subpixel grid, AGS-001 frontlit simulation, AGS-101 backlit simulation, and CRT aperture grille scanline filters.
- [x] Implement HD UI & Vectorized Typography (`hd_ui_system.cpp`): Hook dialogue and menu string output (`BgCmdBuffer_WriteString` at `0x0800125C` & `0x0800148C`) to render crisp, high-resolution vector text overlays with translucent dialog boxes while preserving original sprite portraits.

### P5 — Game-Aware Cheats & Quality-of-Life Subsystems
- [x] Hook into `cvaos` symbol addresses for Player stats, Souls, Inventory, and RNG.
- [x] Implement cheat suite (`cheat_system.cpp`): Infinite HP/MP/Hearts, Invincibility, One-hit kills, EXP multiplier, Guaranteed Soul drops.
- [x] Implement Modern Gameplay & Controls suite (`qol_system.cpp`):
  - **Quick Soul / Weapon Loadouts ("Dawn of Sorrow" Fix)**: Instant 3-slot loadout switching via `L2/LT`, `Q` hotkey, or `L + Select` GBA combo without pausing.
  - **Infamous Luck Stat Bug Fix**: Mathematically corrected drop scaling fixing retail GBA division-underflow bug where Luck worsened drop odds.
  - **Soul Drop Pity Counter**: Deterministic dry-kill tracking guaranteeing a soul drop on the configured threshold kill.
  - **Transparent Mini-Map HUD**: Real-time non-intrusive castle map overlay with configurable opacity and player blip (`M` key toggle).
  - **Enemy Soul Indicators**: Real-time HUD badge displaying enemy soul possession count (`1/9` or `0/9 Uncollected`).
  - **Fast Room Transitions**: Bypasses 16-frame screen fade waits for instant, fluid castle traversal.
  - **Fast Text & Fast-Forward**: Instant typewriter display and turbo cutscene progression.
  - **In-Game Settings Menu Integration**: 9 toggleable options exposed in the `Gameplay` section of the Esc overlay menu and saved to `aria_config.ini`.

### P6 — Modding Subsystem
- [ ] Implement mod manifest format (`mod.json`) with dependencies and versioning.
- [ ] Implement mod asset override virtual filesystem.
- [ ] Implement safe hook dispatch for game event callbacks.
- [ ] Add mod state serialization for save states.

### P7 — HD Asset Replacement Pipeline
- [ ] Build automated ROM asset extractor and hash database generator.
- [ ] Implement runtime texture replacement engine for sprites, backgrounds, UI, and fonts.
- [ ] Support modular HD packs with custom atlas alignment and pivot preservation.

### P8 — Enhanced Audio & Replacement Soundtracks
- [ ] Implement host-side audio mixer with high-quality cubic/sinc interpolation.
- [ ] Build replacement soundtrack engine supporting high-fidelity FLAC/OGG tracks with loop point metadata.
- [ ] Provide independent volume channels for Master, BGM, and SFX.

### P9 — Polish, Accessibility & Steam Deck Optimization
- [ ] Steam Deck controller profiles and UI font scaling.
- [ ] Accessibility features: reduced flashing, screen shake toggle, high contrast modes.
- [ ] Performance profiling and crash dump diagnostics.

### P10 — Android ARM64 Target
- [ ] Android NDK build configuration.
- [ ] Virtual touch overlay and Android gamepad support.
- [ ] Scoped storage save management.
