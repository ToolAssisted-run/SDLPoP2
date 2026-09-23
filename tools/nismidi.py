#!/usr/bin/env python3
"""MIDI sound resources (SND type 2) of a DAT: tracks, tempo, cue points (meta 7) and end times, in seconds and in the
game's 60 Hz ticks as the sequencer (194C:2F10, 240 Hz, 16.16 increment) plays them.  nismidi.py FILE.DAT ID"""
import sys, os, struct
sys.path.insert(0, os.path.dirname(__file__)); import nisdat
def vl(b, i):
    v = 0
    while True:
        c = b[i]; i += 1; v = v << 7 | (c & 0x7F)
        if not c & 0x80: return v, i
def fdiv(a, b): return (a << 16) // b
def main():
    r = nisdat.load(sys.argv[1]); b = r[('\x00SND', int(sys.argv[2]))][0]
    print('type byte', hex(b[0]), 'size', len(b))
    m = b[1:]; assert m[:4] == b'MThd'
    fmt, ntr, div = struct.unpack_from('>HHH', m, 8); p = 8 + struct.unpack_from('>I', m, 4)[0]
    print('format', fmt, 'tracks', ntr, 'division', div)
    events = []
    for t in range(ntr):
        ln = struct.unpack_from('>I', m, p + 4)[0]; q = p + 8; end = q + ln; tick = 0; run = 0
        while q < end:
            d, q = vl(m, q); tick += d; c = m[q]; q += 1
            if c == 0xFF:
                ty = m[q]; q += 1; L, q = vl(m, q); data = m[q:q + L]; q += L
                if ty == 0x51: events.append((tick, t, 'tempo', data[0] << 16 | data[2] << 8 | data[1]))   # (the game swaps the low bytes: 194C:316B)
                elif ty == 0x07: events.append((tick, t, 'cue', data[0], data))
                elif ty == 0x2F: events.append((tick, t, 'eot')); break
                else: events.append((tick, t, 'meta%02x' % ty, data[:8]))
            elif c in (0xF0, 0xF7): L, q = vl(m, q); q += L
            else:
                if c < 0x80: q -= 1; c = run
                run = c; q += 1 if (c & 0xF0) in (0xC0, 0xD0) else 2
        p = end
    events.sort(key=lambda e: (e[0], e[1]))
    tempo = 500000; inc = fdiv(fdiv(0x0F424000, 240 << 16), fdiv(tempo << 8, div << 16))
    # simulate the 240 Hz steps
    acc = 0; pos = 0; step = 0; k = 0; secs = 0.0; last = 0
    for e in events:
        while pos < e[0]:
            a = (acc & 0xFFFF) + (inc & 0xFFFF); n = (inc >> 16) + (a >> 16); acc = a & 0xFFFF; pos += n; step += 1
        secs += (e[0] - last) * tempo / div / 1e6; last = e[0]
        if e[2] == 'tempo' and e[1] == 0: tempo = e[3]; inc = fdiv(fdiv(0x0F424000, 240 << 16), fdiv(tempo << 8, div << 16))
        if e[2] in ('cue', 'eot', 'tempo'): print('%8d midi ticks  %8.3f s  step %6d  game tick %6.1f  track %d  %s' % (e[0], secs, step, step / 4, e[1], e[2:]))
main()
