# Cheats & Quality-of-Life Settings Menu — Design

**Date:** 2026-09-07
**Status:** Approved for implementation planning
**Scope:** Add game-owned Cheats and Quality of Life sections to the in-game
settings overlay, and make the cheats that have real implementations actually
affect the running game.

---

## 1. Context

The in-game overlay (recomp-ui, opened with Esc) is now compiled in and
working, but it shows only the engine's own catalog: Display, Graphics, Audio,
System, Assist Tools. Nothing from `aria_config.ini`'s `[Gameplay]` or
`[Cheats]` sections is reachable, and the systems under `src/gameplay/` are
compiled into `aria_core` yet never referenced by `src/main.cpp`.

## 2. Findings that shape this design

These were established by reading the code and probing the running binary, and
they are the reason the design looks the way it does.

**The menu needs no engine change.** `RunOptions::ui_extra_items` plus
`ui_get` / `ui_set` / `ui_action` / `ui_enabled` (`runtime.h:120-152`) is the
supported seam for game-owned rows. `runtime.cpp:244-371` routes any key the
engine does not recognise to those callbacks.

**Making cheats effective does need an engine change.** `CheatSystem::ApplyFrameCheats`
requires EWRAM/IWRAM pointers once per frame. `RunOptions` exposes no such
callback; the only game-owned per-frame hook is `extended_view_frame`, which
carries read-only PPU registers and fires only when a non-native view policy
is active.

**Most of the cheat and QoL surface does not exist yet.** Of the 15 fields in
`CheatsConfig`, only `enableCheats` (as a master gate), `infiniteHP`,
`infiniteMP` and `infiniteHearts` have code. `guaranteedSouls` is an empty
`if` with a comment; `UnlockAllSouls` / `UnlockAllItems` / `UnlockFullMap`
return `true` without writing anything; `invincibility`, `oneHitKill`,
`expMultiplier`, `unlockAllWarps`, `noclip`, `freezeEnemies` and `gameSpeed`
have no code at all. In `qol_system.cpp`, `Update()` is an empty body, and
`GetAdjustedDropRate` / `RecordEnemyKill` are correct pure functions with no
caller anywhere in the tree — they would need drop-roll hooks inside the
game's own code. Every `GameplayConfig` field is therefore currently just a
stored boolean.

**The one implemented cheat path targets an unverified address.**
`ADDR_PLAYER_ENTITY = 0x02000020`, with HP/MAX_HP/MP/MAX_MP/HEARTS/MAX_HEARTS
packed at +0/+2/+4/+6/+8/+0x0A (`cvaos_symbols.hpp:36-57`), reads as an
invented schema rather than a reverse-engineered one. Parsing the `GBAS`
container in `test_play.state` shows that region entirely zero, but that state
has only 6,836 of 262,144 EWRAM bytes non-zero, so it is pre-gameplay and the
observation is inconclusive either way.

## 3. Goals

- Cheats and Quality of Life sections appear in the overlay, backed by
  `ConfigSystem` and persisted to `aria_config.ini`.
- Infinite HP, Infinite MP and Infinite Hearts genuinely work in game.
- A row with no implementation behind it is visibly disabled, never a toggle
  that silently does nothing.
- A wrong or stale player-struct address cannot corrupt the player's save file.

## 4. Non-goals

- Implementing the missing cheats (invincibility, one-hit kill, exp
  multiplier, unlock-all souls/items/map, noclip, freeze enemies, game speed).
- Implementing the QoL behaviours (fast text, fast doors, skip cutscenes,
  quick retry, save anywhere, drop-rate and pity hooks).
- Any change to the engine's standard catalog rows.

Both exclusions are deliberate: they require reverse-engineering work against
the ROM and were explicitly deferred.

## 5. Design

### 5.1 Engine seam — per-frame guest memory

Add to `RunOptions` in `third_party/gbarecomp/src/runtime/runtime.h`:

    // Read/write view of guest work RAM, published once per emulated frame to
    // an opted-in game's frame callback. The runtime owns the memory; the game
    // owns what it does with it. Null callback = no cost, no behaviour change.
    struct GuestMemoryView {
        std::uint8_t* ewram      = nullptr;
        std::size_t   ewram_size = 0;
        std::uint8_t* iwram      = nullptr;
        std::size_t   iwram_size = 0;
    };

    void (*guest_frame)(const GuestMemoryView* memory) = nullptr;

It fires from the existing frame-boundary hook. `g_frame_start_hook` runs at
`events.frame_completed` inside `tick_devices` (`runtime_bus_bridge.cpp:561`) —
a true frame boundary, which is where a cheat pass belongs. That hook is
single-slot and already claimed by `extended_view_frame`, so `runtime.cpp`
registers one lambda that fans out to both callbacks rather than letting the
later registration silently drop the earlier one.

Rationale for rejecting the alternatives: the mod system
(`GBARECOMP_ENABLE_MODS` + `mod_function_hooks`) keys on guest *functions*, not
a per-frame memory pass, and enabling it also flips `RECOMP_UI_ENABLE_MODS` and
pulls in the `.gbamod` catalog. Reusing `extended_view_frame` does not work at
all, as it carries no work-RAM pointers.

### 5.2 Address verification and the safety gate

The address is verified once, by investigation, then hardcoded — the ROM is
SHA-1 gated to a single revision, so a constant is legitimate. A runtime
signature scan would add real complexity to survive revisions that are not
supported anyway.

Procedure, using the TCP debug server (`TCP.md:86-112`), which is the
sanctioned interface and already exposes everything needed:

1. Launch with `--tcp <port> --no-window`.
2. Drive to gameplay with `run_frames {"n":N,"keyinput":K}` and `set_keyinput`,
   working through the intro, title and save-file load. `keyinput` is GBA
   `KEYINPUT` semantics — active low, bit 3 Start, bit 0 A.
3. `savestate_save` a mid-gameplay state, kept as a test fixture.
4. `read_ewram` a full 256 KB dump.
5. Filter candidates: `u16` pairs at `+0`/`+2` where `0 < cur <= max` and `max`
   is in a plausible Aria of Sorrow band.
6. Advance idle frames and re-dump: HP and MAX_HP must be *stable* while
   animation and position counters move. Drops volatile noise.
7. Take damage via scripted input and diff again: the true HP address changes
   while MAX_HP holds. That pins it.
8. Correct `cvaos_symbols.hpp` and record the evidence in a comment.

Independently of the outcome, `ApplyFrameCheats` gains a plausibility gate
before any write: `max` non-zero and within a sane band, and `cur <= max`.
This matters because these writes land in live guest memory that the runtime
flushes to `Castlevania - Aria of Sorrow (USA).sav`; an address that is wrong,
or right but read before the struct is populated, would otherwise corrupt a
real save file. The gate makes a bad address inert instead of destructive.

### 5.3 New unit — src/gameplay/frame_hook.hpp / .cpp

One job: receive the engine's per-frame `GuestMemoryView` and forward it to
`CheatSystem::ApplyFrameCheats` and `QolSystem::Update`. Exposes a single free
function matching the `guest_frame` signature. Depends only on the two existing
systems. Holds no state of its own.

### 5.4 New unit — src/ui/settings_rows.hpp / .cpp

Owns the `RecompRuntimeUiItem` array and the four callbacks, and is the only
place that maps a row key to a `ConfigSystem` field.

- Keys are namespaced `cheats.*` and `gameplay.*` so they cannot collide with
  the engine's `display.*` / `graphics.*` / `assist.*` keys.
- The item array and every string it references are static storage duration,
  satisfying the `RunOptions` requirement that they outlive `run_game()`.
- `ui_get` / `ui_set` translate `bool` fields as 0/1. Float multipliers are
  `RECOMP_RUNTIME_UI_INT` rows carrying percent (100 = 1.0x) with explicit
  `minimum` / `maximum` / `step`, since recomp-ui has no float row type.
- `ui_set` writes through to `ConfigSystem`, saves `aria_config.ini`, then
  pushes the updated config into `CheatSystem::SetConfig` / `QolSystem::SetConfig`
  so the change takes effect on the next frame. Write-through is necessary
  because gbarecomp never wires recomp-ui's `save` callback
  (`runtime.cpp:2528-2535` sets get/set/action/is_enabled and the text pair,
  and nothing else).
- `ui_action` handles the one-shot unlock rows. They are unimplemented, so it
  returns 0 for them for now.
- `ui_enabled` returns 0 for keys with no implementation behind them, and
  **1 for every key it does not own**. This is load-bearing: `runtime_ui_enabled`
  (`runtime.cpp:359-373`) consults `opts.ui_enabled` for all fall-through keys,
  so returning 0 by default would grey out the engine's own rows.
- Cheat rows other than the master toggle also report disabled while
  `cheats.enable` is off, so the dependency is visible rather than surprising.

### 5.5 Build wiring

`settings_rows.cpp` is the one unit in `src/` that needs recomp-ui's headers,
because `RunOptions` deliberately types `ui_extra_items` as `const void*` so
that `runtime.h` stays parseable in builds without them. `aria_core` therefore
gains the `third_party/recomp-ui/src` and `src/common` include directories,
guarded by the same `GBARECOMP_RUNTIME_UI_ROOT` condition already added to the
root `CMakeLists.txt`. When the overlay is not built, `settings_rows` compiles
to nothing and `main.cpp` leaves the `ui_*` fields null — the `guest_frame`
hook and the cheats still work, they are simply not reachable from a menu.

### 5.6 src/main.cpp

Populates `opts.ui_extra_items` / `ui_extra_item_count` and the four callback
pointers, sets `opts.guest_frame`, and calls `CheatSystem::Initialize` /
`QolSystem::Initialize` with the loaded config — which nothing currently does.

### 5.7 Row inventory

`Implemented` means there is code behind the row, so switching it changes the
running game. It is distinct from the row's live enabled state: an implemented
cheat row still reports disabled while `cheats.enable` is off (5.4), and an
unimplemented row is disabled unconditionally.

| Key | Section | Type | Implemented |
|---|---|---|---|
| `cheats.enable` | Cheats | BOOL | yes |
| `cheats.infinite_hp` | Cheats | BOOL | yes |
| `cheats.infinite_mp` | Cheats | BOOL | yes |
| `cheats.infinite_hearts` | Cheats | BOOL | yes |
| `cheats.invincibility` | Cheats | BOOL | no |
| `cheats.one_hit_kill` | Cheats | BOOL | no |
| `cheats.exp_multiplier` | Cheats | INT (%) | no |
| `cheats.guaranteed_souls` | Cheats | BOOL | no |
| `cheats.unlock_all_souls` | Cheats | ACTION | no |
| `cheats.unlock_all_items` | Cheats | ACTION | no |
| `cheats.unlock_full_map` | Cheats | ACTION | no |
| `gameplay.fast_text` | Quality of Life | BOOL | no |
| `gameplay.fast_doors` | Quality of Life | BOOL | no |
| `gameplay.skip_seen_cutscenes` | Quality of Life | BOOL | no |
| `gameplay.death_quick_retry` | Quality of Life | BOOL | no |
| `gameplay.boss_quick_retry` | Quality of Life | BOOL | no |
| `gameplay.soul_drop_multiplier` | Quality of Life | INT (%) | no |
| `gameplay.farm_pity` | Quality of Life | BOOL | no |

The Quality of Life section therefore ships entirely disabled. That is the
accepted consequence of not padding the menu with inert toggles; each row
becomes enabled in the same change that implements its behaviour.

Omitted rows and why: `noclip`, `freezeEnemies`, `gameSpeed`, `unlockAllWarps`,
`skipIntro`, `autoSaveOnRoomChange`, `allowSaveAnywhere`, `itemDropMultiplier`.
Each has neither an implementation nor a near-term path to one, and `skipIntro`
is already handled by the engine's `--bios-skip-intro`. They stay in
`aria_config.ini` and can be surfaced when they do something.

## 6. Testing

- A unit test over the mid-gameplay EWRAM fixture. Step 3 of 5.2 saves a
  `GBAS` savestate; the verification step also writes its 256 KB EWRAM block
  out as a raw `tests/fixtures/ewram_gameplay.bin` so the unit test needs only
  `fread`, not gbarecomp's internal snapshot container parser. The test loads
  that buffer, runs `ApplyFrameCheats` with `infiniteHP` on, and asserts HP is
  clamped to MAX_HP.
- A negative test: point the cheat at a deliberately wrong offset and assert
  the plausibility gate leaves memory untouched.
- A `settings_rows` test asserting `ui_enabled` returns 1 for an engine key
  such as `display.fullscreen`, 0 for an unimplemented key, and that a `ui_set`
  round-trips through `ConfigSystem` and back out of `ui_get`.
- The four existing tests stay green.
- Manual confirmation in the real window, driven the way the overlay was
  verified: `SendInput` scancodes, since `SendKeys` does not reach SDL.

## 7. Risks

- **The player struct may not be at a fixed address.** If it turns out to live
  behind a pointer indirection, 5.2 discovers that and the design needs a
  revision: reading a pointer each frame rather than a constant. The
  plausibility gate keeps that failure safe rather than destructive.
- **Verification may not reach gameplay under scripted input.** If driving the
  save-file load proves unreliable, the fallback is to advance further with the
  existing `GBARECOMP_DEMO_INPUT=campaign` track, whose walk phase is
  documented to reach controllable gameplay near frame 9000.
- **`RunOptions` layout change.** The new field is appended and defaults to
  null, so existing consumers are unaffected, but gbarecomp is vendored and
  this is a local modification to carry across updates. It belongs in the same
  note as the pacing fix already applied to `runtime.cpp`.
