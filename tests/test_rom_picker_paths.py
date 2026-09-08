#!/usr/bin/env python3
"""Regression test: launcher state must follow the executable, not the cwd.

A double-click from Explorer happens to set the working directory to the exe's
folder, so cwd-relative state appears to work. A shortcut with "Start in" set,
Steam, or any other launcher does not -- and then aria_config.ini is not found,
the ROM picker re-prompts, and a fresh config plus bios/ and bios.cfg are
littered into whatever directory the process started in.

This drives the real install layout (the repo root, where aria_recomp.exe
lives) with the working directory pointed somewhere else, and asserts the
saved ROM is still picked up without prompting.

The subprocess is run under a timeout because the failure mode is an
interactive modal dialog: without the timeout a regression hangs forever
instead of failing.
"""
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
INSTALL_DIR = os.path.abspath(os.path.join(HERE, ".."))
EXE = os.path.join(INSTALL_DIR, "aria_recomp.exe")
CONFIG = os.path.join(INSTALL_DIR, "aria_config.ini")
ROM = os.path.join(INSTALL_DIR, "Castlevania - Aria of Sorrow (USA).gba")
TIMEOUT = 180


def write_config(path, rom_path):
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(
            "# AriaRecomp Configuration File\n\n"
            "[Paths]\n"
            f"romPath = {rom_path}\n"
            "biosPath = \n"
            "saveDirectory = \n"
        )


def read_rom_path(path):
    if not os.path.exists(path):
        return None
    with open(path, "r", encoding="utf-8") as handle:
        for line in handle:
            if line.strip().startswith("romPath"):
                return line.split("=", 1)[1].strip()
    return None


def main():
    if not os.path.exists(EXE):
        print(f"[SKIP] {EXE} not built")
        return 0
    if not os.path.exists(ROM):
        print(f"[SKIP] ROM not present at {ROM}")
        return 0

    backup = None
    if os.path.exists(CONFIG):
        backup = CONFIG + ".testbak"
        shutil.copy2(CONFIG, backup)

    failures = []
    try:
        # The install directory holds a config naming the ROM by absolute path.
        write_config(CONFIG, ROM)

        with tempfile.TemporaryDirectory() as foreign_cwd:
            proc = subprocess.Popen(
                [EXE, "--frames", "2", "--no-window"],
                cwd=foreign_cwd,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
            )
            try:
                out, _ = proc.communicate(timeout=TIMEOUT)
            except subprocess.TimeoutExpired:
                proc.kill()
                out, _ = proc.communicate()
                failures.append(
                    "timed out -- the ROM picker almost certainly opened a modal "
                    "dialog because aria_config.ini was looked up in the working "
                    "directory instead of next to the executable")
                out = out or ""

            if "Prompting user" in out:
                failures.append(
                    "prompted for the ROM even though the install directory's "
                    "config names it")
            if not failures and "Loaded configuration from" not in out:
                failures.append(
                    "did not load the install directory's aria_config.ini")

            littered = [name for name in ("aria_config.ini", "bios.cfg", "bios")
                        if os.path.exists(os.path.join(foreign_cwd, name))]
            if littered:
                failures.append(
                    f"wrote launcher state into the working directory: {littered}")

        saved = read_rom_path(CONFIG)
        if saved and not os.path.isabs(saved):
            failures.append(
                f"persisted a relative romPath ({saved!r}); it cannot survive a "
                "launch from another working directory")
    finally:
        if backup:
            shutil.move(backup, CONFIG)
        elif os.path.exists(CONFIG):
            os.remove(CONFIG)

    if failures:
        for f in failures:
            print(f"[FAIL] {f}")
        return 1
    print("[PASS] launcher state follows the executable, not the cwd")
    return 0


if __name__ == "__main__":
    sys.exit(main())
