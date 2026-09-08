with open('third_party/cvaos/asm/m4a0.s', 'r') as f:
    lines = f.readlines()

current_mode = 'thumb'
m4a_funcs = []
for line in lines:
    line_clean = line.strip()
    if 'arm_func_start' in line_clean:
        current_mode = 'arm'
    elif 'thumb_func_start' in line_clean or 'non_word_aligned_thumb_func_start' in line_clean:
        current_mode = 'thumb'
    
    if '@ 0x08' in line_clean:
        parts = line_clean.split('@')
        name = parts[0].strip().rstrip(':')
        addr_str = parts[1].strip()
        try:
            addr = int(addr_str, 16)
            m4a_funcs.append((addr, current_mode, name))
        except ValueError:
            pass

for addr, mode, name in sorted(m4a_funcs):
    print(f'addr = 0x{addr:08X}, mode = "{mode}", name = "{name}"')
