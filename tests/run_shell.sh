#!/bin/sh
# The shell (source/shell.c, menu.c, text.c, loader.c) against its oracle captures in ~/pop2dec/oracle/shell (see
# docs/SHELL.md 9): the game state at every tick start and the screens. usage: tests/run_shell.sh
set -e
here=$(cd "$(dirname "$0")" && pwd); O=$HOME/pop2dec/oracle; C=$O/shell; S=$HOME/pop2dec/sources/prince2
W=${WORK:-$(mktemp -d)}; mkdir -p $W/files
cd "$here/.."
# BUILD=dir (meson test): take the programs built there instead of compiling them here
build() { n=$1; shift; if [ -n "$BUILD" ]; then cp "$BUILD/$n" "$W/$n"; else "$@"; fi; }
build shelltest gcc -O2 -g -Wall -Wno-format-truncation -o $W/shelltest tests/shelltest.c tests/snap.c source/*.c -lm
cmp_shots() {   # cmp_shots CAPTURE_DIR PREFIX N...: the oracle's 640x400 shots against ours (every other pixel)
	d=$1; p=$2; shift 2
	for n in "$@"; do python3 - "$d" "$p" "$n" <<'E'
import sys,struct
def tga(p):
    b=open(p,'rb').read(); w,h=struct.unpack_from('<HH',b,12); px=b[18+b[0]:]
    rows=[px[y*w*3:(y+1)*w*3] for y in range(h)]
    return w,h,(rows if b[17]&0x20 else rows[::-1])
d,p,n=sys.argv[1:4]; wo,ho,ro=tga('%s/%s%s.tga'%(d,p,n)); wm,hm,rm=tga('MINE/%s%s.tga'%(p,n))
full=sum(ro[y*2][x*6:x*6+3]!=rm[y][x*3:x*3+3] for y in range(hm) for x in range(wm))
top=sum(ro[y*2][x*6:x*6+3]!=rm[y][x*3:x*3+3] for y in range(192) for x in range(wm))
status=sum(ro[y*2][x*6:x*6+3]!=rm[y][x*3:x*3+3] for y in range(192,200) for x in range(wm))
print('  %s%s: %d pixels differ (rows 0..191: %d, status line: %d)'%(p,n,full,top,status))
E
	done
}
run() {   # run NAME SCRIPT WORDS...: the shots go to $W/MINE
	name=$1; script=$2; shift 2
	mkdir -p $W/MINE; rm -f $W/files/*
	sed "s#$C/[A-Z0-9]*/#$W/MINE/#" $script > $W/$name.script
	(cd $W && SHELL_FILES=$W/files SHELL_SEED=cbe2d $W/shelltest $S $W/$name.script "$@" 2>/dev/null | grep -E '^compared' | sed "s/^/$name: /")
}
# menus, pause, save, restore, hall of fame (tick-synchronised keys)
[ -f $O/MENU1-snap.txt ] && { SHELL_SYNC=1 SHELL_CMP=$O/MENU1-snap.txt run MENU1 $C/MENU1.script yippeeyahoo LEVEL1; (cd $W && cmp_shots $C/MENU1 m 620 700 1200 1600 1900 985 1400); }
# the copy protection (the program starts 100 frames earlier than in DOSBox here)
[ -f $O/CP1-snap.txt ] && { SHELL_OFFSET=100 SHELL_CMP=$O/CP1-snap.txt run CP1 $C/CP1.script yippeeyahoo LEVEL3; (cd $W && cmp_shots $C/CP1 c 280 318 338); }
# save, walk, restore that game
[ -f $C/SAVELOAD1/SAVELOAD1-snap.txt ] && SHELL_SYNC=1 SHELL_CMP=$C/SAVELOAD1/SAVELOAD1-snap.txt run SAVELOAD1 $C/SAVELOAD1.script yippeeyahoo LEVEL3
# a death, "PRESS KEY TO CONTINUE", the countdown
[ -f $O/DEATH1-snap.txt ] && { SHELL_SYNC=1 SHELL_CMP=$O/DEATH1-snap.txt run DEATH1 $C/DEATH1.script yippeeyahoo LEVEL1; (cd $W && cmp_shots $C/DEATH1 d 3700 3800 3900); }
# the attract loop: the three recorded demos (following the platform's timing as e2e does)
if [ -f $C/ATTRACT3/ATTRACT3-snap.txt ]; then printf 'end 62000\n' > $W/attract.script; SHELL_FOLLOW=1 SHELL_CMP=$C/ATTRACT3/ATTRACT3-snap.txt run ATTRACT3 $W/attract.script; fi
# the title credits: five pages, shot at the same distance from the credits' start (0D5E:225C at DOSBox frame 18023)
if [ -d $C/SHOTS1 ]; then
	printf 'end 21000\n' > $W/find.script
	f=$(cd $W && SHELL_TRACE=1 SHELL_FILES=$W/files $W/shelltest $S $W/find.script 2>&1 | grep -m1 'shell title_credits' | sed 's/frame=\([0-9]*\).*/\1/')
	{ for d in 277:1 877:2 1577:3 2277:4 2977:5; do echo "shot $((18023 + ${d%%:*})) $C/SHOTS1/p${d##*:}.tga"; done; echo "end 21100"; } > $W/shots1.script
	SHELL_OFFSET=$((18023 - f)) run SHOTS1 $W/shots1.script; (cd $W && cmp_shots $C/SHOTS1 p 1 2 3 4 5)
fi
# time out (TIMEOUT2: level 4, Alt-N, scene 0x14, the clock run down with the cheat key, scene 0x1C)
[ -f $C/TIMEOUT2/TIMEOUT2-snap.txt ] && SHELL_SYNC=1 SHELL_FOLLOW=1 SHELL_CMP=$C/TIMEOUT2/TIMEOUT2-snap.txt run TIMEOUT2 $C/TIMEOUT2.script yippeeyahoo LEVEL4
# the end: level 14 won (DS:5CEC poked to 15), scene 0xB, a hall-of-fame entry typed ("zy"), PRINCE.HOF, the title
# (the ending scene is 78 frames shorter here than in DOSBox)
[ -d $C/ENDING2 ] && { SHELL_OFFSET=78 run ENDING2 $C/ENDING2.script yippeeyahoo LEVEL14; (cd $W && cmp_shots $C/ENDING2 e 5700 5745 5800; od -An -tx1 $W/files/PRINCE.HOF | head -2); }
# the cheat keys K, g, k, T (level 1) and S (level 10), pressed between two ticks
for c in CK1:LEVEL1 CK10:LEVEL10; do n=${c%%:*}; [ -f $O/$n-snap.txt ] && SHELL_SYNC=1 SHELL_FOLLOW=1 SHELL_CMP=$O/$n-snap.txt run $n $O/$n.script yippeeyahoo ${c##*:}; done
# a plan (probepoke) with the tick-time drawing: level 2's waves, puzzle and raft against the VGA dumps of FRP2_raft
F=$O/frames/FRP2_raft.frames; [ -f $F ] && (cd $W && SHELL_SYNC=1 SHELL_CMP=$F SHELL_VRAM=$F SHELL_FILES=$W/files SHELL_SEED=cbe2d $W/shelltest $S $O/P2_raft.script yippeeyahoo LEVEL2 2>/dev/null | grep -E '^compared|^vram:' | sed 's/^/P2_raft: /')
exit 0
