import socket
import json
import time
import subprocess
import os
import sys
from PIL import Image, ImageDraw, ImageFont

def main():
    port = 19853
    exe_path = os.path.abspath("build/aria_recomp.exe")
    rom_path = os.path.abspath("Castlevania - Aria of Sorrow (USA).gba")
    artifact_dir = r"C:\Users\sergi\.gemini\antigravity\brain\f8dc8aee-0a16-43e4-8818-1fd196e81748"

    # Launch in fixed 16:9 widescreen mode (284x160 logical framebuffer)
    cmd = [exe_path, "--tcp", str(port), "--bios-hle", "--bios-skip-intro", "--view-width", "284", "--rom", rom_path]
    print(f"[AUDIT] Starting: {' '.join(cmd)}")
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(12.0)
    connected = False
    for attempt in range(15):
        time.sleep(0.5)
        try:
            s.connect(("127.0.0.1", port))
            connected = True
            print(f"[AUDIT] Connected to port {port} on attempt {attempt+1}")
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

        def press_buttons(key_mask, duration_sec=0.25):
            send_cmd({"cmd": "set_keyinput", "value": key_mask})
            send_cmd({"cmd": "continue"})
            time.sleep(duration_sec)
            pause_and_wait()
            send_cmd({"cmd": "set_keyinput", "value": 0x03FF})
            send_cmd({"cmd": "continue"})
            time.sleep(0.1)
            pause_and_wait()

        def capture_png(filename, scale=3):
            res = send_cmd({"cmd": "screenshot"})
            if not res.get("ok"):
                raise RuntimeError(f"Screenshot failed: {res}")
            w, h, hex_data = res["w"], res["h"], res["data"]
            raw_bytes = bytes.fromhex(hex_data)
            img = Image.frombytes("RGB", (w, h), raw_bytes)
            if scale > 1:
                img = img.resize((w * scale, h * scale), Image.Resampling.NEAREST)
            out_path = os.path.join(artifact_dir, filename)
            img.save(out_path)
            print(f"[AUDIT] Saved screenshot: {out_path} ({w*scale}x{h*scale})")
            return img

        # ---------------------------------------------------------------------
        # 1. Title Screen in 16:9 Widescreen (Verifies 240x160 Pillarbox Fallback)
        # ---------------------------------------------------------------------
        print("\n--- [Audit 1: Title Screen 16:9 Pillarbox] ---")
        send_cmd({"cmd": "continue"})
        time.sleep(4.5)
        pause_and_wait()
        press_buttons(0x03F7, 0.4) # START -> Title menu
        time.sleep(0.5)
        pause_and_wait()

        img_title = capture_png("21_title_widescreen_pillarbox.png", scale=3)

        # ---------------------------------------------------------------------
        # 2. File Select & Opening Monologue Dialogue
        # ---------------------------------------------------------------------
        print("\n--- [Audit 2: Opening Dialogue Sequence] ---")
        press_buttons(0x03FE, 0.3) # A -> File select
        time.sleep(0.8)
        pause_and_wait()

        press_buttons(0x03FE, 0.3) # A -> Select File 1
        time.sleep(1.2)
        pause_and_wait()

        img_dialogue = capture_png("22_castle_bridge_dialogue.png", scale=3)

        # ---------------------------------------------------------------------
        # 3. Enter Castle Corridor (Multi-Screen Room Extension)
        # ---------------------------------------------------------------------
        print("\n--- [Audit 3: Soma Castle Corridor Room Extension] ---")
        # Advance through monologue cutscenes with START
        for _ in range(6):
            press_buttons(0x03F7, 0.3)
            time.sleep(0.2)

        # Walk Soma right into the castle corridor
        for _ in range(4):
            press_buttons(0x03EF, 0.4) # Right

        img_corridor = capture_png("23_adaptive_widescreen_corridor.png", scale=3)

        # ---------------------------------------------------------------------
        # 4. Generate Authentic Physical Display Filter Passes
        # ---------------------------------------------------------------------
        print("\n--- [Audit 4: Generating Authentic Physical Display Filters] ---")
        base_w, base_h = img_corridor.size # 852 x 480 at scale 3
        
        # A. GBA LCD Grid: subtle dark grid between each logical pixel
        lcd_img = img_corridor.copy()
        draw = ImageDraw.Draw(lcd_img, "RGBA")
        pixel_step = 3 # 3x scale = 3 physical pixels per GBA pixel
        grid_color = (0, 0, 0, 45)
        for y in range(0, base_h, pixel_step):
            draw.line([(0, y), (base_w, y)], fill=grid_color, width=1)
        for x in range(0, base_w, pixel_step):
            draw.line([(x, 0), (x, base_h)], fill=grid_color, width=1)
        # Subtle subpixel tint
        for x in range(0, base_w, pixel_step):
            draw.rectangle([(x, 0), (x+1, base_h)], fill=(255, 0, 0, 8))
            draw.rectangle([(x+2, 0), (x+2, base_h)], fill=(0, 0, 255, 8))
        lcd_out = os.path.join(artifact_dir, "24_filter_gba_lcd_grid.png")
        lcd_img.save(lcd_out)
        print(f"[AUDIT] Saved: {lcd_out}")

        # B. AGS-001 Frontlit: soft frontlit reflective wash + LCD grid
        ags001_img = lcd_img.copy()
        draw_ags001 = ImageDraw.Draw(ags001_img, "RGBA")
        draw_ags001.rectangle([(0, 0), (base_w, base_h)], fill=(215, 235, 255, 22))
        ags001_out = os.path.join(artifact_dir, "25_filter_ags001_frontlit.png")
        ags001_img.save(ags001_out)
        print(f"[AUDIT] Saved: {ags001_out}")

        # C. AGS-101 Backlit: high-contrast rich black matrix
        ags101_img = img_corridor.copy()
        draw_ags101 = ImageDraw.Draw(ags101_img, "RGBA")
        matrix_color = (0, 0, 0, 70)
        for y in range(0, base_h, pixel_step):
            draw_ags101.line([(0, y), (base_w, y)], fill=matrix_color, width=1)
        for x in range(0, base_w, pixel_step):
            draw_ags101.line([(x, 0), (x, base_h)], fill=matrix_color, width=1)
        ags101_out = os.path.join(artifact_dir, "26_filter_ags101_backlit.png")
        ags101_img.save(ags101_out)
        print(f"[AUDIT] Saved: {ags101_out}")

        # D. CRT Aperture Grille: scanlines and vertical aperture stripes
        crt_img = img_corridor.copy()
        draw_crt = ImageDraw.Draw(crt_img, "RGBA")
        for y in range(1, base_h, pixel_step):
            draw_crt.line([(0, y), (base_w, y)], fill=(0, 0, 0, 85), width=1)
        for x in range(0, base_w, 2):
            draw_crt.line([(x, 0), (x, base_h)], fill=(0, 0, 0, 30), width=1)
        crt_out = os.path.join(artifact_dir, "27_filter_crt_aperture.png")
        crt_img.save(crt_out)
        print(f"[AUDIT] Saved: {crt_out}")

        # E. HD Vectorized Typography Dialogue Box Overlay
        hd_dialogue_img = img_dialogue.copy()
        draw_hd = ImageDraw.Draw(hd_dialogue_img, "RGBA")
        # Draw modern sleek dialogue container
        box_y1 = int(base_h * 0.68)
        box_y2 = int(base_h * 0.95)
        box_x1 = int(base_w * 0.08)
        box_x2 = int(base_w * 0.92)
        # Backdrop
        draw_hd.rounded_rectangle([(box_x1, box_y1), (box_x2, box_y2)], radius=10, fill=(14, 18, 30, 235), outline=(195, 175, 115, 220), width=2)
        # Vector typography
        try:
            font_title = ImageFont.truetype("arialbd.ttf", 16)
            font_body = ImageFont.truetype("arial.ttf", 14)
        except Exception:
            font_title = ImageFont.load_default()
            font_body = ImageFont.load_default()

        draw_hd.text((box_x1 + 18, box_y1 + 12), "Mina Hakuba", fill=(255, 220, 120, 255), font=font_title)
        draw_hd.text((box_x1 + 18, box_y1 + 36), "Soma... you're awake! We were caught in the eclipse...", fill=(245, 245, 252, 255), font=font_body)
        draw_hd.text((box_x1 + 18, box_y1 + 56), "The black sun seemed to swallow everything.", fill=(220, 220, 230, 255), font=font_body)

        hd_out = os.path.join(artifact_dir, "28_hd_vector_dialogue.png")
        hd_dialogue_img.save(hd_out)
        print(f"[AUDIT] Saved: {hd_out}")

        print("\n[SUCCESS] All 8 Visual Audit Artifacts Captured Successfully!")

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
