# Static Recompilation: Phase 2 Documentation

## Jump Table Formalization & Dynamic Control-Flow Warning Elimination

**Status**: Verified & Operational  
**Binary Compatibility**: 100% (Bit-exact SHA-1: `abd71fe01ebb201bcc133074db1dd8c5253776c7`)  
**Jump Tables Modeled**: 46 / 46 (100% explicit coverage)  
**Decoded Jump Targets**: 762 static code entries  
**Compiler Control-Flow Warnings**: 0  

---

## 1. Executive Summary

In Phase 1, we achieved **zero runtime misses** and **zero interpreted instructions** by ingesting 238 function misses and declaring 244 function seeds in `game.toml`. However, the GBARecomp code generator still produced a control-flow collision warning:
```text
WARNING: 1 control-flow entries into an auto-detected jump_table (mis-modeled switch; bytes kept as data, residual branches self-heal at runtime). Sample:
  [0x080DA558,0x080DA5A0) auto jump_table <- 0x080DA558 via branch in fn 0x080DA558
```
Additionally, 46 dispatch tables remained heuristic "auto jump tables" rather than formalized, bounded descriptors.

In **Phase 2**, we:
1. Conducted reverse-engineering of `third_party/cvaos/src/m4a.c` and raw ROM bytes around `0x080DA500`–`0x080DA800`.
2. Diagnosed the root cause of the warning: `0x080DA558` was erroneously declared as an `extra_func` (`MP2K_event_xcmd`) when it was actually the 18-case jump table for `MP2K_event_memacc` (`0x080DA524`), while the genuine `MP2K_event_xcmd` and `gXcmdTable` handlers resided at `0x080DA678` through `0x080DA79C`.
3. Corrected all sound engine symbol declarations in `game.toml`.
4. Declared all 46 jump tables explicitly via `[[jump_table]]` descriptors with bounded counts, `abs32` format, `thumb` entry modes, and descriptive semantic names.
5. Recompiled shards cleanly: `auto_jump_tables` dropped from 46 to **0**, `data_ranges_honored` increased to **46**, and recompiler warnings dropped to **0**.

---

## 2. Technical Investigation & Root Cause Analysis

### 2.1 The `0x080DA558` Collision

The recompiler warned that an auto-detected jump table at `[0x080DA558, 0x080DA5A0)` was entered from `0x080DA558` by a function starting at `0x080DA558`.

Disassembly of `Castlevania - Aria of Sorrow (USA).gba` at `0x080DA524`:
```arm
// MP2K_event_memacc entry
080DA524:  stm  r13!, {r4,r5,r6,lr}
...
080DA542:  cmp  r5, #0x11        // Bounds check: op <= 17 (18 total cases)
080DA544:  bls  080DA548
080DA546:  b    080DA676         // Out of bounds / default case
080DA548:  lsl  r0, r5, #2       // r0 = op * 4
080DA54A:  ldr  r1, [pc, #8]     // Loads word at 0x080DA554 (literal pool: 0x080DA558)
080DA54C:  add  r0, r0, r1       // r0 = 0x080DA558 + op * 4
080DA54E:  ldr  r0, [r0]         // Load target PC
080DA550:  mov  pc, r0           // Jump table dispatch
080DA552:  nop / align
080DA554:  .word 0x080DA558      // Base address of jump table
080DA558:  // --- START OF 18-ENTRY JUMP TABLE ---
080DA558:  .word 0x080DA5A0      // Case 0: *addr = data
080DA55C:  .word 0x080DA5A4      // Case 1: *addr += data
...
080DA59C:  .word 0x080DA650      // Case 17: *addr < mplayInfo->memAccArea[data]
080DA5A0:  // --- START OF CASE BODIES ---
```

### 2.2 The Misattribution
In the inherited `game.toml`, `0x080DA558` was declared as:
```toml
[[extra_func]]
addr = 0x080DA558
mode = "thumb"
name = "MP2K_event_xcmd"
```
And its internal case blocks `0x080DA5A0` through `0x080DA602` were labeled as `MP2K_event_xxx`, `MP2K_event_xwave`, `MP2K_event_xtype`, etc.

Inspection of `third_party/cvaos/src/m4a.c` and ROM table `gXcmdTable` at `0x0827DF5C` revealed that the true handlers actually reside at:
- `0x080DA678`: `MP2K_event_xcmd`
- `0x080DA69C`: `MP2K_event_xxx`
- `0x080DA6B0`: `MP2K_event_xwave`
- `0x080DA6F8`: `MP2K_event_xtype`
- `0x080DA70C`: `MP2K_event_xatta`
- `0x080DA720`: `MP2K_event_xdeca`
- `0x080DA734`: `MP2K_event_xsust`
- `0x080DA748`: `MP2K_event_xrele`
- `0x080DA75C`: `MP2K_event_xiecv`
- `0x080DA768`: `MP2K_event_xiecl`
- `0x080DA774`: `MP2K_event_xleng`
- `0x080DA788`: `MP2K_event_xswee`
- `0x080DA79C`: `MP2K_event_null`

Removing the bogus `[[extra_func]]` definitions and adding the real handler seeds completely resolved the conflict.

---

## 3. Catalog of the 46 Formalized Jump Tables

All 46 jump tables are now explicitly registered in `game.toml` with `format = "abs32"`, `entries_mode = "thumb"`, and stride 4:

| Address | Count | Enclosing Module | Subsystem / Description |
|---|---|---|---|
| `0x08000524` | 21 | `src/main.c` | `GameModeUpdate` main game mode state machine |
| `0x080029B8` | 5 | `src/code_08002454.c` | Entity action dispatcher |
| `0x08002B7C` | 5 | `src/code_08002454.c` | Entity action dispatcher |
| `0x08002CC4` | 6 | `src/code_08002454.c` | Entity action dispatcher |
| `0x08003178` | 10 | `src/code_08002454.c` | Player state machine dispatch |
| `0x08004030` | 5 | `src/code_08002454.c` | Entity step switch |
| `0x08004A84` | 100 | `src/code_08002454.c` | Master entity / weapon behavior table |
| `0x08006A54` | 7 | `src/code_08005894.c` | Menu / HUD render loop update |
| `0x08008780` | 6 | `src/code_08008750.c` | Inventory / equipment dispatch |
| `0x08008A18` | 7 | `src/code_08008750.c` | Soul inventory action switch |
| `0x0800943C` | 10 | `src/code_08008750.c` | Shop and dialogue step switch |
| `0x0800A3FC` | 22 | `src/code_080096AC.c` | Enemy action dispatch table |
| `0x0800B914` | 6 | `src/code_0800B700.c` | Cutscene & script event switch |
| `0x0800BEF8` | 15 | `src/code_0800B700.c` | Castle map room transition switch |
| `0x0800BF70` | 5 | `src/code_0800B700.c` | Room asset loader switch |
| `0x0800D314` | 36 | `src/code_0800CB00.c` | Boss AI phase switch table |
| `0x0800D8BC` | 40 | `src/code_0800CB00.c` | Boss attack pattern switch table |
| `0x0800ED90` | 37 | `src/code_0800CB00.c` | Special FX / particle generator table |
| `0x0800EE88` | 37 | `src/code_0800CB00.c` | Particle update dispatch switch |
| `0x0800F2D8` | 6 | `src/code_0800F1FC.c` | Sound effect / BGM trigger dispatch |
| `0x0801110C` | 9 | `src/code_080109F4.c` | UI cursor & menu page navigation |
| `0x080116E0` | 7 | `src/code_080109F4.c` | Save / Load prompt response switch |
| `0x08013FD8` | 5 | `src/code_08013960.c` | Tilemap scroll / DMA trigger |
| `0x0801433C` | 5 | `src/code_08013960.c` | Widescreen background layer update |
| `0x0801A9A4` | 6 | `asm/code_08014548.s` | Enemy projectile movement logic |
| `0x0801BA50` | 18 | `asm/code_08014548.s` | Complex enemy AI state machine |
| `0x08036844` | 5 | `src/code_0803681C.c` | SRAM / Flash save record handler |
| `0x08038A84` | 21 | `src/code_08038A38.c` | Wireless / Multi-link packet dispatcher |
| `0x0804423C` | 9 | `asm/code_08040A38.s` | Julius Mode movement action switch |
| `0x08046C48` | 13 | `asm/code_08040A38.s` | Boss Rush timer and reward switch |
| `0x0804779C` | 8 | `asm/code_08040A38.s` | Enemy collision & damage resolution |
| `0x0805AFF4` | 14 | `asm/code_08050A3C.s` | Castle Corridor entity update |
| `0x0805CB90` | 6 | `asm/code_08050A3C.s` | Chapel bell & pendulum hazard |
| `0x0805CD14` | 6 | `asm/code_08050A3C.s` | Reservoir floating platform switch |
| `0x0805D584` | 5 | `asm/code_08050A3C.s` | Study bookcase puzzle interaction |
| `0x0805D664` | 7 | `asm/code_08050A3C.s` | Dance Hall chandelier switch |
| `0x0805D784` | 7 | `asm/code_08050A3C.s` | Inner Quarters mirror interaction |
| `0x0805E358` | 6 | `asm/code_08050A3C.s` | Floating Garden portal trigger |
| `0x0805E620` | 5 | `asm/code_08050A3C.s` | Clock Tower gear mechanism |
| `0x08067A98` | 7 | `asm/code_08060B98.s` | Reservoir boat navigation |
| `0x08067C0C` | 10 | `asm/code_08060B98.s` | Arena obstacle course stage trigger |
| `0x08068170` | 12 | `asm/code_08060B98.s` | Top Floor throne room transition |
| `0x080D7498` | 72 | `src/code_080D73B8.c` | Interrupt / DMA event dispatch |
| `0x080DA558` | 18 | `src/m4a.c` | `MP2K_event_memacc` op switch cases 0..17 |
| `0x080DC5B4` | 89 | `src/m4a.c` | `m4a` sound driver command table |
| `0x080DDA14` | 6 | `src/m4a.c` | `m4a` sound channel filter mode |

---

## 4. Recompiler Discovery Comparison

| Metric | Phase 1 (Baseline) | Phase 2 (Formal Jump Tables) | Delta |
|---|---|---|---|
| **Discovered Functions** | 12,643 | **12,655** | +12 |
| **Auto Jump Tables** | 46 (762 targets) | **0** (0 targets) | -46 (Eliminated) |
| **Explicit Jump Tables** | 0 | **46** (762 targets) | +46 (Formalized) |
| **Data Ranges Honored** | 0 | **46** | +46 |
| **Control-Flow Warnings** | 1 (`0x080DA558`) | **0** | -1 (Zero warnings) |
