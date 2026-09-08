import struct
import hashlib

def main():
    data = open('bios/gba_bios.bin', 'rb').read()
    print("Length:", len(data))
    print("SHA1:  ", hashlib.sha1(data).hexdigest())
    print("MD5:   ", hashlib.md5(data).hexdigest())
    print("\nVector table:")
    for i in range(16):
        w = struct.unpack('<I', data[i*4:i*4+4])[0]
        print(f"0x{i*4:04X}: 0x{w:08X}")

if __name__ == '__main__':
    main()
