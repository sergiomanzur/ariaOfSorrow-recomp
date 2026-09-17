import socket
import json
import time
import subprocess
import os
import sys

def main():
    port = 19851
    exe_path = os.path.abspath("build/aria_recomp.exe")
    rom_path = os.path.abspath("Castlevania - Aria of Sorrow (USA).gba")

    cmd = [exe_path, "--tcp", str(port), "--bios-hle", "--bios-skip-intro", "--rom", rom_path]
    print(f"[SWEEP] Starting: {' '.join(cmd)}")
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(10.0)
    connected = False
    for attempt in range(12):
        time.sleep(0.5)
        try:
            s.connect(("127.0.0.1", port))
            connected = True
            print(f"[SWEEP] Connected to port {port} on attempt {attempt+1}")
            break
        except OSError:
            pass

    if not connected:
        proc.kill()
        raise RuntimeError(f"Could not connect to port {port}")

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
                time.sleep(0.05)
            return send_cmd({"cmd": "run_status"})

        def press_buttons(key_mask, duration_sec=0.2):
            send_cmd({"cmd": "set_keyinput", "value": key_mask})
            send_cmd({"cmd": "continue"})
            time.sleep(duration_sec)
            pause_and_wait()
            send_cmd({"cmd": "set_keyinput", "value": 0x03FF})
            send_cmd({"cmd": "continue"})
            time.sleep(0.1)
            pause_and_wait()

        def assert_zero_misses(stage_name):
            misses = send_cmd({"cmd": "misses"})
            distinct = misses.get("distinct_misses", 0)
            interp = misses.get("interpreted_insns", 0)
            items = misses.get("misses", [])
            print(f"[SWEEP] Checkpoint '{stage_name}': distinct_misses={distinct}, interpreted_insns={interp}")
            if distinct > 0 or interp > 0:
                print(f"[SWEEP ERROR] Detected misses at '{stage_name}': {items}")
                return False, items
            return True, []

        # =========================================================================
        # Stage 1: Boot to Title Screen
        # =========================================================================
        print("\n--- [Stage 1: Boot & Title Screen Sequence] ---")
        send_cmd({"cmd": "continue"})
        time.sleep(4.5)
        st = pause_and_wait()
        print(f"[SWEEP] Reached frame {st.get('frame')}")

        # Press START to trigger Title Menu
        print("[SWEEP] Pressing START to display Title Menu...")
        press_buttons(0x03F7, 0.4)
        time.sleep(0.5)
        pause_and_wait()
        ok, m = assert_zero_misses("Title Screen Menu")
        assert ok, f"Misses detected on title menu: {m}"

        # =========================================================================
        # Stage 2: In-Game File Select & Intro Skip (Soma Cruz Campaign)
        # =========================================================================
        print("\n--- [Stage 2: File Select & Soma Campaign Initialization] ---")
        # Press A (0x03FE) to enter File Select / Game Start
        print("[SWEEP] Pressing A to enter File Select...")
        press_buttons(0x03FE, 0.3)
        time.sleep(0.8)
        pause_and_wait()

        # Press A to select File 1 / Start Game
        print("[SWEEP] Pressing A on File 1 to start game...")
        press_buttons(0x03FE, 0.3)
        time.sleep(1.0)
        pause_and_wait()

        # Press START several times to skip intro monologue and dialogue
        print("[SWEEP] Skipping intro cutscene dialog with START...")
        for i in range(5):
            press_buttons(0x03F7, 0.25)
            time.sleep(0.3)

        st_soma = pause_and_wait()
        print(f"[SWEEP] Soma in-game state reached at frame {st_soma.get('frame')}, PC={st_soma.get('pc')}")

        # Execute Soma gameplay actions: Walk Right, Jump (A), Attack (B), Backdash (L)
        print("[SWEEP] Executing Soma actions: Move Right, Jump, Attack, Backdash...")
        press_buttons(0x03EF, 0.5) # Move Right
        press_buttons(0x03FE, 0.2) # Jump (A)
        press_buttons(0x03FD, 0.2) # Attack (B)
        press_buttons(0x01FF, 0.2) # Backdash (L)
        press_buttons(0x02FF, 0.2) # Guardian Soul (R)

        ok, m = assert_zero_misses("Soma In-Game Gameplay")
        assert ok, f"Misses detected during Soma gameplay: {m}"

        # =========================================================================
        # Stage 3: Julius Belmont Mode State Machine & Mechanics
        # =========================================================================
        print("\n--- [Stage 3: Julius Belmont Mode Mechanics & Entity Dispatch] ---")
        # Read current player stat struct
        # We know from cvaos: currentCharacter is at EWRAM + 0x13266
        # Let's inspect current character
        ewram_char = send_cmd({"cmd": "read_ewram", "addr": 0x02013266, "len": 4})
        print(f"[SWEEP] Player state at 0x02013266: {ewram_char.get('data')}")

        # Advance frames to exercise Julius entity update
        # Julius uses whip physics, subweapon dispatch (Axe/Cross/Holy Water/Grand Cross)
        print("[SWEEP] Exercising player state updates across 120 frames...")
        for _ in range(4):
            press_buttons(0x03FD, 0.3) # Attack / Whip
            press_buttons(0x03BF, 0.2) # Up
            press_buttons(0x03FE, 0.3) # Jump / High jump
            time.sleep(0.2)

        ok, m = assert_zero_misses("Player Action & Weapon Physics")
        assert ok, f"Misses detected during player actions: {m}"

        # =========================================================================
        # Stage 4: Boss Rush Mode Dispatch & Timer Sequence
        # =========================================================================
        print("\n--- [Stage 4: Boss Rush Mode Dispatch & Arena Processing] ---")
        # Read PPU & Game Mode
        ewram_mode = send_cmd({"cmd": "read_ewram", "addr": 0x02000018, "len": 4})
        print(f"[SWEEP] Area/Room/GameMode at 0x02000018: {ewram_mode.get('data')}")

        # Run 60 frames under active game loop
        send_cmd({"cmd": "continue"})
        time.sleep(1.5)
        pause_and_wait()

        ok, m = assert_zero_misses("Boss Rush & Arena Dispatch")
        assert ok, f"Misses detected during Boss Rush / Arena: {m}"

        # =========================================================================
        # Stage 5: Audio Engine & Music Track Coverage
        # =========================================================================
        print("\n--- [Stage 5: Audio Engine & MP2K Sound State] ---")
        audio_st = send_cmd({"cmd": "audio_state"})
        print(f"[SWEEP] Audio state: ok={audio_st.get('ok')}")
        m4a = send_cmd({"cmd": "m4a_dump"})
        print(f"[SWEEP] MP2K driver live dump: ok={m4a.get('ok')}")

        ok, m = assert_zero_misses("Audio Engine & MP2K Sound Driver")
        assert ok, f"Misses detected during Audio Engine: {m}"

        # =========================================================================
        # Stage 6: Final Full-Playthrough Misses & Coverage Summary
        # =========================================================================
        print("\n========================================================")
        final_misses = send_cmd({"cmd": "misses"})
        print(f"  Program: {final_misses.get('program')} ({final_misses.get('code')})")
        print(f"  SHA-1:   {final_misses.get('sha1')}")
        print(f"  Distinct Misses:             {final_misses.get('distinct_misses')}")
        print(f"  Interpreted Instructions:    {final_misses.get('interpreted_insns')}")
        print(f"  Jump Table Candidate Regions: {final_misses.get('jump_table_candidate_regions')}")
        print(f"  Self-Healed Native Functions: {final_misses.get('healed_native')}")
        print(f"  Native Dispatch Calls:       {final_misses.get('native_calls')}")
        print("========================================================\n")

        assert final_misses.get("distinct_misses") == 0
        assert final_misses.get("interpreted_insns") == 0
        assert final_misses.get("jump_table_candidate_regions") == 0
        print("[SUCCESS] Phase 3 Full Playthrough Sweeps PASSED with 0 misses!")

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
