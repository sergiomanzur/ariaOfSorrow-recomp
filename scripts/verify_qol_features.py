import socket
import json
import time
import subprocess
import os
import struct
import zlib

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

def main():
    port = 19862
    exe_path = os.path.abspath("build/aria_recomp.exe")
    rom_path = os.path.abspath("Castlevania - Aria of Sorrow (USA).gba")
    artifact_dir = r"C:\Users\sergi\.gemini\antigravity\brain\f8dc8aee-0a16-43e4-8818-1fd196e81748"

    cmd = [exe_path, "--tcp", str(port), "--bios-hle", "--bios-skip-intro", "--rom", rom_path]
    print(f"[VERIFY] Starting game: {' '.join(cmd)}")
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(15.0)
    connected = False
    for attempt in range(15):
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
            for _ in range(50):
                st = send_cmd({"cmd": "run_status"})
                if st.get("parked"):
                    return st
                time.sleep(0.04)
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

        # 1. Boot up to Title Screen
        print("[VERIFY] Booting to Title Screen...")
        send_cmd({"cmd": "continue"})
        time.sleep(4.0)
        pause_and_wait()
        press_buttons(0x03F7, 0.4) # Press START
        time.sleep(0.5)
        pause_and_wait()
        take_screenshot("16_title_screen.png")

        # 2. Enter File Select & Start Game
        print("[VERIFY] Entering File 1...")
        press_buttons(0x03FE, 0.3) # Press A
        time.sleep(0.6)
        pause_and_wait()
        press_buttons(0x03FE, 0.3) # Press A on File 1
        time.sleep(0.8)
        pause_and_wait()

        # Skip intro dialog
        for _ in range(5):
            press_buttons(0x03F7, 0.2)
            time.sleep(0.2)

        st = pause_and_wait()
        print(f"[VERIFY] In-game active at frame {st.get('frame')}")
        take_screenshot("17_soma_castle_corridor.png")

        # 3. Test Quick Loadout Presets ("Dawn of Sorrow" Fix)
        print("[VERIFY] Testing Quick Loadouts via GBA Combo (L + Select)...")
        # Read starting weapon & souls
        ew_weapon = send_cmd({"cmd": "read_ewram", "addr": 0x02013268, "len": 1})
        ew_red = send_cmd({"cmd": "read_ewram", "addr": 0x02013269, "len": 1})
        print(f"[VERIFY] Preset 1 initial gear: Weapon={ew_weapon.get('data')}, RedSoul={ew_red.get('data')}")

        # Trigger L + Select combo (L is bit 9 = 0x0200, Select is bit 2 = 0x0004)
        # Active low: ~(0x0200 | 0x0004) = 0x01FB
        press_buttons(0x01FB, 0.3)
        time.sleep(0.2)
        pause_and_wait()
        take_screenshot("18_quick_loadout_swapped.png")

        # 4. Movement and Combat Actions
        print("[VERIFY] Walking and jumping in castle corridor...")
        press_buttons(0x03EF, 0.5) # Walk right
        press_buttons(0x03FE, 0.3) # Jump
        press_buttons(0x03FD, 0.2) # Knife slash
        take_screenshot("19_soma_combat_knife.png")

        # 5. Fast Room Transition Verification
        print("[VERIFY] Moving right to room transition...")
        for _ in range(3):
            press_buttons(0x03EF, 0.6) # Hold Right
        take_screenshot("20_castle_door_transition.png")

        # 6. Verify Misses & Stability
        misses = send_cmd({"cmd": "misses"})
        print(f"[VERIFY] Recompilation coverage check: distinct_misses={misses.get('distinct_misses')}, interpreted={misses.get('interpreted_insns')}")
        assert misses.get("distinct_misses") == 0
        assert misses.get("interpreted_insns") == 0

        print("\n[SUCCESS] Modern Gameplay & Controls verification complete!")

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
    main()
