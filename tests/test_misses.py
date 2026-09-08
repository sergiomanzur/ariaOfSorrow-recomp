import socket
import json
import time
import subprocess
import os

def main():
    port = 19849
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

        # 1. Ping
        ping_resp = send_cmd({"cmd": "ping"})
        print(f"[TEST] Ping: {ping_resp}")

        # 2. Run for 2.0 seconds
        print("[TEST] Free running for 2.0 seconds...")
        send_cmd({"cmd": "continue"})
        time.sleep(2.0)

        # 3. Query misses / self-heal
        misses_resp = send_cmd({"cmd": "misses"})
        print(f"[TEST] Misses / Self-heal: {misses_resp}")

        # 4. Status
        st = send_cmd({"cmd": "run_status"})
        print(f"[TEST] Run status: {st}")

        send_cmd({"cmd": "quit"})
        s.close()
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except Exception:
            pass

if __name__ == "__main__":
    main()
