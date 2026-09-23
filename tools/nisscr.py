#!/usr/bin/env python3
"""Disassemble a PoP2 animation script (_SCR resource, played by OVL00 32D4:00B8).  nisscr.py FILE.DAT ID"""
import sys, struct
sys.path.insert(0, __import__('os').path.dirname(__file__))
import nisdat
NAMES = {0: 'end', 1: 'frames', 2: 'delay', 3: 'music', 4: 'stopmusic', 5: 'waitmusic', 6: 'cue', 7: 'draw'}
def dis(b):
    t, l, bo, r = struct.unpack_from('<4h', b, 0)
    out = ['rect top=%d left=%d bottom=%d right=%d' % (t, l, bo, r)]
    p = 8
    while p < len(b):
        op = b[p]; a = p
        w = lambda k: struct.unpack_from('<h', b, p + k)[0]
        if op == 0: out.append('%04x end' % a); p += 1; break
        elif op == 1: s = 'frames n=%d delay=%d' % (w(1), w(3)); p += 5
        elif op in (2, 3, 4, 5): s = '%s %d' % (NAMES[op], w(1)); p += 3
        elif op == 6: s = 'cue %d' % b[p + 1]; p += 2
        elif op == 7: s = 'draw'; p += 1
        elif op & 0xFC == 0x10: s = 'callback%d %d' % (op & 3, w(1)); p += 3
        elif op & 0xFC == 0x14: s = 'loop%d n=%d -> %04x' % (op & 3, w(1), a + w(3) + 3); p += 5
        elif op >= 0x20:
            m = op & 0x1F; k = op & 0xE0
            if k == 0x20: s = 'm%d flags &=%04x |=%04x' % (m, w(1) & 0xFFFF, w(3) & 0xFFFF); p += 5
            elif k == 0x40: s = 'm%d move %d,%d' % (m, w(1), w(3)); p += 5
            elif k == 0x60: s = 'm%d pos x=%d y=%d' % (m, w(1), w(3)); p += 5
            elif k == 0x80: s = 'm%d shapes %d..%d' % (m, w(1), w(3)); p += 5
            elif k == 0xA0: s = 'm%d vel %.4f,%.4f' % (m, struct.unpack_from('<i', b, p+1)[0] / 65536, struct.unpack_from('<i', b, p+5)[0] / 65536); p += 9
            elif k == 0xC0: s = 'm%d acc %.4f,%.4f' % (m, struct.unpack_from('<i', b, p+1)[0] / 65536, struct.unpack_from('<i', b, p+5)[0] / 65536); p += 9
            else: s = 'm%d stamp %d' % (m, b[p + 1]); p += 2
        else: s = 'UNKNOWN %02x' % op; p += 1
        out.append('%04x %s' % (a, s))
    return out
if __name__ == '__main__':
    r = nisdat.load(sys.argv[1])
    b = r[('\x00SCR', int(sys.argv[2]))][0]
    print('\n'.join(dis(b)))
    if ('\x00PSL', int(sys.argv[2])) in r: print('PSL', r[('\x00PSL', int(sys.argv[2]))][0].hex())
