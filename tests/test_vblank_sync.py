#!/usr/bin/env python3
"""Regression test: VBlankIntrWait must re-phase the guest to VBlank.

Aria of Sorrow calls VBlankIntrWait (SWI 0x05) once per frame. A correct
implementation returns at the next VBlank start, so consecutive calls are
exactly one GBA frame (280,896 cycles) apart and land at a stable scanline.

The original HLE advanced a fixed 280,896 cycles instead of waiting for the
event. Ticking exactly one frame period preserves the raster phase, so the
guest resumed at the scanline it called from and its own per-frame workload
(~19,945 cycles) became permanent phase drift: every per-frame PPU register
write landed ~16 scanlines lower than the last, producing a horizontal seam
that marched down the screen during fades and in-game raster effects.

Run from the repository root, where aria_recomp.exe and the ROM live.
"""
import os
import subprocess
import sys
import tempfile

CYCLES_PER_FRAME = 280896
CYCLES_PER_SCANLINE = 1232
SWI_VBLANK_INTR_WAIT = 5

# One frame, within 1%. The guest's own workload must be absorbed by the wait,
# not added to it.
GAP_TOLERANCE = CYCLES_PER_FRAME * 0.01
# A stable phase wanders by well under a scanline; the bug drifted 16 per frame.
MAX_SCANLINE_DRIFT = 4.0

EXE = "./aria_recomp.exe"
FRAMES = 470


def collect_vblank_waits(log_path):
    """Return [(cycles, scanline)] for each VBlankIntrWait call."""
    out = []
    with open(log_path, "r", encoding="utf-8") as handle:
        next(handle)  # header: seq,cycles,imm,ret,r0,r1,r2,lr,iwflags
        for line in handle:
            if not line.strip():
                continue
            fields = line.split(",")
            if int(fields[2]) != SWI_VBLANK_INTR_WAIT:
                continue
            cycles = int(fields[1])
            out.append((cycles, (cycles % CYCLES_PER_FRAME) // CYCLES_PER_SCANLINE))
    return out


def main():
    if not os.path.exists(EXE):
        print(f"[SKIP] {EXE} not built")
        return 0

    with tempfile.TemporaryDirectory() as tmp:
        log = os.path.join(tmp, "swi.csv")
        env = dict(os.environ, GBARECOMP_SWI_LOG=log)
        proc = subprocess.run(
            [EXE, "--frames", str(FRAMES), "--no-window"],
            env=env, capture_output=True, text=True, timeout=600,
        )
        if not os.path.exists(log):
            print("[FAIL] no SWI log produced")
            print(proc.stdout[-2000:])
            return 1
        waits = collect_vblank_waits(log)

    if len(waits) < 50:
        print(f"[FAIL] only {len(waits)} VBlankIntrWait calls in {FRAMES} frames; "
              "expected roughly one per frame")
        return 1

    # Use a contiguous mid-run window: startup and scene changes legitimately
    # break the once-per-frame cadence.
    window = waits[len(waits) // 3: len(waits) // 3 + 30]
    gaps = [b[0] - a[0] for a, b in zip(window, window[1:])]
    gaps.sort()
    median_gap = gaps[len(gaps) // 2]

    # Derive drift from the gap rather than differencing the raw scanline: the
    # scanline wraps modulo 228 inside the window, so endpoint differencing
    # silently reports a near-zero slope for a badly drifting phase.
    drift = (median_gap - CYCLES_PER_FRAME) / CYCLES_PER_SCANLINE

    print(f"VBlankIntrWait calls: {len(waits)} in {FRAMES} frames")
    print(f"median gap: {median_gap} cycles (one frame = {CYCLES_PER_FRAME})")
    print(f"mean scanline drift: {drift:+.2f} lines/frame")

    failures = []
    if abs(median_gap - CYCLES_PER_FRAME) > GAP_TOLERANCE:
        failures.append(
            f"gap {median_gap} is {median_gap - CYCLES_PER_FRAME:+d} cycles off one "
            f"frame ({(median_gap - CYCLES_PER_FRAME) / CYCLES_PER_SCANLINE:+.1f} "
            "scanlines) — the wait is not re-phasing to VBlank")
    if abs(drift) > MAX_SCANLINE_DRIFT:
        failures.append(
            f"per-frame register writes drift {drift:+.2f} scanlines/frame — "
            "raster effects and fades will tear")

    if failures:
        for f in failures:
            print(f"[FAIL] {f}")
        return 1

    print("[PASS] VBlankIntrWait is phase-locked to VBlank")
    return 0


if __name__ == "__main__":
    sys.exit(main())
