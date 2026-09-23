#!/usr/bin/env python3
"""Contact sheet of oracle TGA shots: nissheet.py OUT.png COLS SCALE file1.tga ... (SCALE 2 = 320x200, 4 = 160x100)"""
import struct, zlib, sys
def tga(path):
    b = open(path, 'rb').read(); w, h = struct.unpack_from('<HH', b, 12); n = b[16] // 8; desc = b[17]
    px = b[18 + b[0]:]
    rows = [px[y * w * n:(y + 1) * w * n] for y in range(h)]
    if not desc & 0x20: rows = rows[::-1]
    return w, h, n, rows
def main():
    out, cols, sc = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]); files = sys.argv[4:]
    W, H = 640 // sc, 400 // sc; rowsN = (len(files) + cols - 1) // cols
    canvas = [bytearray(b'\x40' * (W * cols * 3)) for _ in range(H * rowsN)]
    for i, f in enumerate(files):
        w, h, n, rows = tga(f); cx, cy = (i % cols) * W, (i // cols) * H
        for y in range(H):
            r = rows[y * sc]; line = canvas[cy + y]
            for x in range(W - 1):
                p = x * sc * n; q = (cx + x) * 3
                line[q] = r[p + 2]; line[q + 1] = r[p + 1]; line[q + 2] = r[p]
    raw = b''.join(b'\0' + bytes(l) for l in canvas)
    def chunk(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    open(out, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', W * cols, H * rowsN, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))
main()
