/* Slicer blades (tiles 0xC + 0xD, one blade over two columns) of level kinds 2 and 4: OVL05, loaded at 33FD.
 * The animation state (attribute low 5 bits) runs 0..0x12; the blade is closed (deadly) at 2 and 0x10/0x11. */
#include <string.h>
#include "types.h"
#include "globals.h"

static const uint8_t *bl_ds;   /* DS:176C redraw box, DS:1774 / 177C blade boxes, DS:1784 frame timing */
void blades_set_tables(const uint8_t *ds) { bl_ds = ds; }
static int16_t dsw(uint16_t a) { return (int16_t)(bl_ds[a] | bl_ds[a + 1] << 8); }
static uint16_t *attr_word(uint8_t room, int8_t tp) { return (uint16_t *)((uint8_t *)&level + 0x348 + (room * 30 + tp) * 4); }   /* DS:2F00 */

/* 33FD:0322: redraw (off screen: the animation keeps running, state 1) */
static void blade_redraw(void) { if (!anim_visible_pub()) cur_trob.state = 1; }
/* 33FD:0000: animation of tile 0xD; the left half (0xC) gets the same state */
void anim_blade(void)
{
	int si = (uint16_t)anim_mod & 0x1F;
	if (si >= 0x13) { anim_mod = (anim_mod & ~0xFFu) | ((uint8_t)anim_mod & 0xE0); cur_trob.state = 0xFF; }
	else {
		anim_mod = (anim_mod & 0xFFFF0000u) | (uint16_t)((uint16_t)anim_mod + 1);
		if (dsw(0x1784 + 2 * si) != dsw(0x1786 + 2 * si)) blade_redraw();
		if (si == 0) play_sound(0x54);
		else if (si == 0xF) play_sound(0x53);
		else if (si >= 0x10 && (anim_mod & 0x40) && !(anim_mod & 0x80)) { anim_mod |= 0x80; blade_redraw(); }
	}
	get_tile(cur_trob.tilepos / 10, cur_trob.tilepos % 10 - 1, cur_trob.room);
	curr_modifier = (curr_modifier & 0xFF00) | (uint8_t)anim_mod;
	*attr_word(curr_room, curr_tilepos) = curr_modifier;
}
/* 33FD:0412: set a blade going (from either half) unless it runs already */
static void blade_trigger(uint8_t room, int8_t tp)
{
	if (curr_tile == 0xC) { get_tile(tile_row, tile_col + 1, room); room = curr_room; tp = curr_tilepos; }
	if (!(*attr_word(room, tp) & 0x1F)) add_trob(0xD, 1, tp, room);
}
/* 33FD:00B2 (the prince's hook): feet on a blade tile, past its middle */
void blade_touch(void)
{
	if (Char.charid == 7 || Char.charid == 8 || Char.charid == 0xB || Char.alive >= 0) return;
	if (63 * Char.curr_row + 0x28 > Char.y) return;
	uint8_t t = get_tile(Char.curr_row, Char.curr_col, Char.room);
	if ((t == 0xC || t == 0xD) && col_x_left[Char.curr_col] + 0x12 < Char.x) blade_trigger(curr_room, curr_tilepos);
}
static int blade_closed(uint16_t mod) { int s = mod & 0x1F; return s == 2 || s == 0x10 || s == 0x11; }   /* 33FD:02F8 */
/* 33FD:0264: the blade's box at curr_tile (DS:1774 upper, DS:177C lower, both at 2) */
static int blade_box(int16_t *b)
{
	int s = curr_modifier & 0x1F;
	const int16_t u[4] = {dsw(0x1774), dsw(0x1776), dsw(0x1778), dsw(0x177A)}, l[4] = {dsw(0x177C), dsw(0x177E), dsw(0x1780), dsw(0x1782)};
	if (s == 2) { b[0] = u[0] < l[0] ? u[0] : l[0]; b[1] = u[1] < l[1] ? u[1] : l[1]; b[2] = u[2] > l[2] ? u[2] : l[2]; b[3] = u[3] > l[3] ? u[3] : l[3]; }
	else if (s == 0x10) memcpy(b, u, 8);
	else if (s == 0x11) memcpy(b, l, 8);
	else return 0;
	int16_t dx = (tile_col + (curr_tile == 0xD)) * 32, dy = 63 * tile_row;
	b[0] += dy; b[1] += dx; b[2] += dy; b[3] += dx;
	return 1;
}
/* 33FD:0118 (after a character moves): a closing blade in the column or beside it cuts the prince or a guard */
void blade_hits(void)
{
	if ((Char.charid != 0 && Char.charid != 2) || Char.f19 == 0x2C || GOD_KID) return;   /* (god mode: the blades miss him) */
	for (int8_t c = Char.curr_col - 1; c <= Char.curr_col + 1; c++) {
		uint8_t t = get_tile(Char.curr_row, c, Char.room);
		if (t != 0xC && t != 0xD) continue;
		int16_t b[4], cb[4] = {Char.bbox_top, Char.bbox_left, Char.bbox_bottom, Char.bbox_right};
		if (blade_closed(curr_modifier) && blade_box(b) && boxes_overlap_pub(b, cb) && !(Char.frame >= 0x10B && Char.frame <= 0x10F)) {
			if (Char.charid == 0) { seqtbl_offset_char(0x71); seq_set_85f8(5); }
			else { seqtbl_offset_char(0xBE); Char.alive = 1; }
			play_seq(); take_hp(100); play_sound(0x55);
			curr_room_attrs[curr_tilepos] = (curr_room_attrs[curr_tilepos] & ~0xFFFFu) | (uint16_t)(curr_modifier | 0x40);   /* blood */
			get_tile(Char.curr_row, t == 0xC ? c + 1 : c - 1, Char.room);
			curr_room_attrs[curr_tilepos] = (curr_room_attrs[curr_tilepos] & ~0xFFFFu) | (uint16_t)(curr_modifier | 0x40);
		}
		return;   /* only the first blade tile counts */
	}
}
/* 33FD:0380 (control): is the blade at the character's feet (or the one just behind) running? */
int blade_running_here(void)
{
	uint8_t t = get_tile(Char.curr_row, Char.curr_col, Char.room);
	if (t == 0xC) { int s = curr_modifier & 0x1F; if (s >= 3 && s <= 0xF) return 1; }
	if (t != 0xD) return 0;
	get_tile(Char.curr_row, Char.curr_col - 1, Char.room);
	int s = curr_modifier & 0x1F;
	return s >= 3 && s <= 0xF && col_x_left[Char.curr_col] + 9 >= char_x_left_coll;
}
