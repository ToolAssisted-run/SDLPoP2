#!/bin/bash
# oracle_check.sh MODE SCRIPT: run an oracle suite script (meson test) and judge its output.
#   ticks: every "mismatching" count 0, except the known single-tick harness cases L1 D/E/F (1/1/2, a mid-tick room
#          change the harness does not model); coretest 0 round trips differ
#   tiles: every room build "0 entries differ"
#   info : the script ran (sound model / shell comparisons have documented, accepted differences)
mode=$1; script=$2; shift 2
out=$(bash "$script" "$@" 2>&1); rc=$?
echo "$out"
[ $rc -eq 0 ] || { echo "FAIL: $script exited with $rc"; exit 1; }
case $mode in
ticks)
	bad=$(echo "$out" | grep "mismatching" | grep -v " 0 mismatching" | grep -vE "^L1 D tick: .* 1 mismatching|^L1 E tick: .* 1 mismatching|^L1 F tick: .* 2 mismatching")
	bad="$bad$(echo "$out" | grep "round trips differ" | grep -v "^coretest: 0 of")"
	[ -z "$bad" ] || { echo "FAIL:"; echo "$bad"; exit 1; } ;;
tiles)
	bad=$(echo "$out" | grep -v " 0 entries differ")
	[ -z "$bad" ] || { echo "FAIL:"; echo "$bad"; exit 1; } ;;
esac
exit 0
