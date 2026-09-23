#!/bin/sh
cd "$(dirname "$0")"
for l in $(seq 1 14); do for k in $(seq 1 18); do echo "$l $k"; done; done | xargs -P 250 -n 2 ./capone.sh > capall.out 2>&1
echo CAPDONE >> capall.out
