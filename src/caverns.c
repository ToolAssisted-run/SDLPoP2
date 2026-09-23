/* Kind-3 (caverns) overlay routines (OVL04, loaded at 33FD): falling rocks. */
#include <stddef.h>
#include "types.h"
#include "globals.h"

/* 33FD:04D0: a trigger releases the rock tile (0x24): animate it and drop a type-2 object from it */
void rock_drop(uint8_t room, int8_t tp)
{
	mob_type saved = cur_mob;
	if (((uint8_t)curr_room_attrs[tp] & 0xF) == 0) {
		add_trob(0x24, 1, tp, room);
		play_sound(0x50);
		cur_mob.w7 = 0x12; cur_mob.speed = -11;
		int16_t x = (cur_trob.tilepos % 10) * 32 + 0x1E;   /* DS:0D26[col] = 32 * col */
		if (x > 0x140) {
			if (room == room_L) { room = drawn_room; x -= 0x140; }
			else cur_mob.speed = -1;
		}
		if (cur_mob.speed != -1) {
			static const int16_t row_y[3] = {66, 129, 192};   /* DS:0D40 */
			cur_mob.x = x; cur_mob.room = room; cur_mob.y = row_y[cur_trob.tilepos / 10] - 0xF;
			cur_mob.type = 2; cur_mob.row = cur_trob.tilepos / 10;
			add_mob();
		}
	}
	cur_mob = saved;
}

/* 33FD:04B6 */
static void rock_redraw(void) { if (anim_visible_pub()) return; /* 1375:0416(0x151E) queues the tile redraw */ cur_trob.state = 1; }
/* 33FD:017C: the rock tile's release animation (attribute low nibble 0..6) */
void anim_rock(void)
{
	int d = (uint8_t)anim_mod & 0xF;
	if (d >= 6) { anim_mod &= ~0xFFu | 0xF0; cur_trob.state = 0xFF; return; }
	anim_mod = (anim_mod & 0xFFFF0000u) | (uint16_t)((uint16_t)anim_mod + 1);
	if (d < 3) rock_redraw();
}

/* 194C:5266: do two {top, left, bottom, right} boxes overlap? */
static int boxes_overlap(const int16_t *a, const int16_t *b)
{
	int16_t l = a[1] > b[1] ? a[1] : b[1], r = a[3] < b[3] ? a[3] : b[3];
	if (l >= r) return 0;
	int16_t t = a[0] > b[0] ? a[0] : b[0], bo = a[2] < b[2] ? a[2] : b[2];
	return t < bo;
}
/* 33FD:0422: a rock hits Char */
static void rock_hits_char(void)
{
	if (Char.alive >= 0) return;
	if (!take_hp(1)) { Char.f24 = 8; play_sound(Char.charid == 0 ? 0x1F : 0x48); }
	else {
		int16_t d = distance_to_edge_weight();
		if (tile_is_empty_kind(get_tile_behind_char()) && d >= 4) {
			Char.x = char_dx_forward(d - 0x28); load_fram_det_col(); Char.curr_row++; seqtbl_offset_char(0x12);
		} else { char_dies_pub(); char_y_to_floor(); Char.fall_y = 0; }
	}
	play_sound(0x51); seq_set_85f8(0);
}
/* 33FD:01F4: flying rocks (objects of type 2) against Char's box, also swept back along their last step */
void rocks_hit_char(void)
{
	if (Char.frame >= 0x6C && Char.frame <= 0x70 && Char.f19 == 0x4F) return;   /* crouching under them */
	mob_type *m;
	for (int n = 1; (m = find_mob_pub(n, 2)) != NULL; n++) {
		if (m->speed == -1) continue;
		int16_t box[4] = {m->y - 2, m->x + 10, m->y, m->x + 14}, cb[4] = {Char.bbox_top, Char.bbox_left, Char.bbox_bottom, Char.bbox_right};
		if (boxes_overlap(cb, box)
		    || (Char.bbox_left <= box[1] && Char.bbox_top - 2 <= box[2] && Char.bbox_bottom - 2 >= box[0]
		        && box[1] - m->w7 < Char.bbox_right && box[2] - m->speed < Char.bbox_bottom)) {
			rock_hits_char(); m->speed = -1;
		}
	}
}
/* 33FD:02AC: a flying rock moves (speed = y step, w7 = x step; -2 = stopped this tick, -1 = gone) */
void rock_fly(void)
{
	if (cur_mob.speed == -2) { cur_mob.speed = -1; return; }
	if (cur_mob.speed == -1) return;
	int16_t y = cur_mob.y + cur_mob.speed, x = cur_mob.x + cur_mob.w7;
	if (x > 0x140) {
		if (cur_mob.room == room_L) { cur_mob.room = drawn_room; x -= 0x140; }
		else { if (level_links(cur_mob.room)[1] == 0) play_sound(0x52); cur_mob.speed = -2; }
	}
	if (cur_mob.speed != -1 && y < 3) {
		if (cur_mob.room == room_B) { cur_mob.room = drawn_room; y += 0xC0; }
		else { if (level_links(cur_mob.room)[2] == 0) play_sound(0x52); cur_mob.speed = -2; }
	}
	if (cur_mob.speed == -2) return;
	if (63 * cur_mob.row + 3 >= y) {   /* into the row above */
		cur_mob.row--;
		if (!tile_is_empty_kind(get_tile(cur_mob.row, x / 32, cur_mob.room))) cur_mob.speed = -2;
	} else if (x / 32 != cur_mob.x / 32) {
		uint8_t t = get_tile(cur_mob.row, x / 32, cur_mob.room);
		if (tile_is_wall_kind(t) || (t == 4 && can_bump_into_gate())) cur_mob.speed = -2;
	}
	if (cur_mob.speed == -2) play_sound(0x52);
	cur_mob.y = y; cur_mob.x = x;
}
