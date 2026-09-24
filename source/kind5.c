/* Level kind 5 (level 1), OVL02 at 33FD: the sea below the ship (rooms 0x10 and 0x13): a character that sinks below
 * y 0xAC is grabbed (DS:6936 = its index, DS:6937 = step 1..8; the prince is index 10) and at the 8th step dies on
 * the bottom row; room 0x13's railing nudges the prince; the rooms' palette flag DS:2B68; the walk into the sea at
 * the end (room 15). Palettes, drawing and sounds are left out. Transcribed from the disassembly. */
#include "types.h"
#include "globals.h"

int16_t word_6938;   /* DS:6938: the grab's x */
uint8_t byte_693a;   /* DS:693A: the grabbed character was hanging (action 9) */

/* 33FD:0370: the sea takes Char */
void grab_start(void)
{
	if (byte_6937 != 0) return;
	byte_9276 = Char.index; byte_6937 = 1; byte_693a = Char.action == 9;
	word_6938 = Char.x - 0x82 + (Char.direction == 0 ? -(image_width / 2) : image_width / 2);
	play_sound(0x30);
}
/* 33FD:0068: Char in the sea rooms */
static void grab_check(void)
{
	if (byte_9276 == 0xFF) {
		if (Char.y < 0xAC || Char.f19 == 0x44 || Char.f19 == 0xF || Char.f19 == 0x3B || Char.action == 2) return;
		grab_start(); return;
	}
	if (Char.index != byte_9276) return;
	if ((int8_t)byte_6937 < 7) {
		int bg = room_background_id(); if (bg != 0x12 && bg != 0x13) return;   /* (1375:0F5A redraws the hand) */
		byte_6937++; return;
	}
	if ((int8_t)byte_6937 < 8) { byte_6937++; return; }
	byte_6937 = 0; byte_9276 = 0xFF;
	if (Char.curr_row >= 2) { Char.frame = 0xB9; take_hp(100); seq_set_85f8(0xF); }
}
/* 33FD:0AE2 (2FDF, the prince's controls): at room 0x13's left part, running, jumping or on frame 0x50 */
int ovl_34ab2(void) { return Char.room == 0x13 && Char.x < 0xD0 && (Char.action == 2 || Char.action == 6 || Char.frame == 0x50); }
static int on_railing(void) { return Char.room == 0x13 && Char.f19 == 0x3B; }   /* 33FD:0AA0 */
/* 33FD:03C8: the prince */
static void kind5_kid(void)
{
	if (Kid.room != 0x13 && Kid.room != 0x10 && byte_9276 != 0xA) return;
	loadkid(); grab_check();
	if (Kid.room == 0x13 && (ovl_34ab2() || on_railing())) {
		Char.x--;
		if (on_railing() && Char.frame == 0x95) { /* 1611:001C music */ }
	}
	Kid = Char;
}
/* 33FD:0428: the drawn room's characters */
static void kind5_chars(void)
{
	int8_t n = room_nchars(drawn_room);
	for (int8_t i = 0; i < n; i++) {
		load_char(i);
		if (Char.room == 0x13 || Char.room == 0x10 || byte_9276 == (uint8_t)i) { grab_check(); save_char(); }
	}
}
/* 33FD:01CE: in rooms 0x13/0x10/0xF (drawn), every third tick while on time (DS:2BA4 0), the sea's colors 0xE6..0xE8,
 * 0xE9..0xEB and 0xEC..0xED rotate (2699:0048: the renderer's) */
static void kind5_palette(void)
{
	if (word_2ba4 != 0 || (drawn_room != 0x13 && drawn_room != 0x10 && drawn_room != 0xF) || tick % 3 != 0) return;
	hook_pal_rotate(0xE6, 3); hook_pal_rotate(0xE9, 3); hook_pal_rotate(0xEC, 2);
}
/* 33FD:0232 (DS:0664): 01CE, 03C8, 0428 */
void kind5_tick(void) { kind5_palette(); kind5_kid(); kind5_chars(); }
/* 33FD:0128 / 0186 (0CD6:003A in the full redraw): the sea rooms' palette; DS:2B68 = 1 in rooms 0x13/0x10/0xF, 0 elsewhere */
void kind5_room_palette(uint8_t room)
{
	if (level_kind != 5) return;
	if (room == 0x13 || room == 0x10 || room == 0xF) { if (byte_2b68 != 1) byte_2b68 = 1; }
	else if (byte_2b68 != 0) byte_2b68 = 0;
}
/* 33FD:0240 (2D3E:1746, the prince leaving room 15 to the right): past x 0x201 he walks into the sea: with digital
 * sound (DS:2085 bit 0) the waves (sound 0x20) play first while neither 0x2730 nor 0x273F is playing; then every hp
 * goes, one per step of the display (frame waits 2797:0104), and he sinks (seq 0x47) */
void ovl_34210(void)
{
	if (Char.x <= 0x201) return;
	if (sound_digital() && !sound_playing(0x2730) && !sound_playing(0x273F)) { play_sound(0x20); return; }
	for (int n = (int8_t)Char.f12; n > 0; n--) { take_hp(1); Kid = Char; apply_hp_deltas(); hp_bars_reload(); loadkid(); }
	char_y_to_floor(); seqtbl_offset_char(0x47);
}
