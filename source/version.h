#pragma once
/* The three DOS releases of the game (docs/VERSIONS.md): the initial release (IR), 1.0 and 1.1, the CD build this
 * reconstruction follows. `game_ver` is the one being played: the code tests it where the releases differ, and
 * everything else is 1.1. The game's files decide the rest: each release's own data files, and the static tables
 * of its PRINCE.EXE, which version_load_exe() brings into the layout of 1.1's data segment (the one every DS offset
 * in this code is written in). 1.0 and 1.1 share every data file, so either can be played with the other's. */
#include <stddef.h>
#include <stdint.h>

enum { POP2_VER_AUTO = -1, POP2_VER_11 = 0, POP2_VER_10 = 1, POP2_VER_IR = 2 };
extern int game_ver;        /* the release played (POP2_VER_11 unless chosen) */
extern int exe_ver;         /* the release of the PRINCE.EXE whose tables are loaded */
extern uint32_t exe_data_base;   /* its initialised data (the RTLink data overlay: frame tables at +0, the font at
                                    +0xD20, DS:0 at +0x2940) in the file: 1.1 0x3A500, 1.0 0x42710, IR 0x41C00 */

#define V_IR (game_ver == POP2_VER_IR)
#define V_10 (game_ver == POP2_VER_10)
#define V_11 (game_ver == POP2_VER_11)
#define V_1X (game_ver != POP2_VER_IR)   /* 1.0 or 1.1 */

const char *version_name(int v);        /* "IR", "1.0", "1.1" */
/* which release an executable is (by its title string: "PRINCE OF PERSIA 2 v1.1", "... 1.0", else the initial
 * release; the cracked copies differ only in a few code bytes) and where its data lies; -1 if it is not the game */
int version_of_exe(const uint8_t *exe, size_t n, uint32_t *data_base);
/* reads PRINCE.EXE: its release (exe_ver, exe_data_base) and its static data segment in 1.1's layout (0x27BF bytes:
 * the anchors of version_maps.inc, the DS pointer tables translated, 1.1's empty rect DS:1F12); 0 on failure */
int version_load_exe(const char *path, uint8_t *ds11);
/* where 1.1's DS offset lies in release v's data segment, and back */
uint16_t version_ds_to(int v, uint16_t off11);
uint16_t version_ds_from(int v, uint16_t off);
/* the release to play: `want` (POP2_VER_*, AUTO: the executable's). The initial release's data files differ from 1.0 /
 * 1.1's, so it needs its own files and they need theirs: NULL when the combination works, else why not */
const char *version_choose(int want);
