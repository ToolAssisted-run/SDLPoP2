#!/bin/sh
# fleet.sh GAME_DIR: 252 explorations (levels 1..14 x explorer seeds 1..18) in parallel; results in ./out/
cd "$(dirname "$0")"; mkdir -p out
export GAME=$1
for l in $(seq 1 14); do for k in $(seq 1 18); do echo "$l $k"; done; done | xargs -P 256 -n 2 sh -c 'l=$0; k=$1; pre=; if [ $l = 14 ] && [ $((k % 2)) = 0 ]; then pre=spirit_pre.plan; fi; EXPLORE_HP=12 EXPLORE_CTRL=1 EXPLORE_RNG=$((k * 7919 + l)) EXPLORE_PREFIX=$pre timeout 5400 ./explore "$GAME" $l 0x3528860F 300000 out/L${l}_$k.plan > out/L${l}_$k.log 2>&1'
echo FLEETDONE
