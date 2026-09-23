#!/usr/bin/env python3
"""List the sound resources (tag "DNS", id 10000+n) of PoP2 sound DATs: audiodat.py FILE.DAT [...]
kinds by byte 0 & 0x7F: 0 PC speaker, 1 digital (8-bit PCM), 2 MIDI (bit 7: loop); see docs/AUDIO.md."""
import struct, sys

def resources(path):
    d = open(path, 'rb').read()
    to = struct.unpack_from('<I', d, 0)[0]
    nt = struct.unpack_from('<H', d, to)[0]
    for t in range(nt):
        te = d[to + 2 + 6 * t: to + 8 + 6 * t]
        off = to + struct.unpack_from('<H', te, 4)[0]
        cnt = struct.unpack_from('<H', d, off)[0]
        for i in range(cnt):
            e = d[off + 2 + 11 * i: off + 13 + 11 * i]
            rid, o, sz = struct.unpack_from('<HIH', e)
            yield te[:4], rid, d[o + 1: o + sz]

def describe(r):
    k = r[0] & 0x7F; loop = 'L' if r[0] & 0x80 else ' '
    if k == 1:
        rate, b3, ln, ls, le = struct.unpack_from('<HBHHH', r, 1)
        packed = b3 == 0xFF
        return 'digi%s rate %5d b3 %02X len %5d loop %5d..%5d%s' % (loop, rate, b3, struct.unpack_from('<H', r, 10)[0] if packed else ln, ls, le, ' packed' if packed else '')
    if k == 2:
        if r[1:5] == b'MSeq': return 'midi%s MSeq %d bytes' % (loop, len(r))
        fmt, ntr, div = struct.unpack_from('>HHH', r, 9)
        return 'midi%s fmt %d tracks %d div %d, %d bytes' % (loop, fmt, ntr, div, len(r))
    if k == 0:
        return 'spkr%s timer %d Hz, %d notes' % (loop, struct.unpack_from('<H', r, 1)[0], (len(r) - 3) // 3)
    return 'kind %d' % k

if __name__ == '__main__':
    for p in sys.argv[1:]:
        for tag, rid, r in resources(p):
            if tag == b'DNS\0': print('%s %5d (0x%03X) %s' % (p.split('/')[-1], rid, rid - 10000, describe(r)))
