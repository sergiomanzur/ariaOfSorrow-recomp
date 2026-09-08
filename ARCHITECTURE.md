# AriaRecomp Architecture Specification

This document details the architectural design and system breakdown for **AriaRecomp**, a modernized native PC port of *Castlevania: Aria of Sorrow (USA)*.

---

## 1. High-Level Architectural Layers

```
+-------------------------------------------------------------------------+
|                              HOST APPLICATION                           |
|  +---------------------+   +---------------------+   +---------------+  |
|  | Modern ImGui / UI   |   | Input Action System |   | Audio Host    |  |
|  | - Settings Overlay  |   | - SDL3 / Gamepads   |   | - High-res Mix|  |
|  | - Debugger / Cheats |   | - Rebinding         |   | - Music Packs |  |
|  +---------------------+   +---------------------+   +---------------+  |
+-------------------------------------------------------------------------+
                                    |
+-------------------------------------------------------------------------+
|                         MODERNIZATION LAYER (P2-P9)                     |
|  +------------------+  +------------------+  +------------------------+ |
|  | Adaptive Wide-   |  | Deterministic    |  | Game-Aware Cheats &    | |
|  | screen & High-Hz |  | 10s Rewind Buffer|  | Quality of Life Engine | |
|  +------------------+  +------------------+  +------------------------+ |
|  +------------------+  +------------------+  +------------------------+ |
|  | HD Asset & Sprite|  | Mod Subsystem &  |  | Centralized Typed      | |
|  | Replacement Hook |  | State Serialize  |  | Configuration System   | |
|  +------------------+  +------------------+  +------------------------+ |
+-------------------------------------------------------------------------+
                                    |
+-------------------------------------------------------------------------+
|                           FAITHFUL CORE (P0-P1)                         |
|  +-------------------------------------------------------------------+  |
|  |                  GBARecomp Static Recompiler Runtime              |  |
|  |  - Sharded Native C++ Translation of ARM7TDMI Instructions        |  |
|  |  - JIT/Interpreter Fallback Tier for Dynamic Jump Tables          |  |
|  |  - Cycle-Accurate PPU, DMA, Timer, Interrupt & Bus Architecture   |  |
|  +-------------------------------------------------------------------+  |
|  |                     cvaos Reverse-Engineered Data                 |  |
|  |  - Symbol Tables, Memory Maps (EWRAM/IWRAM), Entity Structures    |  |
|  |  - Verified USA ROM Checksum Gate (`abd71fe01ebb201bcc133...`)   |  |
|  +-------------------------------------------------------------------+  |
+-------------------------------------------------------------------------+
```

---

## 2. Subsystem Details

### 2.1 ROM Verification Subsystem (`src/core/rom_validator.hpp`)
- Cryptographically hashes the ROM file supplied by the user.
- Compares against known hashes:
  - **USA v1.0**: SHA-1 `abd71fe01ebb201bcc133074db1dd8c5253776c7`, MD5 `e7470df4d241f73060d14437011b90ce`, Size: 8,388,608 bytes.
- Halts execution gracefully with diagnostic feedback if the ROM is missing, corrupted, or of an unsupported revision.

### 2.2 Configuration System (`src/config/config_system.hpp`)
- Single source of truth for all game options.
- Strongly-typed structs with schema validation:
  - `VideoConfig` (Resolution, Fullscreen, Aspect Ratio, Widescreen, Interpolation, VSync, Integer Scale, Color Filter).
  - `AudioConfig` (Master, Music, SFX volumes, Mode: Authentic / Enhanced / Replacement).
  - `InputConfig` (Action bindings, Deadzones, Glyph style, Vibration, Turbo).
  - `GameplayConfig` (Difficulty presets, Fast text, Skip cutscenes, Boss retry, Farm pity).
  - `CheatsConfig` (Infinite HP/MP, EXP multiplier, Drop multipliers, Teleport).
  - `ModsConfig` (Active mod list, Mod load orders, Mod state flags).
- JSON/TOML serializable with backward/forward compatibility.

### 2.3 Input Action System (`src/input/action_system.hpp`)
- Maps physical events (SDL_Gamepad, XInput, DirectInput, DualSense, Steam Deck, Keyboard/Mouse) to game actions:
  - `Action::DpadUp`, `Action::DpadDown`, `Action::DpadLeft`, `Action::DpadRight`
  - `Action::Jump` (A), `Action::Attack` (B), `Action::Soul` (R), `Action::Backdash` (L)
  - `Action::Menu` (Start), `Action::Map` (Select)
  - `Action::OverlayMenu` (Escape / Guide), `Action::QuickSave`, `Action::QuickLoad`, `Action::Rewind`
  - Modern action expansions: `Action::SoulRadial`, `Action::WeaponSwap`, `Action::FastForward`.

### 2.4 Video & Adaptive Widescreen Subsystem (`src/video/`)
- Authentic rendering path: Pixel-perfect 240×160 PPU frame buffer with software/GL/Vulkan upscaling.
- Adaptive Widescreen path:
  - Room geometry & camera bounding box discovery from `cvaos` symbols.
  - Scene compatibility tag system (`COMPAT_WIDE_FULL`, `COMPAT_WIDE_SAFE`, `COMPAT_PILLARBOX_ONLY`).
  - Extended tilemap fetch without exposing out-of-bounds VRAM.
  - High-Hz sub-frame display interpolation preserving 60 Hz game logic clock.

### 2.5 Deterministic Rewind & Save State Engine (`src/state/`)
- Bounded ring buffer holding 10 seconds of historical frames at 60 Hz (600 frames).
- Frame snapshot components:
  - GBA CPU register bank (R0-R15, CPSR, SPSR).
  - Memory: IWRAM (32 KB), EWRAM (256 KB), VRAM (96 KB), OAM (1 KB), Palette RAM (1 KB).
  - Hardware state: Timers, DMA channels, PPU scanline state, Sound channels & mixer.
  - Software state: `gRngState`, player entity pointer, active entity list, game flags.
  - Active mod state hooks.
- Delta compression to minimize memory footprint while maintaining $O(1)$ fast rewind restoration.

### 2.6 Cheats & Quality-of-Life Subsystem (`src/cheats/`, `src/qol/`)
- Direct hook integration using verified symbol addresses:
  - Player HP / MP / Hearts (`gPlayer` struct in EWRAM / IWRAM).
  - Inventory & Equipment tables.
  - Soul drop calculation hook (adjusting RNG or forcing drop flags).
  - Enemy HP overlay & bestiary lookup.
  - Warp room activation matrix.

### 2.7 HD Asset & Modding Framework (`src/hd/`, `src/mods/`)
- Dynamic texture replacement based on unique asset hash / symbol IDs.
- Modular HD packs in `.zip` or directory format containing:
  - `manifest.json` (author, version, compatibility).
  - Spritesheets, background layers, high-res portraits, vectorized fonts.
  - Replacement soundtrack tracks (OGG / FLAC / WAV) with loop point markers.

---

## 3. Directory Layout

```
ariaOfSorrow-recomp/
├── CMakeLists.txt
├── AGENTS.md
├── ARCHITECTURE.md
├── ROADMAP.md
├── .gitignore
├── include/
│   └── aria/
│       ├── config.hpp
│       ├── core.hpp
│       └── symbols.hpp
├── src/
│   ├── main.cpp
│   ├── core/
│   │   ├── rom_validator.hpp
│   │   ├── rom_validator.cpp
│   │   ├── gba_runtime.hpp
│   │   └── gba_runtime.cpp
│   ├── config/
│   │   ├── config_system.hpp
│   │   └── config_system.cpp
│   ├── input/
│   │   ├── action_system.hpp
│   │   └── action_system.cpp
│   ├── video/
│   │   ├── video_renderer.hpp
│   │   └── video_renderer.cpp
│   ├── audio/
│   │   ├── audio_system.hpp
│   │   └── audio_system.cpp
│   ├── state/
│   │   ├── save_state_manager.hpp
│   │   ├── save_state_manager.cpp
│   │   ├── rewind_engine.hpp
│   │   └── rewind_engine.cpp
│   ├── cheats/
│   │   ├── cheat_engine.hpp
│   │   └── cheat_engine.cpp
│   ├── qol/
│   │   ├── qol_features.hpp
│   │   └── qol_features.cpp
│   ├── mods/
│   │   ├── mod_manager.hpp
│   │   └── mod_manager.cpp
│   └── ui/
│       ├── overlay_menu.hpp
│       └── overlay_menu.cpp
├── symbols/
│   ├── cvaos_symbols.hpp
│   └── symbols_us.json
├── tools/
│   ├── rom_analyzer.py
│   └── hd_asset_packer.py
└── tests/
    ├── test_rom_validator.cpp
    ├── test_config.cpp
    ├── test_rewind.cpp
    └── test_save_state.cpp
```
