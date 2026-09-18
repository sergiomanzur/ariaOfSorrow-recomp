import socket
import json
import time
import subprocess
import os
import struct
import zlib
import sys

def write_png(path: str, rgb: bytes, w=240, h=160):
    raw = bytearray()
    row_bytes = w * 3
    for y in range(h):
        raw.append(0)  # filter type 0
        raw.extend(rgb[y * row_bytes : (y + 1) * row_bytes])
    compressed = zlib.compress(bytes(raw), level=6)

    def chunk(tag: bytes, data: bytes) -> bytes:
        crc = zlib.crc32(tag + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)

    ihdr = struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", ihdr))
        f.write(chunk(b"IDAT", compressed))
        f.write(chunk(b"IEND", b""))

def run_verification():
    port = 19865
    exe_path = os.path.abspath("build/aria_recomp.exe")
    rom_path = os.path.abspath("Castlevania - Aria of Sorrow (USA).gba")
    artifact_dir = r"C:\Users\sergi\.gemini\antigravity\brain\f8dc8aee-0a16-43e4-8818-1fd196e81748"

    cmd = [exe_path, "--tcp", str(port), "--bios-hle", "--bios-skip-intro", "--rom", rom_path]
    print(f"[VERIFY] Starting game in headless mode: {' '.join(cmd)}")
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(20.0)
    connected = False
    for attempt in range(20):
        time.sleep(0.5)
        try:
            s.connect(("127.0.0.1", port))
            connected = True
            print(f"[VERIFY] Connected to game TCP port {port}")
            break
        except OSError:
            pass

    if not connected:
        proc.kill()
        raise RuntimeError(f"Could not connect to game on port {port}")

    test_results = {}

    try:
        def send_cmd(cmd_dict):
            payload = json.dumps(cmd_dict) + "\n"
            s.sendall(payload.encode('utf-8'))
            resp = b""
            while not resp.endswith(b"\n"):
                chunk = s.recv(4096)
                if not chunk:
                    break
                resp += chunk
            return json.loads(resp.decode('utf-8').strip())

        def pause_and_wait():
            send_cmd({"cmd": "pause"})
            for _ in range(60):
                st = send_cmd({"cmd": "run_status"})
                if st.get("parked"):
                    return st
                time.sleep(0.03)
            return send_cmd({"cmd": "run_status"})

        def press_buttons(key_mask, duration_sec=0.25):
            send_cmd({"cmd": "set_keyinput", "value": key_mask})
            send_cmd({"cmd": "continue"})
            time.sleep(duration_sec)
            pause_and_wait()
            send_cmd({"cmd": "set_keyinput", "value": 0x03FF})
            send_cmd({"cmd": "continue"})
            time.sleep(0.08)
            pause_and_wait()

        def take_screenshot(filename):
            shot = send_cmd({"cmd": "screenshot"})
            if shot.get("ok"):
                raw_bytes = bytes.fromhex(shot["data"])
                out_path = os.path.join(artifact_dir, filename)
                write_png(out_path, raw_bytes, w=shot.get("w", 240), h=shot.get("h", 160))
                print(f"[SCREENSHOT] Saved: {out_path} ({shot.get('w')}x{shot.get('h')})")
                return out_path
            return None

        # 1. Boot up through Title Screen and File 1
        print("\n--- STEP 1: Booting and Navigating to In-Game Gameplay ---")
        send_cmd({"cmd": "continue"})
        time.sleep(3.5)
        pause_and_wait()
        press_buttons(0x03F7, 0.35)  # START
        time.sleep(0.5)
        pause_and_wait()
        press_buttons(0x03FE, 0.3)   # A on Title Screen
        time.sleep(0.6)
        pause_and_wait()
        press_buttons(0x03FE, 0.3)   # A on File 1
        time.sleep(0.8)
        pause_and_wait()

        # Advance through intro text
        for _ in range(6):
            press_buttons(0x03F7, 0.2)
            time.sleep(0.15)

        st = pause_and_wait()
        print(f"[VERIFY] In-game active at guest frame {st.get('frame')}")
        take_screenshot("29_headless_initial_castle.png")

        # 2. Verify Initial Game State
        print("\n--- STEP 2: Verifying Initial In-Game State ---")
        state0 = send_cmd({"cmd": "get_game_state"})
        print(f"[INITIAL STATE] {json.dumps(state0, indent=2)}")
        assert state0.get("ok") is True
        assert state0.get("level") == 1
        assert state0.get("hp") == 320
        assert state0.get("max_hp") == 320
        assert state0.get("mp") == 80
        assert state0.get("max_mp") == 80
        assert state0.get("weapon") == 0  # Knife (0-indexed)
        assert state0.get("owned_weapons") == 1
        test_results["initial_state"] = state0

        # 3. Test Level Changing (GrantSystem LevelUpTo & LevelUpOne)
        print("\n--- STEP 3: Testing Level Changing (Set Level to 25) ---")
        res_lvl = send_cmd({"cmd": "set_level", "level": 25})
        print(f"[SET LEVEL RESPONSE] {res_lvl}")
        assert res_lvl.get("ok") is True

        # Let guest frame run so ApplyPending processes the grant
        send_cmd({"cmd": "continue"})
        time.sleep(0.25)
        pause_and_wait()

        state_lvl25 = send_cmd({"cmd": "get_game_state"})
        print(f"[LEVEL 25 STATE] {json.dumps(state_lvl25, indent=2)}")
        assert state_lvl25.get("level") == 25
        assert state_lvl25.get("max_hp") > 320
        assert state_lvl25.get("max_mp") > 80
        assert state_lvl25.get("hp") == state_lvl25.get("max_hp")  # refilled on level-up
        assert state_lvl25.get("mp") == state_lvl25.get("max_mp")  # refilled on level-up
        assert state_lvl25.get("str") > state0.get("str")
        assert state_lvl25.get("con") > state0.get("con")
        assert state_lvl25.get("int") > state0.get("int")
        test_results["level_25"] = state_lvl25

        # Test single level up
        print("\n--- Testing Single Level-Up Increment (Level 25 -> 26) ---")
        send_cmd({"cmd": "trigger_grant", "type": "level_up_one"})
        send_cmd({"cmd": "continue"})
        time.sleep(0.25)
        pause_and_wait()
        state_lvl26 = send_cmd({"cmd": "get_game_state"})
        assert state_lvl26.get("level") == 26
        assert state_lvl26.get("max_hp") >= state_lvl25.get("max_hp")
        print(f"[LEVEL 26 CONFIRMED] Level={state_lvl26.get('level')}, MaxHP={state_lvl26.get('max_hp')}, MaxMP={state_lvl26.get('max_mp')}")
        take_screenshot("30_headless_level_25_stats.png")

        # 4. Test Item & Weapon Grants
        print("\n--- STEP 4: Testing Item & Weapon Grants ---")
        send_cmd({"cmd": "trigger_grant", "type": "weapons"})
        send_cmd({"cmd": "trigger_grant", "type": "consumables"})
        send_cmd({"cmd": "trigger_grant", "type": "armor"})
        send_cmd({"cmd": "continue"})
        time.sleep(0.3)
        pause_and_wait()

        state_items = send_cmd({"cmd": "get_game_state"})
        print(f"[INVENTORY AFTER GRANTS] Owned Weapons={state_items.get('owned_weapons')}, Owned Consumables={state_items.get('owned_items')}")
        assert state_items.get("owned_weapons") == 59  # All 59 weapons
        assert state_items.get("owned_items") == 32    # All 32 consumables
        test_results["items_granted"] = {
            "owned_weapons": state_items.get("owned_weapons"),
            "owned_items": state_items.get("owned_items")
        }
        take_screenshot("31_headless_all_items_weapons.png")

        # 5. Test Soul Grants (Bullet, Guardian, Enchant, Ability)
        print("\n--- STEP 5: Testing Soul Grants ---")
        send_cmd({"cmd": "trigger_grant", "type": "bullet_souls"})
        send_cmd({"cmd": "trigger_grant", "type": "guardian_souls"})
        send_cmd({"cmd": "trigger_grant", "type": "enchant_souls"})
        send_cmd({"cmd": "trigger_grant", "type": "ability_souls"})
        send_cmd({"cmd": "continue"})
        time.sleep(0.35)
        pause_and_wait()

        state_souls = send_cmd({"cmd": "get_game_state"})
        print(f"[SOULS AFTER GRANTS] Total Souls Collected Counter={state_souls.get('total_souls')}")
        assert state_souls.get("total_souls") > 100
        test_results["souls_granted"] = state_souls.get("total_souls")
        take_screenshot("32_headless_all_souls_granted.png")

        # 6. Test Map Reveal Grant
        print("\n--- STEP 6: Testing Reveal Map Grant ---")
        send_cmd({"cmd": "trigger_grant", "type": "reveal_map"})
        send_cmd({"cmd": "continue"})
        time.sleep(0.25)
        pause_and_wait()

        map_sample = send_cmd({"cmd": "read_ewram", "addr": 0x02013398, "len": 16})
        print(f"[MAP EWRAM AFTER REVEAL] {map_sample.get('data')}")
        assert any(b != '0' for b in map_sample.get("data", ""))
        test_results["map_revealed"] = True

        # 7. Test Cheats: Infinite HP, Infinite MP, Invincibility
        print("\n--- STEP 7: Testing Cheats (Infinite HP/MP & Invincibility) ---")
        send_cmd({"cmd": "set_cheat", "infinite_hp": True, "infinite_mp": True, "invincibility": True})
        send_cmd({"cmd": "continue"})
        time.sleep(0.2)
        pause_and_wait()

        state_cheat1 = send_cmd({"cmd": "get_game_state"})
        assert state_cheat1.get("invincibility") is True
        print(f"[INVINCIBILITY CONFIRMED] Flag active in memory={state_cheat1.get('invincibility')}")

        # Artificially deplete HP and MP via write_ewram
        cur_max_hp = state_cheat1.get("max_hp")
        cur_max_mp = state_cheat1.get("max_mp")
        print(f"[DEPLETING HP/MP] Manually writing HP=45 (0x002D) and MP=12 (0x000C)...")
        send_cmd({"cmd": "write_ewram", "addr": 0x0201327A, "data": "2d00"}) # HP = 45
        send_cmd({"cmd": "write_ewram", "addr": 0x0201327C, "data": "0c00"}) # MP = 12

        ew_hp = send_cmd({"cmd": "read_ewram", "addr": 0x0201327A, "len": 2})
        print(f"[EWRAM WRITTEN] raw HP data={ew_hp.get('data')}")
        assert ew_hp.get("data") == "2d00"

        # Advance frames: CheatSystem's ApplyFrameCheats should immediately restore HP and MP back to max!
        send_cmd({"cmd": "continue"})
        time.sleep(0.2)
        pause_and_wait()

        state_cheat2 = send_cmd({"cmd": "get_game_state"})
        print(f"[INFINITE HP/MP CONFIRMED] Restored HP={state_cheat2.get('hp')} (Max={cur_max_hp}), MP={state_cheat2.get('mp')} (Max={cur_max_mp})")
        assert state_cheat2.get("hp") == cur_max_hp
        assert state_cheat2.get("mp") == cur_max_mp
        test_results["infinite_hp_mp_verified"] = True
        take_screenshot("33_headless_infinite_hp_mp.png")

        # 8. Test Quick Loadouts Swapping
        print("\n--- STEP 8: Testing Quick Loadout Swapping In-Game ---")
        res_cycle = send_cmd({"cmd": "cycle_loadout"})
        assert res_cycle.get("ok") is True
        send_cmd({"cmd": "continue"})
        time.sleep(0.2)
        pause_and_wait()

        state_loadout1 = send_cmd({"cmd": "get_game_state"})
        print(f"[LOADOUT CYCLED] Current weapon slot state: {state_loadout1.get('weapon')}")
        take_screenshot("34_headless_quick_loadout_active.png")

        # Cycle back
        send_cmd({"cmd": "cycle_loadout"})
        send_cmd({"cmd": "continue"})
        time.sleep(0.2)
        pause_and_wait()

        # 9. Test Recompilation Misses & Stability
        print("\n--- STEP 9: Recompilation Coverage Check ---")
        misses = send_cmd({"cmd": "misses"})
        print(f"[RECOMP COVERAGE] distinct_misses={misses.get('distinct_misses')}, interpreted={misses.get('interpreted_insns')}")
        assert misses.get("distinct_misses") == 0
        assert misses.get("interpreted_insns") == 0

        print("\n========================================================")
        print("  ALL GAMEPLAY CHEAT & QOL VERIFICATIONS PASSED 100%!   ")
        print("========================================================")

        return True

    finally:
        try:
            send_cmd({"cmd": "quit"})
        except Exception:
            pass
        s.close()
        proc.terminate()
        try:
            proc.wait(timeout=2.0)
        except subprocess.TimeoutExpired:
            proc.kill()

if __name__ == "__main__":
    success = run_verification()
    sys.exit(0 if success else 1)
