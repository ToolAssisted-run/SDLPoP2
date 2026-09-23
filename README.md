# SDLPoP2 (work in progress)

A source reconstruction of Prince of Persia 2: The Shadow and the Flame (DOS, 1.0, from the
"Prince of Persia Collection Limited Edition" CD), in the spirit of SDLPoP for the first game,
aimed first at a headless, savestate-able game-logic core for JaffarPlus / Chimera.

Method: Ghidra decompilation of the executable with every RTLink overlay captured at its runtime
address (the game's own loader decompressed them inside a headless DOSBox-X oracle), validated
function by function against that oracle (per-frame RAM diffs, instruction traces, call injection).
Analysis workspace: ~/pop2dec (not part of this repo). Game data files are never committed.

## Status (2026-09-23)
The whole game logic of the 14 levels runs in C: the prince, guards and the other characters of every level kind,
tiles and traps, rooms, fights, the level loop, checkpoints, the clock and the input. Free-running end-to-end tests
reproduce captured DOS runs of every level tick for tick, both from the captured post-load state and from a cold
start (zeroed memory + PRINCE.EXE's data segment). Not reconstructed: drawing and sound (only the state they touch),
the story scenes (NIS; their state effects are applied), the hotkeys besides restart, and a few rarely reached
routines, which the core logs (`pop2_missing()`) when a tick reaches them.

## Core API (`src/core.h`)
    pop2_init("path/to/prince2");            // PRINCE.EXE, SEQUENCE.DAT, PRINCE.DAT, KID.DAT, guard and scenery DATs
    pop2_new_game(level, seed);              // seed: the DOS game seeds its RNG from the clock
    pop2_input in = { .x = 1 };              // x/y -1..1, shift, keystroke
    int r = pop2_frame(&in);                 // one game tick: POP2_PLAYING, POP2_QUIT, or the level just entered
    pop2_save(buf); pop2_load(buf); pop2_hash();   // pop2_state_size() bytes
Build: `cc -O2 -o prog prog.c src/*.c`. `tests/coretest.c` is a small example (random play + savestate round trips).

## Layout
`src/*.c` by subsystem (`docs/ENGINE.md` maps them to the original segments and overlays), `src/glue.c` for the game
files and the overlay entry points, `src/state.c` for the state table, `docs/` for format notes (DAT resources,
SEQUENCE.DAT, levels), `tests/` for the oracle comparisons (`tests/run_all.sh`).
