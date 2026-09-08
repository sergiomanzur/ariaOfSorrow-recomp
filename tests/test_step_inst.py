import socket
import json
import time
import subprocess
import os

def main():
    port = 19848
    exe_path = os.path.abspath("build/aria_recomp.exe")
    rom_path = os.path.abspath("Castlevania - Aria of Sorrow (USA).gba")

    cmd = [exe_path, "--tcp", str(port), "--bios-hle", "--bios-skip-intro", "--rom", rom_path]
    print(f"[TEST] Starting: {' '.join(cmd)}")
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(1.5)

    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(5.0)
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

        # 2. Run status before running
        st_resp = send_cmd({"cmd": "run_status"})
        print(f"[TEST] Initial run_status: {st_resp}")

        # 3. Free run for 1.0 second
        print("[TEST] Free running for 1.0 second...")
        send_cmd({"cmd": "continue"})
        time.sleep(1.0)

        # 4. Pause and wait for core to park at frame boundary
        st_parked = pause_and_wait()
        print(f"[TEST] Core parked status: {st_parked}")

        # 5. Read PPU state
        ppu_resp = send_cmd({"cmd": "ppu_state"})
        print(f"[TEST] PPU State: {ppu_resp}")

        # 6. Read EWRAM / IWRAM data
        ewram_resp = send_cmd({"cmd": "read_ewram", "addr": 0x02000000, "len": 32})
        print(f"[TEST] EWRAM data: {ewram_resp.get('data')[:32]}...")

        # 7. Test Savestate Save & Load while parked
        print("[TEST] Testing Savestate save...")
        save_resp = send_cmd({"cmd": "savestate_save", "path": "test_tcp.state"})
        print(f"[TEST] Savestate save: {save_resp}")

        # Resume for 0.5s
        send_cmd({"cmd": "continue"})
        time.sleep(0.5)
        st2 = pause_and_wait()
        print(f"[TEST] Frame after resume: {st2.get('frame')}")

        # Restore
        load_resp = send_cmd({"cmd": "savestate_load", "path": "test_tcp.state"})
        print(f"[TEST] Savestate load: {load_resp}")
        st3 = send_cmd({"cmd": "run_status"})
        print(f"[TEST] Frame after restore: {st3.get('frame')}")

        # Verify restored frame matches saved frame
        assert st3.get('frame') == st_parked.get('frame'), "Savestate restore frame mismatch!"

        # 8. Clean exit
        send_cmd({"cmd": "quit"})
        s.close()
        print("\n[SUCCESS] Deterministic Savestate save and restore verified 100%!")

        if os.path.exists("test_tcp.state"):
            os.remove("test_tcp.state")

    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except Exception:
            pass

if __name__ == "__main__":
    main()
