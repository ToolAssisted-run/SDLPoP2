#!/bin/sh
# round.sh R: one full fleet round: 252 explorations (explorer seeds shifted by R; odd k at the normal hp, even k at
# hp 12), then every plan captured in the oracle and compared (capone.sh). Results: out_R/, capall_R.out
cd "$(dirname "$0")"; R=$1; export GAME=$HOME/pop2dec/sources/prince2 R
mkdir -p out_$R
for l in $(seq 1 14); do for k in $(seq 1 18); do echo "$l $k"; done; done | xargs -P 256 -n 2 sh -c 'l=$0; k=$1; pre=; hp=; if [ $((k % 2)) = 0 ]; then hp=12; fi; if [ $l = 14 ] && [ $((k % 4)) = 0 ]; then pre=spirit_pre.plan; fi; EXPLORE_HP=$hp EXPLORE_CTRL=1 EXPLORE_RNG=$((k * 7919 + l + 100003 * R)) EXPLORE_PREFIX=$pre timeout 5400 ./explore "$GAME" $l 0x3528860F ${ITER:-300000} out_$R/L${l}_$k.plan > out_$R/L${l}_$k.log 2>&1'
for l in $(seq 1 14); do for k in $(seq 1 18); do echo "$l $k"; done; done | xargs -P 250 -n 2 sh -c 'OUT=out_$R NAME=R${R}_ HP=$( [ $(($1 % 2)) = 0 ] && echo 12 ) ./capone.sh $0 $1' > capall_$R.out 2>&1
echo ROUNDDONE >> capall_$R.out
