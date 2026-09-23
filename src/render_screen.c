/* The screen: the offscreen buffer's changed rectangles copied to the VGA memory (0FB3:218A -> 0823:1326, upside-down
 * through 2699:01A6 / 194C:1346), the whole screen after a full redraw, the status line drawn straight into the VGA
 * memory. Transcribed from the disassembly. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "globals.h"
#include "render.h"
#include "render_tiles.h"
#include "render_frame.h"

/* 194C:1346 (2699:01A6): the offscreen buffer upside down (its 192 rows swapped end for end) */
static void flip_offscreen(void)
{
	uint8_t row[SCREEN_W];
	for (int y = 0; y < 96; y++) {
		memcpy(row, offscreen + y * SCREEN_W, SCREEN_W);
		memcpy(offscreen + y * SCREEN_W, offscreen + (191 - y) * SCREEN_W, SCREEN_W);
		memcpy(offscreen + (191 - y) * SCREEN_W, row, SCREEN_W);
	}
}
/* 0823:1326: a rectangle of the offscreen buffer to the screen (upside-down: the rectangle mirrored, DS:5D38) */
void render_copy_rect(const int16_t *rect)
{
	int16_t r[4]; memcpy(r, rect, sizeof r);
	if (word_5d38) { int16_t t = r[0]; r[0] = (int16_t)(0xC0 - r[2]); r[2] = (int16_t)(0xC0 - t); }
	for (int y = r[0] < 0 ? 0 : r[0]; y < r[2] && y < 192; y++)
		for (int x = r[1] < 0 ? 0 : r[1]; x < r[3] && x < SCREEN_W; x++) screen_buf[y * SCREEN_W + x] = offscreen[y * SCREEN_W + x];   /* 194C:4CB0 (CopyBits, clipped to the ports) */
}
/* 169B:0AB3 .. 0AD5: the frame's changed rectangles to the screen (0FB3:218A: last first), the offscreen buffer
 * turned upside down around the copies while DS:5D38 counts */
void render_present(void)
{
	if (word_5d38) flip_offscreen();
	while (dirty_count) { dirty_count--; render_copy_rect(dirty_rects[dirty_count]); }
	if (word_5d38) flip_offscreen();
}
/* 169B:04CC .. 04F6 (after a full redraw): the whole screen (DS:097E) copied, upside down while DS:5D38 counts */
void render_present_all(void)
{
	int16_t r[4]; for (int k = 0; k < 4; k++) r[k] = (int16_t)ds_word((uint16_t)(0x097E + 2 * k));
	if (word_5d38) flip_offscreen();
	render_copy_rect(r);
	if (word_5d38) flip_offscreen();
	dirty_count = 0;
}

/* ---- the status line: hit points (drawn straight to the screen) ---- */
static int16_t ds_w(uint16_t a) { return (int16_t)ds_word(a); }
/* 0FB3:24EA: the prince's hit points: `hp` full flasks (KID.DAT image 0xD8) from x 2 by 8, then empty ones (0xD9)
 * up to `max`; below the level's starting hit points (DS:6B71) the rest of the bar up to x 0x62 erased */
void render_kid_hp(int hp, int max)
{
	int si = 2;
	for (int k = 0; k < hp; k++, si += 8) render_image_to_screen(2, 0xD8 + 1, si, 0xC1, 0, 1);   /* 26BC:0592 */
	if (hp < max) for (int k = 0; k < max - hp; k++, si += 8) render_image_to_screen(2, 0xD9 + 1, si, 0xC1, 0, 1);
	if (max >= 1 && (int8_t)Char.f13 < (int8_t)start_hp) {
		int16_t r[4] = {0xC1, (int16_t)((((int8_t)Kid.f13 - max) << 3) + si), ds_w(0x1F2E), 0x62};
		render_erase_screen(r);
	}
}
/* 0FB3:25D4: an opponent's hit points (index, hp, max): from x 0x134 leftwards by 10, KID.DAT image 0x83 (the heads,
 * charids 7 / 8, count two a flask: an odd one is image 0x84), the rest of 8 erased; nothing (the right part
 * erased) for no opponent, the prince, the riser, the shadow, level 5's guards out of row 0 in rooms 0xA / 7 / 0xC */
void render_opp_hp(uint8_t index, int hp, int max)
{
	(void)max;
	if (index == 0xFF) {
		index = (uint8_t)find_opponent(1);   /* 2D3E:08E8 */
		if (index == 0xFF && room_nchars(drawn_room) != 0) index = 0;
	}
	if (index != 0xFF) load_char(index);
	int erase_all = index == 0xFF || room_nchars(drawn_room) == 0 || Char.charid == 0xA || Char.charid == 0xB || Char.charid == 0
	             || (Char.charid == 4 && Char.curr_row != 0 && level_number == 5 && (Char.room == 0xA || Char.room == 7 || Char.room == 0xC));
	if (erase_all) { int16_t r[4] = {0xC1, 0xDA, ds_w(0x1F2E), ds_w(0x1F30)}; render_erase_screen(r); return; }
	if (word_5cdc == 0x258) return;
	int si = 0x134, odd = 0;
	if (Char.charid == 7 || Char.charid == 8) { odd = hp % 2; hp /= 2; }
	for (int k = 0; k < hp; k++, si -= 10) render_image_to_screen(2, 0x83 + 1, si, 0xC1, 0, 1);
	if ((Char.charid == 7 || Char.charid == 8) && odd) { render_image_to_screen(2, 0x84 + 1, si, 0xC1, 0, 1); si -= 10; }
	if (hp < 8) for (int k = 0; k < 8 - hp; k++, si -= 10) { int16_t r[4] = {0xC1, (int16_t)si, ds_w(0x1F2E), (int16_t)(si + 10)}; render_erase_screen(r); }
}
/* 0823:0F38 (0993:029A, every frame): the hit points redrawn when they change; the last one blinks (frame / clock
 * parity) */
void render_status_hp(void)
{
	Char = Kid;
	if (Char.hp_delta) render_kid_hp((int8_t)Char.f12, (int8_t)Char.f13);
	if (Char.f12 == 1) { if (tick & 1) render_kid_hp(1, 0); else render_kid_hp(0, 1); }
	Kid = Char;
	if (Kid.opp_index == 0xFF) return;
	load_char(Kid.opp_index);
	int heads2 = (Char.charid == 7 || Char.charid == 8) && Char.f24 == 2;
	if (Char.hp_delta && !heads2) render_opp_hp(Char.index, (int8_t)Char.f12, (int8_t)Char.f13);
	if (Char.f12 == 1 && !heads2) render_opp_hp(Char.index, (clock_ticks & 1) ? 1 : 0, (int8_t)Char.f13);
}
/* 0FB3:259C: both bars redrawn (the opponent's when one is tracked) */
void render_hp_bars(void)
{
	if ((int8_t)Kid.opp_index >= 0 && Kid.opp_index < 5) { Opp = chars[Kid.opp_index]; render_opp_hp(Opp.index, (int8_t)Opp.f12, (int8_t)Opp.f13); }   /* (0AFF:080A: into Opp) */
	render_kid_hp((int8_t)Kid.f12, (int8_t)Kid.f13);
}

/* 0FB3:29B8 (0823:0E72, a room switch): colors 0x10..0xFF saved and black, and (unless already so) the prince's old
 * box (Kid +0x2C, DS:5B62) erased on the screen, cut to DS:097E */
void render_room_switch(const int16_t *old_box)
{
	if (render_pal_saved()) return;
	render_pal_blackout();
	int16_t r[4], g[4]; for (int k = 0; k < 4; k++) g[k] = (int16_t)ds_word((uint16_t)(0x097E + 2 * k));
	if (old_box[0] >= old_box[2] || old_box[1] >= old_box[3]) return;   /* 194C:4D50 */
	if (render_sect_rect(r, old_box, g)) render_erase_screen(r);
}

/* ---- the status line's message area (the messages themselves are text.c's) ---- */
/* 0FB3:2104: the whole status line (DS:098E moved to x 0, to x 0x140) in color 0 */
void render_msg_erase(void)
{
	int16_t r[4]; for (int k = 0; k < 4; k++) r[k] = ds_w((uint16_t)(0x098E + 2 * k));
	r[3] = (int16_t)(r[3] - r[1]); r[1] = 0; r[3] = 0x140;   /* 194C:500C (offset by -left) */
	render_erase_screen(r);   /* 194C:4D72 */
}
/* 0FB3:2136 (clear_count): the message area DS:098E erased (194C:6632), the whole status line while the countdown
 * after a death (DS:5CDC 0x258) shows; with clear_count the countdown DS:5CDC / 5CDA is reset (game logic's) */
void render_msg_clear(void)
{
	if (word_5cdc == 0x258) { render_msg_erase(); return; }
	int16_t r[4]; for (int k = 0; k < 4; k++) r[k] = ds_w((uint16_t)(0x098E + 2 * k));
	render_erase_screen(r);
}
