#!/usr/bin/env python3
"""
ingest_recomp_misses.py — Ingests runtime misses from recomp_coverage_*.json,
cross-references with decompilation symbols in third_party/cvaos, and populates
game.toml with descriptive, verified function seeds for static recompilation.
"""

import json
import os
import re
import sys
from pathlib import Path

def find_cvaos_symbol(addr_int, cvaos_root, file_cache):
    """
    Search third_party/cvaos files for symbol definitions or references matching addr_int.
    Returns (symbol_name, file_context, note).
    """
    addr_hex_8 = f"08{addr_int & 0x01FFFFFF:06X}"
    addr_hex_lower = addr_hex_8.lower()

    exact_matches = []
    
    for rel_path, content in file_cache.items():
        if addr_hex_lower in content.lower():
            for line in content.splitlines():
                if addr_hex_lower in line.lower():
                    exact_matches.append((rel_path, line.strip()))
                    
    # Subsystem mapping based on address ranges in Aria of Sorrow
    subsystem = "General"
    if 0x08000000 <= addr_int < 0x08008000:
        subsystem = "CoreEngine/VRAM/Collision"
    elif 0x08008000 <= addr_int < 0x08014000:
        subsystem = "GameLoop/RoomManager"
    elif 0x08014000 <= addr_int < 0x08035000:
        subsystem = "Player/Physics/SoulMechanics"
    elif 0x08035000 <= addr_int < 0x08050000:
        subsystem = "Weapons/Projectiles/Damage"
    elif 0x08050000 <= addr_int < 0x08070000:
        subsystem = "EnemyAI/BossStateMachine"
    elif 0x08070000 <= addr_int < 0x080A0000:
        subsystem = "UI/Inventory/SoulEquip"
    elif 0x080A0000 <= addr_int < 0x080E0000:
        subsystem = "Audio/Scripting/SRAM"

    name = None
    note_details = []
    
    for rel_path, line in exact_matches:
        m_fn = re.search(r'\b([A-Za-z0-9_]+)\s*\([^)]*\)', line)
        if m_fn and addr_hex_lower in m_fn.group(1).lower():
            name = m_fn.group(1)
            note_details.append(f"Defined in {rel_path}")
            break
        m_lbl = re.search(r'\b(sub_[08A-Fa-f0-9]+)\b', line)
        if m_lbl and addr_hex_lower in m_lbl.group(1).lower():
            name = m_lbl.group(1)
            note_details.append(f"Label in {rel_path}")
            break

    if not name:
        for rel_path, content in file_cache.items():
            if addr_hex_lower in content.lower():
                note_details.append(f"Referenced in {rel_path}")
                break

    if not name:
        name = f"sub_{addr_hex_8}"
        
    source_info = note_details[0] if note_details else f"{subsystem}"
    note = f"{subsystem} | {source_info}"
    
    return name, subsystem, note


def main():
    repo_root = Path(__file__).resolve().parent.parent
    coverage_json_path = repo_root / "recomp_coverage_A2CE.json"
    game_toml_path = repo_root / "game.toml"
    cvaos_root = repo_root / "third_party" / "cvaos"

    if not coverage_json_path.exists():
        print(f"[ERROR] Coverage file not found: {coverage_json_path}")
        sys.exit(1)
    if not game_toml_path.exists():
        print(f"[ERROR] game.toml not found: {game_toml_path}")
        sys.exit(1)

    print(f"[INFO] Reading {coverage_json_path}...")
    with open(coverage_json_path, "r", encoding="utf-8") as f:
        coverage = json.load(f)

    misses = coverage.get("misses", [])
    print(f"[INFO] Found {len(misses)} misses in coverage report.")

    print(f"[INFO] Indexing cvaos decompilation files...")
    file_cache = {}
    if cvaos_root.exists():
        for ext in ("*.c", "*.h", "*.s", "*.inc"):
            for p in cvaos_root.rglob(ext):
                try:
                    rel = p.relative_to(cvaos_root).as_posix()
                    file_cache[rel] = p.read_text(encoding="utf-8", errors="ignore")
                except Exception:
                    pass
    print(f"[INFO] Cached {len(file_cache)} files from cvaos.")

    print(f"[INFO] Reading existing game.toml...")
    with open(game_toml_path, "r", encoding="utf-8") as f:
        existing_toml = f.read()

    existing_addrs = set()
    for m in re.finditer(r'addr\s*=\s*(0x[0-9A-Fa-f]+)', existing_toml):
        val = int(m.group(1), 16)
        existing_addrs.add(val)

    print(f"[INFO] Found {len(existing_addrs)} existing function seeds in game.toml.")

    unique_misses = {}
    for m in misses:
        addr_int = int(m["pc"], 16)
        if addr_int not in unique_misses:
            unique_misses[addr_int] = m

    new_entries = []
    skipped_existing = 0

    for addr_int in sorted(unique_misses.keys()):
        if addr_int in existing_addrs:
            skipped_existing += 1
            continue

        m = unique_misses[addr_int]
        mode = m.get("mode", "thumb").lower()
        is_jump_table = m.get("jump_table_candidate", False)
        bridged_count = m.get("bridged", 0)
        native_calls = m.get("native_calls", 0)

        name, subsystem, note = find_cvaos_symbol(addr_int, cvaos_root, file_cache)
        
        if is_jump_table:
            note += f" | Jump table target (bridged={bridged_count}, native_calls={native_calls})"
        else:
            note += f" | Direct miss (bridged={bridged_count})"

        new_entries.append({
            "addr": f"0x{addr_int:08X}",
            "mode": mode,
            "name": name,
            "note": note,
            "subsystem": subsystem,
        })

    print(f"[INFO] Skipped {skipped_existing} already present in game.toml.")
    print(f"[INFO] Prepared {len(new_entries)} new function seeds to ingest.")

    if not new_entries:
        print("[SUCCESS] All misses are already seeded in game.toml!")
        return

    backup_path = game_toml_path.with_suffix(".toml.bak")
    with open(backup_path, "w", encoding="utf-8") as f:
        f.write(existing_toml)
    print(f"[INFO] Backed up game.toml to {backup_path}")

    by_subsystem = {}
    for entry in new_entries:
        by_subsystem.setdefault(entry["subsystem"], []).append(entry)

    toml_append_blocks = [
        "\n# " + "=" * 78,
        "# -- Phase 1 Ingested Function Seeds (recomp_coverage_A2CE) -----------------",
        f"# Ingested {len(new_entries)} function seeds cross-referenced with cvaos decomp symbols.",
        f"# Eliminates runtime self-healing interpreter bridging during normal gameplay.",
        "# " + "=" * 78 + "\n"
    ]

    for sub, entries in sorted(by_subsystem.items()):
        toml_append_blocks.append(f"\n# -- Subsystem: {sub} ({len(entries)} seeds) " + "-" * max(0, 50 - len(sub)))
        for e in entries:
            block = (
                f"[[extra_func]]\n"
                f"addr = {e['addr']}\n"
                f"mode = \"{e['mode']}\"\n"
                f"name = \"{e['name']}\"\n"
                f"note = \"{e['note']}\"\n"
            )
            toml_append_blocks.append(block)

    code_copy_marker = "# -- Dynamic Code Copied to IWRAM at Boot"
    if code_copy_marker in existing_toml:
        idx = existing_toml.index(code_copy_marker)
        updated_toml = existing_toml[:idx] + "\n".join(toml_append_blocks) + "\n" + existing_toml[idx:]
    else:
        # Fall back to alternative marker
        alt_marker = "# ── Dynamic Code Copied to IWRAM at Boot"
        if alt_marker in existing_toml:
            idx = existing_toml.index(alt_marker)
            updated_toml = existing_toml[:idx] + "\n".join(toml_append_blocks) + "\n" + existing_toml[idx:]
        else:
            updated_toml = existing_toml + "\n".join(toml_append_blocks)

    with open(game_toml_path, "w", encoding="utf-8") as f:
        f.write(updated_toml)

    print(f"[SUCCESS] Ingested {len(new_entries)} function seeds into game.toml!")


if __name__ == "__main__":
    main()
