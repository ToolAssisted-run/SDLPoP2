/* The prince's control step (0AFF:10E8 / 11F8): input to the ctrl1 press states, control() in facing-relative terms,
 * and the death count that leads to the restart prompt. */
#include "types.h"
#include "globals.h"

int8_t kid_ctrl1_saved[5];     /* DS:6128: the prince's ctrl1_* between ticks (the characters' control() reuses DS:6122) */
uint16_t word_5d38;            /* DS:5D38: nonzero = up and down swapped */
uint16_t word_5cd0;            /* DS:5CD0: show the time left */
uint16_t word_5cda, word_5cdc; /* DS:5CDA / 5CDC: countdown and its start (0x258 after a death) */

/* 0AFF:1336 / 1356: make control_x (resp. y) relative to the facing direction by swapping the ctrl1 pairs */
static void flip_x(void) { control_x = -control_x; int8_t t = ctrl1_forward; ctrl1_forward = ctrl1_backward; ctrl1_backward = t; }
static void flip_y(void) { control_y = -control_y; int8_t t = ctrl1_up; ctrl1_up = ctrl1_down; ctrl1_down = t; }
/* one press state: -1 newly pressed (kept until control() consumes it, then 1), 0 released */
static void press(int8_t *c, int held) { if (*c >= 0) *c = held ? (*c == 0 ? -1 : *c) : 0; }
/* 0AFF:13B8 (left = forward, right = backward before the flip) */
static void read_user_control(void)
{
	press(&ctrl1_forward, control_x == -1); press(&ctrl1_backward, control_x == 1);
	press(&ctrl1_up, control_y == -1); press(&ctrl1_down, control_y == 1);
	if (control_shift == -1) { if (ctrl1_shift != 1) ctrl1_shift = -1; }
	else if (control_shift == -2) { if (ctrl1_shift != 2) ctrl1_shift = -2; }
	else ctrl1_shift = 0;
}
/* 0AFF:12CA */
static void kid_control(void)
{
	int8_t dir = Char.direction;
	if (dir == 0) flip_x();
	if (word_5d38) flip_y();
	if (word_2ba8) ovl_15db_64();
	control();
	if (dir == 0) flip_x();
	if (word_5d38) flip_y();
	if (Char.f19 == 0xED && Char.frame == 0xB9 && Char.room == 9 && level_number == 8) note_missing("2A31_0E1B");
}
/* 0AFF:11F8 */
static int kid_input_and_control(void)
{
	if (Char.alive < 0 && Char.f12 == 0) Char.alive = 0;
	if (word_8a84) word_8a84--;
	ctrl1_forward = kid_ctrl1_saved[0]; ctrl1_backward = kid_ctrl1_saved[1]; ctrl1_up = kid_ctrl1_saved[2]; ctrl1_down = kid_ctrl1_saved[3]; ctrl1_shift = kid_ctrl1_saved[4];
	read_input();
	if ((int8_t)word_32d8 != (int16_t)counter_5cec || Char.room == 0 || Char.f19 == 0x3B) control_x = control_y = 0;   /* level over, outside, or seq 0x3B */
	read_user_control();
	kid_control();
	int si = -2;   /* 15DB:000C: -1 when a recorded demo runs out */
	kid_ctrl1_saved[0] = ctrl1_forward; kid_ctrl1_saved[1] = ctrl1_backward; kid_ctrl1_saved[2] = ctrl1_up; kid_ctrl1_saved[3] = ctrl1_down; kid_ctrl1_saved[4] = ctrl1_shift;
	return si;
}
/* 0AFF:10E8: -2 normally, -1 to leave the level (restart prompt) */
int play_kid_control(void)
{
	int si = kid_input_and_control();
	if (si != -2 || Char.alive < 0 || !is_dead_frame(Char.frame)) return si;
	if (word_5ce8) note_missing("DEAD_IN_CUTSCENE");   /* 0AFF:1117: revive and continue the cutscene */
	if (Char.charid != 0 && Char.charid != 1) return si;
	if (death_sound_playing(0)) return si;   /* DS:0882 still playing (unless it is sound 4, 7 or 0x36) */
	word_5cd0 = 0;
	if (Char.alive < 6) { Char.alive++; return si; }
	if (Char.alive == 6) { seq_music_1611(byte_5cb8); Char.alive++; return si; }
	if (Char.alive != 7 || death_sound_playing(1)) return si;   /* DS:0882 or DS:0884 still playing */
	if (minutes_left == 0) { restart_prompt(); si = -1; }   /* out of time */
	else if (word_5cdc != 0x258) { word_5cdc = word_5cda = 0x258; note_missing("FB3_20A4"); }
	Char.alive++;
	return si;
}
