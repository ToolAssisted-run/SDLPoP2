#!/bin/sh
# capone.sh L K: capture fleet plan out/L<L>_<K>.plan in the oracle and compare it strictly with the core
cd "$(dirname "$0")"; l=$1; k=$2; OUT=${OUT:-out}; n=${NAME:-F}${l}_$k; O=$PWD/oracle; S=$HOME/pop2dec/sources/prince2
[ -s $OUT/L${l}_$k.plan ] || { echo "$n: no plan"; exit 0; }
{ cat $OUT/L${l}_$k.plan; i=0; while [ $i -lt 100 ]; do echo "0 0 0"; i=$((i+1)); done; } > $O/$n.plan
POKE_HP=${HP-12} python3 tools/plan2script.py $l $O/$n.plan $n $O > /dev/null
mkdir -p $O/w_$n
timeout 3000 $HOME/chimera-oracle/chimera-core-dosbox-x/build/meson-native/oracle-run --workdir $O/w_$n --rom $HOME/pop2dec/oracle/pop.hdd --autoexec 'c:' --autoexec 'cd prince2' --autoexec "prince yippeeyahoo LEVEL$l" --script $O/$n.script --events $O/$n-snap.txt > $O/w_$n/log.txt 2>&1
export KID_DAT=$S/KID.DAT PRINCE_DAT=$S/PRINCE.DAT PRINCE2_DIR=$S
r=$(E2E_STRICT=1 E2E_MISSING=1 timeout 3000 ./e2e $S/SEQUENCE.DAT $O/w/ram$n.bin $S/PRINCE.EXE $O/$n-snap.txt $O/$n.script 2>/dev/null)
echo "$n: $(echo "$r" | grep '^e2e' | cut -d';' -f1) $(echo "$r" | grep '^strict: .' | cut -c1-200) $(echo "$r" | grep '^missing' | cut -d: -f2 | tr ' ' '\n' | grep . | sort -u | tr '\n' ' ')"
rm -rf $O/w_$n
