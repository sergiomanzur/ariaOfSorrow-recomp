# Plan — audio pre-roll artifact, inventory/map grants, skip-dialogue

Three independent, sequential tasks for AriaRecomp (static recompilation of
Castlevania: Aria of Sorrow, GBA). Execute in order: 1, 2, 3.

## Context

`aria_recomp.exe` at the repo root is the binary that actually runs; CMake
builds to `build/aria_recomp.exe`, so **every task must copy the built exe to
the repo root before testing it**. A stale root exe has already cost this
project a full misdiagnosis cycle.

The engine lives in `third_party/gbarecomp`, which is **our own fork** — edits
there are allowed but must stay minimal and carry a comment explaining why.
`third_party/cvaos` is a read-only community decomp of this exact game and is
the only sanctioned source of memory offsets.

## Global Constraints

- **Never run `git commit`, `git add`, `git stash`, or any command that writes
  git history or the index.** Leave all work in the working tree. This is a
  standing instruction from the repo owner and overrides any workflow default.
- **Never guess a memory address.** Every offset must be cited to a
  `third_party/cvaos` file:line. If the decomp does not document it, say so and
  stop rather than inventing it.
- **No game text in our source.** Do not hardcode official item, soul, enemy or
  dialogue strings — the repo owner requires no copyrighted content.
- Build with `cmake --build build -j 4` from the repo root. Then
  `cp build/aria_recomp.exe ./aria_recomp.exe`.
- These four test binaries must still pass after every task:
  `build/test_cheats_qol.exe`, `build/test_config.exe`,
  `build/test_action_system.exe`, `build/test_rom_validator.exe`.
- Environment is Windows + Git Bash. The exe is a GUI-subsystem app; its stderr
  can be captured with `2> file` when launched from bash. Kill leftover
  instances with
  `powershell -NoProfile -Command "Get-Process aria_recomp -ErrorAction SilentlyContinue | Stop-Process -Force"`.
- Do not delete or modify `recomp_cache/` (60 MB warm self-heal cache),
  `*.sav`, or `*.state1` — those are the owner's data.
- Do not add features beyond the task. No speculative abstractions.

---

## Task 1 — Remove the audio pre-roll speed artifact

### Problem (already root-caused; do not re-investigate)

`third_party/gbarecomp/src/runtime/host_window.cpp:1299-1300` configures the
audio clock-domain bridge with:

```c
cfg.target_ms   = 60.0;    // steady cushion
cfg.preroll_ms  = 250.0;   // boot pre-roll: hide the cold-start warm-up hitch
```

So the ring primes to 250 ms but steady-state target is 60 ms — a 190 ms
surplus. The only thing that removes the surplus is the resampler servo, whose
correction is clamped to ±0.5% (`max_correction = 0.005`,
`recomp_audio_drc.h:45`). That drains only a few ms of buffer per second of
wall time.

Measured on this machine, identically on warm and cold self-heal cache, with
`GBARECOMP_AUDIO_PROBE=1`:

| audio time | `fill_ms` | `corr` |
|---|---|---|
| 2.0 s | 264.0 | **+0.500%** (pinned at the clamp) |
| 10.1 s | 200.6 | **+0.500%** |
| 20.1 s | 101.2 | **+0.500%** |
| 130.6 s | 59.7 | −0.092% (normal) |

Consequence every single launch: audio plays 0.5% fast for roughly the first
30–45 seconds while sitting up to a quarter second behind the video, then
snaps to correct. Video is not affected — emulated frame rate measures
59.696/s vs hardware 59.7275 (0.9995x), and 7972 of 7973 presents advanced
exactly one guest frame.

### Required change

Keep the 250 ms boot cushion (it exists to survive the cold-start hitch, and
cutting it risks reintroducing boot underruns). Instead, **discard the surplus
in one step once the cushion is no longer needed**, in
`third_party/gbarecomp/src/runtime/recomp_audio_drc.h`:

1. Add a config field `trim_after_ms` (double), defaulted in
   `rab_config_defaults` to `2000.0`, documented as: how long after priming to
   wait before dropping a leftover pre-roll surplus in one step.
2. Track how much host time has been pulled since the bridge primed (the pull
   callback knows its own block size and `host_rate`; accumulate there — do not
   call wall-clock time from the audio callback).
3. Once primed AND that elapsed time exceeds `trim_after_ms`, if
   `rab_fill_ms(b)` exceeds `target_ms + 20.0`, advance the read cursor
   (`b->out_pos`) so the fill lands at `target_ms`, exactly once. Guard it with
   a `trimmed` flag so it can never run twice and never fight the servo.
4. Mask the discontinuity with the bridge's existing fade machinery: set
   `b->gain = 0.0` at the trim so the established ramp fades back in, the same
   way the underrun path already conceals a seam.
5. Add `uint64_t trim_events; uint64_t trimmed_frames;` to `rab_stats` and
   report them in the existing `[gba-audio-probe]` line in
   `host_window.cpp` (~line 1455).

Do **not** change `target_ms`, `preroll_ms`, `max_correction` or `slew_pp_per_s`.
Do **not** widen the ±0.5% clamp — that would trade a latency artifact for an
audible pitch artifact.

### Verification (must be evidence, not assertion)

Build, copy the exe to the repo root, then run for at least 60 seconds:

```
GBARECOMP_AUDIO_PROBE=1 GBARECOMP_PRESENT_CADENCE=1 \
  GBARECOMP_DEMO_INPUT=campaign ./aria_recomp.exe --window 2> audio_after.log
```

Paste the `[gba-audio-probe]` lines into the report. Acceptance:

- `fill_ms` drops to **≤ 90** within 8 seconds of audio time and stays near 60.
- `corr` is within **±0.2%** after the trim — it must NOT sit pinned at 0.500%.
- `bridge_underrun=0`, `stretch=0ms(ev=0)`, `overflow_drops=0` across the run.
- `trim_events=1` (exactly one trim).
- `[present-cadence]` fps stays in 59.65–59.80.

Kill the process cleanly afterwards. Then run all four test binaries.

---

## Task 2 — Grant souls / items / full map

### Verified memory layout (cite these; do not re-derive)

All offsets are relative to EWRAM base `0x02000000`, verified in
`third_party/cvaos/include/structs/ewram.h:715-758`:

| Field | EWRAM offset | Shape |
|---|---|---|
| `totalNbrSoulsCollected` | `0x13264` | u16, capped 999 |
| `itemInventory` | `0x13294` | u8[0x20] — 32 consumables |
| `weaponInventory` | `0x132B4` | u8[0x3B] — 59 weapons |
| `armorInventory` | `0x132EF` | u8[0x19] — 25 armor |
| `accessoryInventory` | `0x13308` | u8[0x14] — 20 accessories |
| `redSoulInventory` (bullet) | `0x1331C` | u8[2][0x1C], 55 souls |
| `blueSoulInventory` (guardian) | `0x13354` | u8[2][0xD], 24 souls |
| `yellowSoulInventory` (enchant) | `0x1336E` | u8[2][0x12], 35 souls |
| `abilitySoulInventory` | `0x13392` | u8[0x3] (flat, no group dim), 8 souls |
| category flags | `0x13396` | u8[2] bitfield |
| map grid | `0x000B4` | `{u32 visited; u32 revealed;}[2][40]` |

Souls are a **nibble per soul, value 0-9**; index `i` lives in byte `i >> 1`,
low nibble when `i` is even, high nibble when odd. Items are **one byte per
item = quantity**.

The game's own debug give-all is `third_party/cvaos/src/code_08038A38.c:433-462`
with counts `sUnk_084F158C[] = {0x37,0x18,0x23,0x8}` (souls) and
`sUnk_084F159C[] = {0x20,0x3B,0x2D}` (items). Replicate its semantics exactly:

- Souls: `SoulInventory_AddAmountToFirstGroupTotal(type, index, 1)` — see
  `third_party/cvaos/src/code_08032444.c:100-160`. It reads the nibble, adds 1,
  clamps to 9, writes back to **group 0 only**, and adds the amount to
  `totalNbrSoulsCollected` (capped 999). Amount granted is **1**, not 9.
- Items: sets the byte to `1`. Note armor uses count `0x2D` (45), which
  deliberately runs contiguously from `armorInventory` into
  `accessoryInventory` (`0x132EF + 0x19 == 0x13308`) — that is correct, not a
  bug. Ability souls likewise write 8 nibbles = 4 bytes, spilling into the
  `pad_13395` byte, exactly as the game does.
- The debug path then sets bits 0-5 of the `0x13396` bitfield via
  `sub_08032ADC` (`third_party/cvaos/src/code_08032444.c:718-734`), which is a
  plain bit set. Its meaning is undocumented in the decomp; mirror the game and
  set the bit for each category granted.

### Map reveal

The ROM room table is `sUnk_08116650` → ROM `0x08116650` → **file offset
`0x116650`**, `u16[64*35]`, row stride 64 (`cell = table[x + (y << 6)]`).
`0xFFFF` means "no room"; bit `0x4000` = save room, `0x8000` = warp room. This
was verified against the owner's ROM: 848 rooms, 8 save rooms, and the
occupancy grid renders a recognisable castle silhouette.

Read the table once at startup from the ROM file (`main.cpp` already knows
`romPath` and opens the ROM to validate it) — do **not** add an engine hook for
ROM access. Cache it.

To reveal: for every `(x,y)` with `x < 64`, `y < 35` where the cell is not
`0xFFFF`, set bit `(x & 31)` in **both** u32s of the grid element at
`0xB4 + ((x >> 5) * 40 + y) * 8` (`+0` = visited, `+4` = revealed). Leave rows
35-39 untouched.

### Structure

Create `src/gameplay/grant_system.{hpp,cpp}` — a new focused unit, not an
addition to `CheatSystem` (grants are one-shot events; `CheatSystem` re-pins
values every frame). Delete the three dead stubs `UnlockAllSouls`,
`UnlockAllItems`, `UnlockFullMap` from
`src/gameplay/cheat_system.{hpp,cpp}` — they currently just `return true` and
do nothing.

Public surface:

- `enum class GrantKind` — `BulletSouls, GuardianSouls, EnchantSouls,
  AbilitySouls, Weapons, ArmorAndAccessories, Consumables, RevealMap`.
- `void Request(GrantKind)` — called from the UI thread; only sets a pending
  flag. Must not touch guest memory.
- `void SetRoomTable(const uint16_t* cells, size_t count)` — startup.
- `void SetSavePath(std::string)` — for the backup.
- `void ApplyPending(uint8_t* ewram, size_t ewramSize)` — called from the
  per-frame EWRAM hook; applies at most the pending set, then clears it.

Wire `ApplyPending` into the existing `AriaEwramFrameWrite` in `src/main.cpp`
alongside `CheatSystem::ApplyFrameCheats`. The UI action callback
(`AriaUiAction` in `src/ui/settings_rows.cpp`) routes the new rows to
`Request`.

### Menu rows

Eight `RECOMP_RUNTIME_UI_ACTION` rows in section `"Cheats"`, keys
`cheats.grant_bullet_souls`, `cheats.grant_guardian_souls`,
`cheats.grant_enchant_souls`, `cheats.grant_ability_souls`,
`cheats.grant_weapons`, `cheats.grant_armor`, `cheats.grant_consumables`,
`cheats.reveal_map`. Labels are plain English ("Grant all bullet souls",
"Reveal full map", …) — these are our words, not game text.

`AriaUiEnabled` must gate all eight on the existing
`cheats.enableCheats` master switch, exactly like the existing infinite-HP
rows. Note the load-bearing detail already in that function: keys it does not
own must return 1, or the engine's own settings get greyed out.

### Safety (non-negotiable)

- Apply only on a frame where the player block looks plausible — reuse the
  existing gate style from `cheat_system.cpp:40-42`: `maxHP > 0 && maxHP <= 9999
  && currentHP <= maxHP`. If implausible (title screen, unloaded save), keep the
  request pending and retry next frame rather than writing.
- Before the **first** applied grant of the process, copy the save file to
  `<savepath>.bak-YYYYMMDD-HHMMSS`. Once per process only. If the copy fails,
  log to stderr and still proceed.
- Bounds-check every write against `ewramSize` before touching memory.

### Tests

Extend `tests/test_cheats_qol.cpp` (it already builds as
`build/test_cheats_qol.exe`). Against a simulated 256 KiB EWRAM buffer:

1. Each of the seven inventory grants writes exactly its documented range, and
   the bytes immediately before and after that range are provably unchanged.
2. Nibble packing: after granting bullet souls, soul index 0 is the low nibble
   of `0x1331C` and index 1 is the high nibble of the same byte, both == 1.
3. Repeat-grant clamps at 9, and `totalNbrSoulsCollected` caps at 999.
4. Ability souls write 8 nibbles across 4 bytes (`0x13392`..`0x13395`).
5. Map reveal, given a synthetic room table, sets both planes only for cells
   that are not `0xFFFF`, and touches no element for rows >= 35.
6. The plausibility gate blocks all writes when `maxHP` is implausible.

### Verification

Build, copy exe to root, run all four test binaries. Then a live check: launch
windowed, enable cheats in the Esc menu, activate a grant, and confirm the
bytes landed by dumping EWRAM with `GBARECOMP_EWRAM_DUMP=<path>` and checking
the expected offsets. Report the actual byte values seen.

---

## Task 3 — Skip-dialogue shortcut

### Research outcome (settled; do not re-investigate)

A survey of `third_party/cvaos` established that the decomp is early-stage and
documents **none** of the following: the text/message engine's runtime state, a
text-speed setting, the message-advance code path, or any built-in
fast-text/skip-cutscene support. The only documented input state is
`struct InputData` at EWRAM `0x00014` (`heldInput` `0x14`, `newInput` `0x16`,
`repeatedInput` `0x18`, `playerHeldInput` `0x1C`, `playerNewInput` `0x1E`) —
see `third_party/cvaos/include/structs/ewram.h:6-14`.

Therefore there is **no verified memory field to poke**, and per the Global
Constraints we do not guess one.

### Required design — host-side auto-tap, zero guest writes

Implement the shortcut entirely in the host input layer. While a bound hotkey
is held, synthesize repeated A-button presses into the KEYINPUT value the guest
reads, so the game advances dialogue exactly as if the player were mashing A.
No guest memory is written, so this cannot corrupt the save or desync the
script engine — the guest only ever sees ordinary button input.

1. In `third_party/gbarecomp/src/runtime/host_window.cpp`, add a new hotkey id
   to the `HostHotkey` enum (e.g. `HK_SKIP_DIALOGUE`) with its `kHotkeyNames`
   entry `"SkipDialogue"` and a `kHotkeyDefaults` entry of `"Q"`. Keep the
   arrays index-aligned — they are parallel and a mismatch silently misbinds
   every later hotkey.
2. In `HostWindow::pump()`, where `keys` is built from `SDL_GetKeyboardState`
   (~line 2076) and after the existing player-bind loop, check whether the
   SkipDialogue binding is held (same level-triggered pattern as
   `HK_TURBO`, ~line 2168, including `hotkey_mods_ok`).
3. While held, clear the A bit (bit 0, active-low: clearing = pressed) on an
   alternating cadence so the guest sees fresh press *edges* rather than one
   continuous hold — a held button will not advance successive message boxes.
   Use a frame-parity counter on the Backend struct: pressed for 2 pumps, then
   released for 2 pumps (≈15 taps/second). Name the constant.
4. Do not disturb the player's real A binding: if the player is genuinely
   holding A, the result must still be "pressed". Only ever add the press,
   never force a release of a real press.

### Explicitly out of scope (ruled by the controller)

- **No settings row / config field.** The feature only acts while its key is
  held, so it is already opt-in per keypress; an enable toggle would be a
  setting with no use. It is a rebindable `config.ini` `[KeyMap]` hotkey,
  matching the existing Turbo / Pause / DisplayPerf pattern.
- **No guest memory writes.** Do not touch `struct InputData` in EWRAM, and do
  not add an engine hook for it.

### Documentation

Add the binding to the controls documentation in `README.md` alongside the
other hotkeys, described in our own words. State plainly that because it taps
the A button, holding it during normal gameplay will attack repeatedly.

### Verification

Build, copy the exe to the repo root, then launch windowed with
`GBARECOMP_DEMO_INPUT=campaign` and let it reach the opening narration (roughly
40-60 s of turbo, or drive it manually). Hold the bound key and capture two
`PrintWindow` screenshots several seconds apart showing the dialogue has
advanced further than it would have unaided. Report both screenshot paths and
what changed between them. Then run the four required test binaries.
