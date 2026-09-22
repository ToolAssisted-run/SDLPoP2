/* The game logic of one tick (169B:05E0), without drawing and sound. */
#include "types.h"
#include "globals.h"

uint16_t word_5ce8;   /* DS:5CE8 (8628): nonzero during cutscenes, no sword hits */
/* 169B:05E0: returns 0 normally, -2 frozen prince, -1 quit (level restart handled by the caller) */
int tick_body(void)
{
	falling_floors();                              /* 1375:1A52 */
	animate_tiles();                               /* 1375:0006 */
	if (level.type == 2 || chars[0].charid == 10) ovl_366c_f60();
	spawn_guards(drawn_room);                      /* 2D3E:0A4A */
	guards_see_kid();                              /* 169B:0FF0 */
	int r = play_kid_frame();
	if (r == 0) {
		play_all_chars();
		if (word_5ce8 == 0 && drawn_room != 0) { check_sword_hits(); process_hurt(); }
		checkpoints_0db4();                        /* 169B:0DB4 */
		level_kind_tick();                         /* 169B:11E2 */
		apply_hp_deltas();                         /* 0823:1008 */
		check_kid_left_room();                     /* 2D3E:108A */
		switch_room();                             /* 0823:0E72 */
		return 0;
	}
	if (r == -1) return -1;
	play_all_chars(); level_kind_tick(); apply_hp_deltas();
	return -2;
}
