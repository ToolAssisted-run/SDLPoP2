/* Wall blade trap, tile 2 on level kind 3 (segment 186A). The tile's modifier low byte is the blade position:
 * 0 idle, 1..4 thrusting out, 0x8F..0x80 pulling back, 6..8 resetting, 0xF0 stuck out after catching someone.
 * While it moves, a falling-object entry of type 4 carries it (1375:1B10 dispatches to 186A:048C). */
#include "types.h"
#include <stddef.h>
#include "globals.h"

/* 0AFF:1078: tile_col in drawn-room coordinates */
int8_t tile_col_in_drawn_room(void)
{
	int8_t c = tile_col; uint8_t r = curr_room;
	if (r == drawn_room || r == room_B || r == room_A) return c;
	if (r == room_L || r == room_BL || r == room_AL) return c - 10;
	if (r == room_R || r == room_BR || r == room_AR) return c + 10;
	return c;
}
/* 186A:045E: 0 idle/stuck, 1 pulling back, 2 thrusting */
static int trap_state(void)
{
	uint8_t v = (uint8_t)curr_modifier;
	if (v == 0 || v == 0xF0) return 0;
	if (v & 0x80) return 1;
	return v < 5 ? 2 : 0;
}
/* 186A:0418: the blade object of a row */
static mob_type *find_trap(int8_t row, uint8_t room)
{
	for (int n = 1; ; n++) { mob_type *m = find_mob_pub(n, 4); if (!m) return NULL; if (m->room == room && m->row == row) return m; }
}
static uint16_t *attr_at(uint8_t room, int8_t tp) { return (uint16_t *)((uint8_t *)&level + 0x348 + (room * 30 + tp) * 4); }
static void new_blade(int8_t tp, uint8_t room, int16_t wd)
{
	cur_mob.x = (tp % 10) * 32 + 0x2E; cur_mob.y = (tp / 10) * 63 + 0x2F; cur_mob.row = tp / 10; cur_mob.room = room;
	cur_mob.w7 = 0; cur_mob.speed = 0; cur_mob.type = 4; cur_mob.wd = wd;
	add_mob_pub();
}
/* 186A:0226: on room entry a stuck-out blade gets its object back */
void trap_room_entry(int8_t tp, uint8_t room)
{
	if (find_trap(tp / 10, room)) return;
	uint16_t v = *attr_at(room, tp);
	if ((uint8_t)v == 0xF0) new_blade(tp, room, v);
}
/* 186A:0532: set the blade off */
static void trap_trigger(int8_t tp, uint8_t room)
{
	uint16_t *a = &((uint16_t *)curr_room_attrs)[tp * 2]; uint8_t dl = (uint8_t)*a;
	if (dl != 0 && !(dl & 0x80)) return;
	if (dl == 0) { (*a)++; new_blade(tp, room, *a); play_sound(0x4E); return; }
	if (dl == 0xF0) return;
	uint16_t v = (curr_modifier & 0x800) + 0x8F; *a = v;
	mob_type *m = find_trap(Char.curr_row, Char.room); if (m) m->wd = v;
}
/* 186A:02AA: the blade catches Char */
void trap_kill_pub(void);
static void trap_kill(void)
{
	if (GOD_KID) return;   /* (god mode: the blade misses him) */
	uint16_t *a = attr_at(curr_room, curr_tilepos);
	*a = Char.charid != 4 ? (uint16_t)(((curr_modifier >> 8) & 8) << 8 | 0xF0) : 0x8F;
	mob_type *m = find_mob_pub(1, 4); if (m && m->room == curr_room && m->row == tile_row) m->wd = *a;
	char_y_to_floor(); Char.fall_y = 0;
	if (Char.charid == 0 || Char.charid == 2) {
		Char.f0f = 1; Char.x = col_x_right[tile_col_in_drawn_room()] + 0x1B;
		int seq;
		if (Char.direction == 0) { seq = (uint16_t)((Char.charid == 0 ? 0xFF79 : 0) + 0xBD); Char.direction = -1; } else seq = (uint16_t)((Char.charid == 0 ? 0xFF77 : 0) + 0xBC);   /* 16-bit: prince 0x36 / 0x33, guard 0xBD / 0xBC */
		seqtbl_offset_char(seq); play_sound(0x4D); take_hp(100);
	} else if (Char.charid == 4) skel_blade_hit();
	else return;
	play_seq();
}
/* 186A:009C: after moving: a thrusting blade in this or the previous column catches Char */
void trap_catch_check(void)
{
	if (Char.f19 == 0x55 || Char.charid == 4 || Char.charid == 1 || is_dead_frame(Char.frame)) return;
	for (int8_t c = Char.curr_col - 1; c <= Char.curr_col; c++) {
		if (get_tile(Char.curr_row, c, Char.room) != 2 || trap_state() != 2) continue;
		int16_t edge = col_x_right[c] + 14 + (Char.f10 == 0 ? 14 : 10);
		int hit = 0;
		if (edge > char_x_left && 63 * Char.curr_row + 0x1A <= Char.y && !((Char.frame >= 0x79 && Char.frame <= 0x84) || Char.frame == 0xF)) hit = 1;
		else if (level_kind == 2 && ovl_347c_a0e()) hit = 1;
		if (!hit) continue;
		trap_kill();
		mob_type *m = find_trap(Char.curr_row, Char.room);
		if (m) { m->wd = (m->wd & 0xFF00) | 0xF0; *attr_at(curr_room, curr_tilepos) = m->wd; }
	}
}
/* 186A:01AA: walking up against a blade tile on the left sets it off */
void trap_touch_check(void)
{
	if (Char.curr_col < 0 || Char.charid == 4) return;
	if (get_tile(Char.curr_row, Char.curr_col - 1, Char.room) != 2) return;
	if (63 * Char.curr_row + 0x12 > Char.y) return;
	int16_t edge = col_x_right[tile_col_in_drawn_room()] + 14 + (Char.f10 == 0 ? 0x10 : 2);
	if (Char.direction == 0) edge += image_width - sword_extra_width();
	if (edge < Char.x) return;
	trap_trigger(curr_tilepos, curr_room);
}
/* 186A:048C: the blade object moves */
void trap_update(void)
{
	get_tile(cur_mob.row, mob_col_pub() - 1, cur_mob.room);
	uint16_t v = (uint8_t)curr_modifier;
	if (v == 0xF0) return;
	if (v & 0x80) { v = (v & 0xFF00) | (uint8_t)(v - 1); if (!(v & 0x7F)) { v = 6; play_sound(0x4F); } }
	else { v++; if (v == 5) v = 0x8F; else if (v == 8) { v = 0; cur_mob.speed = -1; } }
	get_room_address(curr_room);
	v += curr_modifier & 0x800;
	((uint16_t *)curr_room_attrs)[curr_tilepos * 2] = v;
	cur_mob.wd = v;
}
void trap_kill_pub(void) { trap_kill(); }
