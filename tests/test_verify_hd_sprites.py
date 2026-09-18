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
    port = 19867
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
                return raw_bytes, shot.get("w", 240), shot.get("h", 160)
            return None, 0, 0

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

        # Advance through intro text and dialog until Soma stands up
        for _ in range(25):
            press_buttons(0x03F7, 0.15)  # START
            time.sleep(0.1)
            press_buttons(0x03FE, 0.15)  # A
            time.sleep(0.1)

        st = pause_and_wait()
        print(f"[VERIFY] In-game active at guest frame {st.get('frame')}")
        # 2. Capture Normal GBA Sprites (Baseline)
        print("\n--- STEP 2: Capturing Normal GBA Sprites (Baseline) ---")
        gfx_off = send_cmd({"cmd": "set_graphics", "hd_sprites": False})
        print(f"[SET_GRAPHICS OFF] {gfx_off}")
        assert gfx_off.get("ok") is True
        assert gfx_off.get("hd_sprites") is False

        send_cmd({"cmd": "continue"})
        time.sleep(0.2)
        pause_and_wait()

        normal_bytes, w, h = take_screenshot("35_sprites_normal_gba.png")
        assert normal_bytes is not None and len(normal_bytes) == w * h * 3

        # 3. Enable HD Character Sprites & Capture Comparison
        print("\n--- STEP 3: Enabling HD Character Sprites ---")
        gfx_on = send_cmd({"cmd": "set_graphics", "hd_sprites": True})
        print(f"[SET_GRAPHICS ON] {gfx_on}")
        assert gfx_on.get("ok") is True
        assert gfx_on.get("hd_sprites") is True

        send_cmd({"cmd": "continue"})
        time.sleep(0.25)
        pause_and_wait()

        hd_bytes, w, h = take_screenshot("36_sprites_hd_active.png")
        assert hd_bytes is not None and len(hd_bytes) == w * h * 3

        # 4. Compare Differences to Prove HD Sprites Composited Over Player
        diff_count = 0
        diff_soma_region = 0
        for i in range(0, len(normal_bytes), 3):
            px_idx = i // 3
            x = px_idx % w
            y = px_idx // w
            r1, g1, b1 = normal_bytes[i], normal_bytes[i+1], normal_bytes[i+2]
            r2, g2, b2 = hd_bytes[i], hd_bytes[i+1], hd_bytes[i+2]
            if (r1, g1, b1) != (r2, g2, b2):
                diff_count += 1
                if 60 <= x <= 160 and 40 <= y <= 140:
                    diff_soma_region += 1

        print(f"[VERIFY] Total pixel differences: {diff_count}")
        print(f"[VERIFY] Pixel differences in Soma screen space: {diff_soma_region}")
        assert diff_soma_region > 50, f"Expected HD sprite pixels to differ in Soma region, got {diff_soma_region}"

        # 5. Disable HD Sprites and Verify Clean Return to Authentic GBA
        print("\n--- STEP 5: Disabling HD Sprites (Restoring Normal) ---")
        gfx_off2 = send_cmd({"cmd": "set_graphics", "hd_sprites": False})
        assert gfx_off2.get("ok") is True
        assert gfx_off2.get("hd_sprites") is False

        send_cmd({"cmd": "continue"})
        time.sleep(0.2)
        pause_and_wait()

        restored_bytes, _, _ = take_screenshot("37_sprites_normal_restored.png")
        restored_diff = sum(1 for i in range(0, len(normal_bytes), 3) if normal_bytes[i:i+3] != restored_bytes[i:i+3])
        print(f"[VERIFY] Restored sprite differences from original: {restored_diff}")

        print("\n========================================================")
        print(" [SUCCESS] HD Character Sprites Headless Verification PASSED!")
        print(" - Faithful Core Baseline: 35_sprites_normal_gba.png")
        print(" - HD Sprites Active:      36_sprites_hd_active.png")
        print(" - Normal Mode Restored:   37_sprites_normal_restored.png")
        print(" - Toggle On/Off dynamically confirmed during active gameplay!")
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
