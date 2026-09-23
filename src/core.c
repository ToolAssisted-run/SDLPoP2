/* SDLPoP2 core API (core.h) over the reconstructed game. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "globals.h"
#include "glue.h"
#include "state.h"
#include "core.h"

extern char glue_dir[400]; extern int pop2_keystrokes; extern uint8_t key_table[0x70], bios_shift_flags;
static int scene; static uint8_t ram[655360];   /* DS at 0x3B250, as the program starts */

int pop2_init(const char *dir)
{
	snprintf(glue_dir, sizeof glue_dir, "%s", dir);
	char p[512];
	snprintf(p, sizeof p, "%s/SEQUENCE.DAT", dir); if (!glue_open_seq(p)) return 0;
	snprintf(p, sizeof p, "%s/PRINCE.EXE", dir);
	/* the data segment as the program starts: PRINCE.EXE's initialised data (file 0x3CE40 = DS:0), the rest zero */
	FILE *f = fopen(p, "rb"); if (!f) return 0;
	fseek(f, 0x3CE40, SEEK_SET); size_t n = fread(ram + 0x3B250, 1, 0x27BF, f); fclose(f); if (n != 0x27BF) return 0;
	glue_load_exe_tables(p); glue_load_ds_tables(ram);
	return 1;
}
void pop2_new_game_loaded(int lv, uint32_t seed)   /* up to the level load (169B:00F5) */
{
	static uint8_t *zero; size_t n = state_size();
	if (!zero) zero = calloc(1, n);
	state_load(zero); state_load_ds_statics(ram + 0x3B250);   /* the program's memory at start */
	random_seed = seed; cheat_mode = lv != 1; level_switch = lv != 1; byte_6b6c = (uint8_t)lv; pop2_keystrokes = 0;
	game_start();
	scene = story_scene((int8_t)word_32d8, lv); scene_played(scene);
	load_level(lv);
}
void pop2_new_game(int lv, uint32_t seed) { pop2_new_game_loaded(lv, seed); level_begin(); level_first_room(); }
int pop2_frame(const pop2_input *in)
{
	memset(key_table, 0, sizeof key_table);
	if (in->x < 0) key_table[0x58] = 1; else if (in->x > 0) key_table[0x5A] = 1;
	if (in->y < 0) key_table[0x55] = 1; else if (in->y > 0) key_table[0x5D] = 1;
	bios_shift_flags = in->shift == 2 ? 4 : in->shift ? 2 : 0;   /* 2 = Ctrl (the sword / the spirit's cast) */
	if (in->keystroke) pop2_keystrokes = 1;
	missing_reset();
	int r = play_frame();
	pop2_keystrokes = 0;
	if (r == -2 || r == -1) return r;
	if (r <= 0 || r > 14) return POP2_QUIT;   /* 0: back to the title */
	scene = word_5cb6 ? 0 : story_scene((int8_t)word_32d8, r); scene_played(scene);   /* 0AAC:000E */
	if (!load_level(r)) return POP2_QUIT;
	level_begin(); level_first_room();
	return r;
}
int pop2_level(void) { return level_number; }
int pop2_scene_skipped(void) { return scene; }
const char *pop2_missing(void) { return missing_log(); }
size_t pop2_state_size(void) { return state_size() + sizeof scene; }
void pop2_save(void *buf) { state_save(buf); memcpy((uint8_t *)buf + state_size(), &scene, sizeof scene); }
void pop2_load(const void *buf) { state_load(buf); memcpy(&scene, (const uint8_t *)buf + state_size(), sizeof scene); }
uint64_t pop2_hash(void) { return state_hash(); }
