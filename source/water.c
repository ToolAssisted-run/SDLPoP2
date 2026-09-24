/* Level 5's water (rooms 7 | 10 | 12 as one 30-column row; OVL12 at 37F0, reached through RTLink thunks 2A31:0DE9 /
 * 0DD5 / 0DDF / 0DC1 / 0DCB / 0DF3 and the falling-object table): swimming in tile 0x2C, the waves (DS:2B78 bits by
 * column), and the plug: the prince and the character of room 10 standing together (columns 4..6 / 5..6) for 0x3C
 * ticks start DS:693E, which sends up to five bubbles (falling objects of type 0xB) from room 10's floor. Drawing,
 * palettes and sounds are left out. Transcribed from the disassembly. */
#include "types.h"
#include "globals.h"

int16_t word_693c;   /* DS:693C: ticks the two stood together */
uint8_t byte_2b78;   /* DS:2B78: the wave columns (bit c-1 for column c of room 10) */
#define word_693e (*(int16_t *)&water_693e[0])   /* DS:693E: 0 waiting, 1..3 starting, -1 done */
#define bubble_at (water_693e + 1)                /* DS:693F + column: a bubble came up there (1 small, 2 big) */

/* 37F0:0164: a column in room 10's coordinates (room 7 is -10..-1, room 12 10..19) */
static int8_t col10(uint8_t room, int8_t col)
{
	if (level_number == 5 && room == 7) col -= 10;
	else if (level_number == 5 && room == 0xC) col += 10;
	return col;
}
/* 37F0:0408: Char is in the water */
static int in_water(void) { return get_tile(Char.curr_row, Char.curr_col, Char.room) == 0x2C; }
/* 37F0:03A6: room 7's gate (position 13) is closed or closing */
static int gate_down(void) { trob_type *t = get_trob(0xD, 7); return t && (t->state == 0 || t->state == 2); }
/* 37F0:050C: swimming: y follows the row and the column's wave offset (DS:1C87) and bobs with the wave */
static void swim(void)
{
	if (!in_water() || (Char.frame >= 0x22 && Char.frame <= 0x2C)) return;
	Char.y = 0x3F * Char.curr_row + 0x38 + (int8_t)ds_byte(0x1C87 + Char.curr_col);
	load_frame();   /* 0AFF:02AC */
	if (!(frame_flags & 0x40)) return;   /* standing: bobs with the wave under him (room 10 row 1) */
	uint16_t w = *(uint16_t *)&ROOM_ATTRS(10)[Char.curr_col + 10] & 0xF;
	if (w == 1) Char.y--; else if (w == 2) Char.y++;
}
/* 37F0:0194: the prince */
static int kid_swims(void)
{
	loadkid();
	int r = in_water();
	if (r) { swim(); if (Char.frame == 0xF || Char.frame == 0xB9) r = 0; }
	else if (Char.room == 7 && Char.f10 != 0xFF && gate_down() && Char.curr_col <= 5) ovl_button22(7, 0xD);   /* OVL04 33FD:0000 */
	Kid = Char;
	return r;
}
/* 37F0:0206: the drawn room's first character */
static int char0_swims(void)
{
	if (room_nchars(drawn_room) == 0) return 0;
	load_char(0);
	int r = in_water(); if (r) swim();
	save_char();
	return r;
}
/* 37F0:04C6: the wave bits around column c */
static void wave_cols(int8_t c)
{
	int e = c + 1; if (e > 8) e = 8; e--;
	int k = c - 1; if (k < 1) k = 1; k--;
	for (; k <= e; k++) byte_2b78 |= (uint8_t)(1 << k);
}
/* 37F0:0454: waves where the swimmers are (all when both swim); a splash sound now and then */
static void waves(int chr, int kid)
{
	int si = 2; byte_2b78 = 0;
	if (kid && chr) { byte_2b78 = 0xFF; si = 1; }
	else if (kid) wave_cols(Kid.curr_col);
	else if (chr) wave_cols(chars[0].curr_col);
	else si = 0;
	if (si && !sound_playing(0x2753) && !random_2751(si * 0x28)) play_sound(0x43);
}
/* 37F0:094A: a bubble rises from room's floor position tp (falling object type 0xB) */
static void bubble(int8_t tp, uint8_t room)
{
	ROOM_TILES(10)[tp] = 0; *(uint16_t *)&ROOM_ATTRS(10)[tp] = 0xC000;
	cur_mob.w7 = 0; cur_mob.speed = 0;
	int8_t col = tp % 10, row = tp / 10;
	cur_mob.x = (int16_t)ds_word(0x0D26 + 2 * col);
	int si;
	if (random_2751(1)) { cur_mob.wd = 5; si = 2; } else { cur_mob.wd = 0; si = 1; }
	bubble_at[col] = (uint8_t)si; cur_mob.wd |= si << 4;
	cur_mob.room = room; cur_mob.row = (uint8_t)row;
	cur_mob.y = (int16_t)ds_word(0x0D40 + 2 * row) + (int8_t)ds_byte(0x1C87 + col);
	cur_mob.type = 0xB; add_mob_pub();   /* (1375:2296 adds its sprite) */
}
/* 37F0:001E: the plug */
static void plug(int chr, int kid)
{
	if (word_693e == 0) {
		if (kid && chr) {
			int d = Kid.curr_col - chars[0].curr_col; if (d < 0) d = -d;
			if (d <= 3) {
				if (++word_693c >= 0x3C && Kid.curr_col >= 4 && Kid.curr_col <= 6 && chars[0].curr_col >= 5 && chars[0].curr_col <= 6 && Kid.f19 != 0x55) word_693e = 1;
			} else if (word_693c) word_693c--;
		} else if (word_693c > 1) word_693c -= 2;
		return;
	}
	if (word_693e <= 0) return;
	if (word_693e != 3) { word_693e++; return; }
	int8_t k = 7;
	while (k >= 3 && bubble_at[k] != 0) k--;
	if (k >= 3) bubble(k + 10, 10);
	if (k == 7) { play_sound(0x44); sound_1611_01a8(0x42); }
	if (k >= 3 && Kid.f19 != 0x55) return;
	word_693e = -1;
	loadkid();
	if (Char.curr_col <= 4 && Char.direction == 0) Char.x = char_dx_forward(0xA);
	Kid = Char;
}
/* 37F0:0000 (OVL04 33FD:0BEA, the kind-3 tick on level 5 in rooms 10 / 7 / 12) */
void water_tick(void)
{
	int kid = kid_swims(), chr = char0_swims();
	waves(chr, kid); plug(chr, kid);
}
/* 37F0:0426 (room hook 0x21 and room 7 / 12 without a description): palette; the waves and the count restart */
void water_room_enter(void) { byte_2b78 = 0; word_693c = 0; }
/* 37F0:023C (2A31:0DD5, guards and the prince with the sword in the water rooms): an edge of the water ahead */
static int opp_at_edge(void)   /* 37F0:035E */
{
	int8_t c = col10(Opp.room, Opp.curr_col);
	return gate_down() && (Opp.direction == 0 || Opp.direction == -1) && c < 2;
}
int rtlink_0dd5(void)
{
	int8_t c = col10(Char.room, Char.curr_col);
	if (Char.direction == 0) return c < 2 ? 1 : opp_at_edge();
	return c >= 8;
}
/* 37F0:0286 (2A31:0DDF, the skeleton in the water rooms): 1 = go on as usual */
int ovl_2a31_ddf(void)
{
	int8_t c = col10(Char.room, Char.curr_col);
	if (Char.action == 5) return 1;
	if (Char.action == 4 && !(Char.curr_row < 3 && level_number == 5 && (Char.room == 10 || Char.room == 7 || Char.room == 12))) { clear_char(); return 0; }
	if (Char.direction == 0) {
		if (Char.f19 == 0x66 || Char.f19 == 0xD8) {
			if (c < 9 && gate_down()) return 0;
			Char.f23 = 2; Char.f10 = 1; seqtbl_offset_char(0x65); return 0;
		}
		if (Char.x <= Opp.x) return 1;
		if (opp_at_edge()) { guard_sheathe_pub(); return 0; }   /* 366C:00DE */
		if (c >= 9) { sword_engage_pub(); return 0; }         /* 2FDF:1FB6 */
		return 1;
	}
	if (opp_at_edge()) { Char.direction = 0; Char.x = char_dx_forward(0xA); return 0; }
	return 1;
}
/* 37F0:0588 (1375:0096, tile 0x2C in room 10): the wave frame (low nibble 0..1) where DS:2B78 has the column */
void anim_tile2c(void)
{
	if (level_number != 5 || drawn_room != 10) { cur_trob.state = 0xFF; return; }
	uint8_t old = (uint8_t)anim_mod & 0xF, v;
	anim_mod &= ~0xFu;
	int bit = 1 << ((int8_t)cur_trob.tilepos % 10 - 1);
	if (!(byte_2b78 & bit)) v = 0;
	else {
		int r = random_2751(3);
		if (r <= 1) v = old;
		else if (old != 0) v = 0;
		else { v = (uint8_t)(r - 1); anim_mod |= v; }
	}
	hook_water_wave((int8_t)cur_trob.tilepos, v);   /* 37F0:06CE redraws the description's wave with frame v */
}
/* the drawing's part of the water's tick code (the renderer's through shell.c; a no-op in the core) */
__attribute__((weak)) void hook_water_wave(int8_t tp, uint8_t v) { (void)tp; (void)v; }
/* 37F0:0742 (0823:0B78, tile 0x2C on entry) */
void anim_start_2c(uint32_t *attrs, int8_t tp, uint8_t room)
{
	*(uint16_t *)&attrs[tp] &= 0xFFF0;
	add_trob(0x2C, 1, tp, room);
	hook_water_wave(tp, (uint8_t)(attrs[tp] & 0xF0));   /* 37F0:06CE */
}
/* 37F0:08F6 (1375:1B10, falling object type 0xB): a bubble grows (small to 4, big to 9 and gone) */
void bubble_update(void)
{
	if (level_number != 5 || drawn_room != 10) { cur_mob.speed = -1; return; }
	int dx = cur_mob.wd & 0xF, bx = (cur_mob.wd & 0x30) >> 4;
	if ((bx == 1 && dx != 4) || (bx == 2 && dx != 9)) { cur_mob.wd = (int16_t)((cur_mob.wd & 0xFFF0) | (dx + 1)); return; }
	if (bx == 2 && dx == 9) cur_mob.speed = -1;
}
/* 37F0:03D2 (366C, a guard advancing in the water rooms): still room in front (column < 3 facing right, > 3 facing left) */
int ovl_383d2(void)
{
	int8_t c = col10(Char.room, Char.curr_col);
	return Char.direction == 0 ? c < 3 : c > 3;
}
