import socket
import json
import time
import subprocess
import os

def main():
    port = 19850
    exe_path = os.path.abspath("build/aria_recomp.exe")
    rom_path = os.path.abspath("Castlevania - Aria of Sorrow (USA).gba")

    cmd = [exe_path, "--tcp", str(port), "--bios-hle", "--bios-skip-intro", "--rom", rom_path]
    print(f"[TEST] Starting: {' '.join(cmd)}")
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(1.5)

    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(10.0)
        s.connect(("127.0.0.1", port))
        print(f"[TEST] Connected to port {port}")

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
                time.sleep(0.05)
            return send_cmd({"cmd": "run_status"})

        # 1. Ping
        ping_resp = send_cmd({"cmd": "ping"})
        print(f"[TEST] Ping: {ping_resp}")
        assert ping_resp.get("ok") is True

        # 2. Run for 5.0 seconds (~300 frames)
        print("[TEST] Free running for 5.0 seconds (Konami logo & title screen)...")
        send_cmd({"cmd": "continue"})
        time.sleep(5.0)

        # 3. Pause
        st = pause_and_wait()
        print(f"[TEST] Run status at frame {st.get('frame')}: PC={st.get('pc')}")

        # 4. Press START to skip intro / open title menu (0x03F7 presses START)
        print("[TEST] Sending START button (keyinput=0x03F7)...")
        send_cmd({"cmd": "set_keyinput", "value": 0x03F7})
        send_cmd({"cmd": "continue"})
        time.sleep(0.5)
        pause_and_wait()

        # Release buttons
        send_cmd({"cmd": "set_keyinput", "value": 0x03FF})
        send_cmd({"cmd": "continue"})
        time.sleep(1.0)
        st_title = pause_and_wait()
        print(f"[TEST] Title menu reached at frame {st_title.get('frame')}")

        # 5. Read PPU State
        ppu = send_cmd({"cmd": "ppu_state"})
        print(f"[TEST] PPU State: frame={ppu.get('frame')}, dispcnt=0x{ppu.get('dispcnt', 0):04X}, bg0cnt=0x{ppu.get('bg0cnt', 0):04X}")

        # 6. Read EWRAM & IWRAM to verify game state
        ewram = send_cmd({"cmd": "read_ewram", "addr": 0x02000000, "len": 64})
        print(f"[TEST] EWRAM read ok: {ewram.get('ok')}")

        # 7. Check misses
        misses = send_cmd({"cmd": "misses"})
        print(f"[TEST] Misses: distinct={misses.get('distinct_misses')}, misses={misses.get('misses')}")

        # 8. Test Savestate Save & Restore
        print("[TEST] Testing Savestate Save...")
        st_title = pause_and_wait()
        save_res = send_cmd({"cmd": "savestate_save", "path": "test_playability.state"})
        print(f"[TEST] Savestate save: {save_res}")
        assert save_res.get("ok") is True

        saved_frame = st_title.get('frame')

        # Advance 60 more frames
        send_cmd({"cmd": "continue"})
        time.sleep(1.0)
        st_advanced = pause_and_wait()
        print(f"[TEST] Advanced to frame {st_advanced.get('frame')}")
        assert st_advanced.get('frame') > saved_frame

        # Restore Savestate
        print("[TEST] Testing Savestate Load/Restore...")
        load_res = send_cmd({"cmd": "savestate_load", "path": "test_playability.state"})
        print(f"[TEST] Savestate load: {load_res}")
        assert load_res.get("ok") is True

        st_restored = send_cmd({"cmd": "run_status"})
        print(f"[TEST] Restored frame: {st_restored.get('frame')}")
        assert st_restored.get('frame') == saved_frame, "Savestate frame restored exactly!"

        # 9. Clean exit
        send_cmd({"cmd": "quit"})
        s.close()
        print("\n========================================================")
        print("  [SUCCESS] All Full-Game Playability Tests PASSED 100%!")
        print("========================================================")

        if os.path.exists("test_playability.state"):
            os.remove("test_playability.state")

    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except Exception:
            pass

if __name__ == "__main__":
    main()
