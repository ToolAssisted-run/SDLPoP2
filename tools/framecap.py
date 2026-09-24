#!/usr/bin/env python3
"""framecap.py NAME "COMMAND" SCRIPT_WITH_KEYS [END_FRAME] [EXTRA_SCRIPT_LINES_FILE]: capture NAME in the oracle with the
keys of SCRIPT_WITH_KEYS plus probes around the drawing of every frame; writes $POP2_WORKSPACE/oracle/frames/NAME.frames
(records: 8-byte label, u32 emulator frame, u32 length, data) and NAME_ram.bin (a RAM dump at the end).
Probes (see tests/frametest.c):
  169B:0A98 (a normal frame, before 0FB3:12F4): pre_lo (DS:0000..2900), pre_ds (DS:2900..6C00), pre_heap (DS:9800..B800), pre_buf (the
            offscreen buffer, phys 0x4CF22, 192 rows of 320)
  169B:0450 (169B:0430 redraw_all, after the DS:2B92 test): rd_lo, rd_ds, rd_heap, rd_buf
  169B:04A2 (redraw_all, the sprites' pass): rd2_lo, rd2_ds, rd2_heap
  0FB3:13C2 (the tables are about to be drawn): svl (the saved screens' list DS:5FEC..60E0)
  the tick's screen writes: 0FB3:29B8 blk (DS:5B62; 29C7 blkgo: not already blacked out, DS:2450 the current port), 169B:03DE lfr_er, 169B:0409 lfr_hp (DS:5B48), 0FB3:259C hpbars,
  0FB3:204C msg / 20A4 msgkey / 2104 msgerase / 2136 msgclear / 2144 msgclral (its whole-line case) (the status
  line's messages)
  0FB3:1303 (0FB3:12F4 before 1308, the tile redraws): mid_ds, mid_tab, mid_cnt
  0FB3:1436 (0FB3:13C2 done: the tables were drawn): tables (39E0:0000..1450), counts (DS:60F0), dirty (DS:27D8..293C),
            buf (the offscreen buffer)
  169B:0A98 / 169B:0450 port: DS:8000..9000 (the game's offscreen port DS:[5CC2] lies there: +0 its bits, a far pointer)
  BUF2=PHYS (hex) in the environment: the buffer probes are repeated at PHYS (pre_bf2, rd_bf2, bf2) and a record
  'bf2addr' (u32 PHYS) is written: a capture across a story scene that moves the offscreen buffer (level 8's scene 6:
  0x514B2) - tests/frametest.c takes the one the port names
  VRAM (cannot be probed): with VRAM_STEP=n in the environment, the VGA memory every n emulator frames from the first
  key frame (records 'vram', 64000 bytes, linear); with SHOT_STEP=n, the screen's RGB every n frames (records 'shot',
  320 x 200 x 3)"""
import sys, os, re, subprocess, struct
name, cmd, src = sys.argv[1], sys.argv[2], sys.argv[3]
end = int(sys.argv[4]) if len(sys.argv) > 4 else None
extra = open(sys.argv[5]).read() if len(sys.argv) > 5 else ''
WS = os.environ.get('POP2_WORKSPACE', os.path.expanduser('~/pop2dec'))   # the analysis workspace (README)
O = WS + '/oracle'; F = O + '/frames'; os.makedirs(F, exist_ok=True)
lines = [l for l in open(src) if l.startswith(('key ', 'probepoke ', 'poke '))]
last = end or max([int(l.split()[1]) for l in lines if l.startswith('key ')] + [2000])
lines = [l for l in lines if not l.startswith(('key ', 'poke ')) or int(l.split()[1]) <= last] if end else lines   # (probepoke lines are by tick)
with open(f'{O}/{name}.script', 'w') as f:
    f.writelines(lines)
    f.write('probe 169B 0A98 pre_lo DS0000 2900\nprobe 169B 0A98 pre_ds DS2900 4300\nprobe 169B 0A98 pre_heap DS9800 2000\nprobe 169B 0A98 pre_buf 4CF22 F000\n')
    f.write('probe 169B 0450 rd_lo DS0000 2900\nprobe 169B 0450 rd_ds DS2900 4300\nprobe 169B 0450 rd_heap DS9800 2000\nprobe 169B 0450 rd_buf 4CF22 F000\n')
    f.write('probe 169B 04A2 rd2_lo DS0000 2900\nprobe 169B 04A2 rd2_ds DS2900 4300\nprobe 169B 04A2 rd2_heap DS9800 2000\n')
    f.write('probe 169B 0A98 port DS8000 1000\nprobe 169B 0450 port DS8000 1000\n')
    buf2 = int(os.environ['BUF2'], 16) if os.environ.get('BUF2') else 0
    if buf2: f.write(f'probe 169B 0A98 pre_bf2 {buf2:X} F000\nprobe 169B 0450 rd_bf2 {buf2:X} F000\nprobe 0FB3 1436 bf2 {buf2:X} F000\n')
    f.write('probe 0FB3 13C2 svl DS5FEC F4\n')
    # the tick's own screen writes: the room switch's blackout (and the prince's old box erased), the level's first
    # room (the screen erased, the hit points), both hit point bars, the status line's messages
    f.write('probe 0FB3 29B8 blk DS5B62 8\nprobe 0FB3 29C7 blkgo DS2450 2\nprobe 169B 03DE lfr_er DS5B48 2\nprobe 169B 0409 lfr_hp DS5B48 2\nprobe 0FB3 259C hpbars DS5B36 40\n')
    f.write('probe 0FB3 204C msg SS0000 20\nprobe 0FB3 20A4 msgkey DS5CD8 2\nprobe 0FB3 2104 msgerase DS5CDC 2\nprobe 0FB3 2136 msgclear SS0000 8\nprobe 0FB3 2144 msgclral DS5CDC 2\n')
    f.write('probe 0FB3 1303 mid_ds DS2900 4300\nprobe 0FB3 1303 mid_tab 39E00 1450\nprobe 0FB3 1303 mid_cnt DS60F0 10\n')
    f.write('probe 0FB3 1436 tables 39E00 1450\nprobe 0FB3 1436 counts DS60F0 10\nprobe 0FB3 1436 dirty DS27D8 168\nprobe 0FB3 1436 buf 4CF22 F000\n')
    f.write(extra)
    vstep = int(os.environ.get('VRAM_STEP', '0')); sstep = int(os.environ.get('SHOT_STEP', '0'))
    TMP = f'{O}/w_{name}_mem'; os.makedirs(TMP, exist_ok=True)
    if vstep:
        for fr in range(200, last, vstep): f.write(f'mem {fr} 4 {TMP}/v{fr}.bin\n')
    if sstep:
        for fr in range(200, last, sstep): f.write(f'shot {fr} {TMP}/s{fr}.tga\n')
    f.write(f'ram {last} {F}/{name}_ram.bin\nend {last + 1}\n')
subprocess.run([O + '/cap.sh', name, cmd], check=True)
n = 0
with open(f'{F}/{name}.frames', 'wb') as out:
    if buf2: out.write(b'bf2addr'.ljust(8, b'\0') + struct.pack('<III', 0, 4, buf2)); n += 1
    for l in open(f'{O}/{name}-snap.txt'):
        m = re.match(r'frame=(\d+) .* probe=\d+ (\w+) at .*mem=(\w+)', l)
        if not m: continue
        data = bytes.fromhex(m.group(3))
        out.write(m.group(2).encode().ljust(8, b'\0')[:8] + struct.pack('<II', int(m.group(1)), len(data)) + data); n += 1
    import glob
    for p in sorted(glob.glob(f'{TMP}/*')):
        b = os.path.basename(p); fr = int(b[1:].split('.')[0]); d = open(p, 'rb').read()
        if b[0] == 'v': data = bytes(d[(a & ~3) * 4 + (a & 3)] for a in range(64000)); lab = b'vram'
        else:
            idl = d[0]; w = d[12] | d[13] << 8; h = d[14] | d[15] << 8; px = d[18 + idl:]
            data = bytearray()
            for y in range(200):
                for x in range(320):
                    o = ((y * h // 200) * w + x * w // 320) * 3; data += bytes((px[o + 2], px[o + 1], px[o]))
            data = bytes(data); lab = b'shot'
        out.write(lab.ljust(8, b'\0') + struct.pack('<II', fr, len(data)) + data); n += 1
        os.remove(p)
os.remove(f'{O}/{name}-snap.txt')
print(n, 'records in', f'{F}/{name}.frames')
