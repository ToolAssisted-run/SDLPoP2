#!/usr/bin/env python3
"""List / extract resources of a PoP2 DAT file (NIS.DAT etc).  nisdat.py FILE [TAG ID OUT]"""
import sys, struct
def load(path):
    d = open(path, 'rb').read()
    to = struct.unpack_from('<I', d, 0)[0]
    nt = struct.unpack_from('<H', d, to)[0]
    res = {}
    for t in range(nt):
        te = to + 2 + t * 6
        tag = d[te:te+4]
        off = to + struct.unpack_from('<H', d, te + 4)[0]
        cnt = struct.unpack_from('<H', d, off)[0]
        for i in range(cnt):
            e = off + 2 + i * 11
            rid, pos, sz = struct.unpack_from('<HIH', d, e)
            flags = d[e+8:e+11]
            res[(tag[::-1].decode("latin1"), rid)] = (d[pos+1:pos+1+sz], flags)   # (after a checksum byte: the body is the whole size)
    return res
if __name__ == '__main__':
    r = load(sys.argv[1])
    if len(sys.argv) > 2:
        open(sys.argv[4], 'wb').write(r[(sys.argv[2], int(sys.argv[3]))][0]); sys.exit()
    for (tag, rid), (b, fl) in sorted(r.items()):
        print(f"{tag!r:8} {rid:5} {len(b):6} {fl.hex()} {b[:16].hex()}")
