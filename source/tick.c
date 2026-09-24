/* The game logic of one tick (169B:05E0), without drawing and sound. */
#include "types.h"
#include "globals.h"

uint16_t word_5ce8;   /* DS:5CE8 (8628): nonzero during cutscenes, no sword hits */
/* 1611:0164: the prince's opponent becomes the current character again; a sound when either stands on frame 0xA7
 * (unless that one is character type 7 or 8) */
static void reload_opponent(void)
{
	if (Kid.opp_index == 0xFF) return;
	load_char(Kid.opp_index);
	if ((Kid.frame == 0xA7 || Char.frame == 0xA7) && Char.charid != 7 && Char.charid != 8) play_sound(0xC);
}
/* 169B:05E0 up to 0823:0E72 (the captures' ds_postroom point): 0 normally, -2 frozen prince, -1 quit */
int tick_main(void)
{
	falling_floors();                              /* 1375:1A52 */
	animate_tiles();                               /* 1375:0006 */
	if (level.type == 2 || chars[0].charid == 10) skeleton_wake();   /* 366C:0F60 */
	spawn_guards(drawn_room);                      /* 2D3E:0A4A */
	guards_see_kid();                              /* 169B:0FF0 */
	int r = play_kid_frame();
	if (r == 0) {
		play_all_chars();
		if (word_5ce8 == 0 && drawn_room != 0) { check_sword_hits(); process_hurt(); }
		reload_opponent();                         /* 1611:0164 */
		checkpoints_0db4();                        /* 169B:0DB4 */
		level_kind_tick();                         /* 169B:11E2 */
		apply_hp_deltas();                         /* 0823:1008 */
		check_kid_left_room();                     /* 2D3E:108A */
		switch_room();                             /* 0823:0E72 (returns -2) */
		return 0;
	}
	if (r == -1) return -1;
	play_all_chars(); level_kind_tick(); apply_hp_deltas();
	return -2;
}
/* 169B:0656..0670, after a normal tick: fallen characters, the clock, and the prompt when time is up */
int tick_tail(void)
{
	chars_fell_below();                            /* 2D3E:0FB0 */
	game_clock();                                  /* 0823:0D5A */
	if (minutes_left == 0) { restart_prompt(); return -1; }   /* 169B:123E */
	return 0;
}
/* 169B:05E0: the whole tick */
int tick_body(void) { int r = tick_main(); return r == 0 ? tick_tail() : r; }
