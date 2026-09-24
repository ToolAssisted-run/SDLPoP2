# SDLPoP2

An unofficial source reconstruction of Prince of Persia 2: The Shadow and the Flame (DOS, 1.0, from the
"Prince of Persia Collection Limited Edition" CD), in the spirit of [SDLPoP](https://github.com/NagyD/SDLPoP) for the first
game, aimed first at a headless, savestate-able game-logic core for [JaffarPlus](https://github.com/ToolAssisted-run/jaffarPlus) /
[Chimera](https://github.com/ToolAssisted-run/chimera).

Method: [Ghidra](https://github.com/NationalSecurityAgency/ghidra) decompilation of the executable with every RTLink overlay captured at its runtime
address (the game's own loader decompressed them inside a headless [DOSBox-X](https://github.com/joncampbell123/dosbox-x) oracle,
[chimera-core-dosbox-x](https://github.com/ToolAssisted-run/chimera-core-dosbox-x)), validated
function by function against that oracle (per-frame RAM diffs, instruction traces, call injection).
Analysis workspace: outside this repo, written `<workspace>` in the docs (`<workspace>/sources`: the game files,
`<workspace>/oracle`: the oracle runner and its captures, `<workspace>/work` and `<workspace>/image`: disassembly
listings and overlay images). The scripts find it through `POP2_WORKSPACE` (default `~/pop2dec`). Game data files
are never committed.

## Status (2026-09-24)
The whole program runs in C: the game logic of the 14 levels, the renderer, the story scenes (NIS), the sound drivers
(Sound Blaster digital, OPL2 FM music, PC speaker) and the program around them (title and demos, menus, save/restore,
copy protection, hall of fame). Everything is checked against the DOS game running in the headless DOSBox-X oracle:
free-running end-to-end tests reproduce captured runs of every level tick for tick (including each level's
completion), the drawing matches the game's offscreen buffer frame for frame, the sound drivers match its register
writes, and menus and scenes match its screenshots. `docs/FINDINGS.md` has everything found; `docs/AUDIO.md`,
`docs/NIS.md` and `docs/SHELL.md` cover those parts.

## Building ([meson](https://mesonbuild.com))
    meson setup build                          # options: meson_options.txt (buildFrontend, buildTools, buildTests, ...)
    meson compile -C build
    build/sdl/sdlpop2 path/to/prince2          # the game; add DOS command-line words, e.g. `yippeeyahoo LEVEL3`
                                               # (options before the directory: --ini PATH, --record NAME, --replay NAME)
Tests: `meson setup build -DgameDir=path/to/prince2` registers the core and settings suites (`meson test -C build --suite core --suite settings`);
`-DoracleTests=true` adds the oracle comparison suites (`--suite oracle`), which need the captures in `<workspace>`.
The `controller` suite (game controllers, SDL's virtual joystick) runs with or without the game files.
The game data files are not part of this repository.

## Settings (`SDLPoP2.ini`)
The SDL frontend reads `SDLPoP2.ini` (modelled on SDLPoP's `SDLPoP.ini`): `--ini PATH`, else `SDLPoP2.ini` in the
current directory, next to the binary (the build copies it to `build/sdl/`), or the installed `share/sdlpop2/SDLPoP2.ini`.
Every option is documented in the file, `default` is accepted everywhere, unknown options are reported. Every default is
the original game: `[General]` (window, 4:3 aspect, integer scaling, sharp / fuzzy / blurry scaling, music, sounds,
volume, the sound device, the intro, the story scenes, the copy protection, skipping the title, the control keys),
`[AdditionalFeatures]` (F6 / F9 quicksave with SDLPoP's one-minute penalty, replays, the random seed, the F1 key
summary), `[Controller]` (game controllers: on / off, rumble, the stick's dead zone and horizontal-only mode, extra
mappings, the buttons), `[CustomGameplay]` (starting time and hit points, ticks per minute, the hit point cap, Alt+N's minutes, the
first level, the copy protection's level, the tick speeds), `[Level N]` (the prince's sword type) and `[Skill N]`
(the guards' strike / block / advance probabilities and refractory timers, PRINCE.EXE's DS:1BB6 / DS:13D0 tables).

The core and the shell read the gameplay options through `pop2_settings_game` (`source/settings.h`), NULL unless the
frontend installs one: at each original site the code is `GAME_SETTING(field, original)`, so the headless core and the
tests run the verified code unchanged. Replays: `sdlpop2 --record NAME GAME_DIR [words]` records a session from the
program's start (the seed, the command-line words, the gameplay settings, the game's own PRINCE.OPT / HOF / SAV and every
video frame's input) into `replays/NAME.p2r`; `sdlpop2 --replay NAME GAME_DIR` plays it back and checks the final state and
screen (`source/replay.h`). `meson test -C build --suite settings` (with `-DgameDir`) checks that the ini's defaults are
the game's and that a scripted session records and replays identically.

Game controllers (`sdl/controller.c`, SDL's game controller database, hot-plugging; every connected controller drives the
game) give the game the keyboard's keys (`shell_input_key`: the arrows held, Shift, Ctrl, Esc, Alt+A, space), so the game
logic, replays and quicksaves are unchanged; the DOS game's own joystick mode (Alt+J) still reports "Joystick Not
Found". Default buttons (SDLPoP's layout, with PoP2's Ctrl on B): D-pad or left stick move (a diagonal = Home / PgUp /
End / PgDn), Y up, A down, X or a trigger Shift, B Ctrl, Start Esc (again: the pause ends), Back Alt+A (restart the level),
LB / RB quicksave / quickload, the right stick pressed space (the time left). In the menus A = Enter, B = Esc, X = Tab,
Y types the name "Prince" (the name fields), and on the title, in scenes and the demo any button is the "any key". The
prince losing hit points rumbles the controller (Kid +0x12 read by the frontend after each frame). `meson test -C build
--suite controller` checks the mapping with SDL's virtual joystick (no hardware; skipped when SDL cannot make one) and,
with `-DgameDir`, that a scripted controller session gives the same shell inputs and pop2_hash every frame as the
equivalent keyboard session.

## Core API (`source/core.h`)
    pop2_init("path/to/prince2");            // PRINCE.EXE, SEQUENCE.DAT, PRINCE.DAT, KID.DAT, guard and scenery DATs
    pop2_new_game(level, seed);              // seed: the DOS game seeds its RNG from the clock
    pop2_input in = { .x = 1 };              // x/y -1..1, shift, keystroke
    int r = pop2_frame(&in);                 // one game tick: POP2_PLAYING, POP2_QUIT, or the level just entered
    pop2_save(buf); pop2_load(buf); pop2_hash();   // pop2_state_size() bytes
In meson, `dependency` `sdlpop2Dependency` (source/meson.build; usable as a subproject). `tests/coretest.c` is a small
example (random play + savestate round trips).

## Layout
`source/*.c` by subsystem (`docs/ENGINE.md` maps them to the original segments and overlays), `source/glue.c` for the game
files and the overlay entry points, `source/state.c` for the state table, `docs/` for format notes (DAT resources,
SEQUENCE.DAT, levels), `sdl/` for the [SDL2](https://www.libsdl.org) frontend, `tools/` for the explorer and capture helpers, `tests/` for the
oracle comparisons (run by `meson test`, or directly: `tests/run_all.sh`, `tools/tiletests.sh`, `tests/run_shell.sh`).

## License and legal
Source available for noncommercial use: the [PolyForm Noncommercial License 1.0.0](https://polyformproject.org/licenses/noncommercial/1.0.0) (`LICENSE`) — use, change and
redistribute it for research, study, experiment, hobby and the other noncommercial purposes the license lists; no
credit asked for; no warranty. This is not an "open source" license in the OSI sense.

This is an unofficial reconstruction of Prince of Persia 2 (DOS) for research and education. It is not affiliated
with or endorsed by the game's rights holders, who own the game, its code, data and trademarks; the contributors
claim no ownership of it, and no game data is included (you need your own copy). The [Nuked OPL3](https://github.com/nukeykt/Nuked-OPL3) emulator
(`source/audio_opl3.*`) stays under the LGPL 2.1 or later. Details: `NOTICE`.

**Getting the game:** SDLPoP2 needs the original game's data files, which are not included. The game is not
currently sold digitally; the legal way to obtain them is an original copy of *Prince of Persia 2: The Shadow and
the Flame* for MS-DOS (1993, Brøderbund) or the *Prince of Persia Collection Limited Edition* CD (the version this
project targets). You can ask for an official re-release by voting on its
[GOG Dreamlist page](https://www.gog.com/dreamlist/game/prince-of-persia-2-the-shadow-and-the-flame).
