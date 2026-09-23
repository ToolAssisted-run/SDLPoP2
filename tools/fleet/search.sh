#!/bin/sh
# search.sh NAME N LEVEL ITER [TARGET]: N explorers in parallel (EXPLORE_* from the environment, EXPLORE_RNG = 1..N),
# out/NAME_k.{plan,log}; stops them all when one logs "won" (the level left through its goal); SEARCHDONE at the end
cd "$(dirname "$0")"; mkdir -p out
name=$1; n=$2; l=$3; it=$4; tgt=$5
for k in $(seq 1 $n); do
	(EXPLORE_RNG=$k timeout 20000 ./explore "$GAME" $l 0x3528860F $it out/${name}_$k.plan $tgt > out/${name}_$k.out 2> out/${name}_$k.log &)
done
while pgrep -x explore > /dev/null; do
	if grep -l "won" out/${name}_*.log 2>/dev/null | head -1 | grep -q .; then sleep 5; pkill -x explore; fi
	sleep 10
done
grep -l "won" out/${name}_*.log; echo SEARCHDONE
