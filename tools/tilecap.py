#!/usr/bin/env python3
"""tilecap.py NAME "COMMAND" SCRIPT_WITH_KEYS [END_FRAME]: capture NAME in the oracle with the keys of SCRIPT_WITH_KEYS
plus probes at 0FB3:0122 (a whole-room build starts: the DS state 0x2900..0x6C00) and 0FB3:0B78 (tables drawn: the
back/fore tables and the counts) and a RAM dump at the end; writes cases NAME_<n>.case (one per room build: the state
hex, then the tables hex, one per line) into ~/pop2dec/oracle/tiles/, and NAME_ram.bin"""
import sys, os, re, subprocess
name, cmd, src = sys.argv[1], sys.argv[2], sys.argv[3]
end = int(sys.argv[4]) if len(sys.argv) > 4 else None
O = os.path.expanduser('~/pop2dec/oracle'); T = O + '/tiles'; os.makedirs(T, exist_ok=True)
lines = [l for l in open(src) if l.startswith(('key ', 'probepoke ', 'poke '))]
last = end or max([int(l.split()[1]) for l in lines if l.startswith('key ')] + [2000])
with open(f'{O}/{name}.script', 'w') as f:
    f.writelines(lines)
    f.write('probe 0FB3 0122 build DS2900 4300\nprobe 0FB3 0B78 tables 39E00 1450\nprobe 0FB3 0B7F counts DS60F0 10\n')
    f.write(f'ram {last} {T}/{name}_ram.bin\nend {last + 1}\n')
subprocess.run([O + '/cap.sh', name, cmd], check=True)
state = None; n = 0; want_tables = False; tab = None
for l in open(f'{O}/{name}-snap.txt'):
    m = re.search(r' probe=\d+ (build|tables|counts) .*mem=(\w+)', l)
    if not m: continue
    k, mem = m.group(1), m.group(2)
    if k == 'build': state = mem; want_tables = True
    elif k == 'tables' and want_tables: tab = mem
    elif k == 'counts' and want_tables and tab:
        n += 1
        open(f'{T}/{name}_{n}.case', 'w').write(state + '\n' + tab + mem[:20] + '\n')
        want_tables = False; tab = None
print(n, 'cases in', T)
