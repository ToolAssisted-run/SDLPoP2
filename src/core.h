/* SDLPoP2 core: the game logic of Prince of Persia 2 (DOS 1.0), headless and savestate-able.
 * One pop2_frame() is one game tick (the DOS game's 169B:0505 pass). Drawing and sound are not part of it. */
#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct pop2_input {
	int8_t x, y;         /* -1 left / up, 1 right / down (a 3x3 grid, as the arrows and Home/PgUp/End/PgDn give) */
	uint8_t shift;       /* 1 shift held, 2 Ctrl held (draw the sword; the spirit casts) */
	uint8_t keystroke;   /* a key was pressed this tick (a dead prince restarts on any key) */
} pop2_input;

enum { POP2_PLAYING = -2, POP2_QUIT = -1 };

int  pop2_init(const char *game_dir);             /* PRINCE.EXE, SEQUENCE.DAT, PRINCE.DAT, KID.DAT, ... ; 0 on failure */
void pop2_new_game(int level, uint32_t seed);     /* a new game at `level` (1 = the menu's start; 2..14 = LEVELn with the cheat word) */
int  pop2_frame(const pop2_input *in);            /* one tick: POP2_PLAYING, POP2_QUIT, or the level just entered */
int  pop2_level(void);                            /* the current level */
int  pop2_scene_skipped(void);                    /* the story scene (NIS) that would have played before this level (0 none) */
const char *pop2_missing(void);                   /* routines not reconstructed yet that this tick reached (empty if none) */

size_t   pop2_state_size(void);
void     pop2_save(void *buf);
void     pop2_load(const void *buf);
uint64_t pop2_hash(void);
void pop2_new_game_loaded(int level, uint32_t seed);   /* (tests) pop2_new_game up to the level load */
