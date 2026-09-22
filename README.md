# SDLPoP2 (work in progress)

A source reconstruction of Prince of Persia 2: The Shadow and the Flame (DOS, 1.0, from the
"Prince of Persia Collection Limited Edition" CD), in the spirit of SDLPoP for the first game,
aimed first at a headless, savestate-able game-logic core for JaffarPlus / Chimera.

Method: Ghidra decompilation of the executable with every RTLink overlay captured at its runtime
address (the game's own loader decompressed them inside a headless DOSBox-X oracle), validated
function by function against that oracle (per-frame RAM diffs, instruction traces, call injection).
Analysis workspace: ~/pop2dec (not part of this repo). Game data files are never committed.

Layout (planned, mirroring SDLPoP): `src/segXXXX.c` per original code segment, `src/data.h` for
the data segment, `src/rtlink/` for the overlay/runtime glue that the reconstruction replaces,
`docs/` for format notes (DAT resources, SEQUENCE.DAT, levels), `tests/` for oracle comparisons.
