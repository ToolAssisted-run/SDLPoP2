/* Kind-3 (caverns) overlay routines (OVL04, loaded at 33FD): falling rocks. */
#include <stddef.h>
#include <string.h>
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

/* Collapsing floors (tiles 0x17 + 0x18, two columns): each on-screen one owns a 0x65-byte object from the near heap
 * (slots DS:2B6C, max 4) whose byte 0 is the stage (bit 7: collapsing, bit 6: a variant) followed by 10 sub-animations
 * of 10 bytes (active word, counter, frame-list index, three words). The trob's state holds the slot. */
uint16_t floor_ptrs[4];               /* DS:2B6C: heap addresses in the original; here nonzero = in use */
uint8_t floor_objs[4][0x65];
static const uint8_t *cav_ds;         /* static DS data: stage table DS:152E (15 x 8), frame lists via DS:15F0 */
void caverns_set_tables(const uint8_t *ds) { cav_ds = ds; }

/* 33FD:0AB6: start a sub-animation from an 8-byte table entry */
static void floor_add_sub(uint8_t *obj, uint16_t entry)
{
	const uint8_t *e = cav_ds + entry; int i;
	for (i = 0; i < 10 && (obj[1 + i * 10] | obj[2 + i * 10]); i++) ;
	if (i >= 10) return;
	uint8_t *s = obj + i * 10;
	s[1] = 1; s[2] = 0; s[3] = 0; s[4] = e[1]; memcpy(s + 5, e + 2, 6);
}
/* 33FD:097C: reset an object to its type's starting sub-animations */
static void floor_init(uint8_t *obj, int type)
{
	memset(obj, 0, 0x65);
	if (type == 1) {
		obj[0] = 3; floor_add_sub(obj, 0x1556); floor_add_sub(obj, 0x154E); obj[0xD] = 1; floor_add_sub(obj, 0x1546); obj[0xD] = 1;
		floor_add_sub(obj, 0x153E); obj[0x21] = 3; floor_add_sub(obj, 0x1536); obj[0x21] = 3; floor_add_sub(obj, 0x152E); obj[0x21] = 3;
	} else if (type == 2) {
		obj[0] = 0xB; floor_add_sub(obj, 0x157E); floor_add_sub(obj, 0x1576); obj[0xD] = 4; floor_add_sub(obj, 0x156E); obj[0x17] = 5;
	} else {
		obj[0] = 0; floor_add_sub(obj, 0x152E); floor_add_sub(obj, 0x1536); floor_add_sub(obj, 0x153E); floor_add_sub(obj, 0x1596); obj[0x21] = 5;
		floor_add_sub(obj, 0x159E); obj[0x2B] = 4;
	}
}
/* 33FD:0804: a free slot (the original stops with an error when all 4 are taken and uses slot 3) */
static int floor_slot(void) { int i; for (i = 0; i < 4 && floor_ptrs[i]; i++) ; if (i == 4) { note_missing("FLOOR_SLOTS"); i = 3; } return i; }
/* 33FD:0A54: a collapsing floor comes on screen */
void floor_room_entry(uint8_t room, int8_t tp)
{
	if (get_trob(tp, room)) return;
	int slot = floor_slot();
	floor_ptrs[slot] = 1;   /* malloc(0x65) (2812:003B) */
	floor_init(floor_objs[slot], tp % 3);
	add_trob(0x17, slot, tp, room);
}
/* 33FD:08C4: free every object */
void floor_free_all(void) { for (int i = 0; i < 4; i++) floor_ptrs[i] = 0; }
/* 33FD:05AE: animation of tile 0x17 */
void anim_floor(void)
{
	int slot = cur_trob.state & 7;
	uint8_t *obj = floor_objs[slot];
	if (!anim_visible_pub()) { floor_ptrs[slot] = 0; return; }   /* free (2812:001A) */
	int st = obj[0] & 0x3F;
	if (st == 0x17 || ((obj[0] & 0x80) && st == 3)) {
		int v = obj[0] & 0x40; floor_init(obj, 0);
		if (st == 3) obj[0] = v ? 0xC4 : 0x84;
		return;   /* 33FD:08F4 redraws */
	}
	obj[0]++;
	if ((obj[0] & 0x80) && st < 3) return;
	if (obj[0] & 0x80) { if (st == 4) { obj[0] &= 0x7F; play_sound(0x61); } st -= 3; }
	for (int i = 0; i < 10; i++) {
		uint8_t *s = obj + 1 + i * 10;
		if (!(s[0] | s[1])) continue;
		s[2]++;
		const uint8_t *list = cav_ds + (cav_ds[0x15F0 + 2 * s[3]] | cav_ds[0x15F1 + 2 * s[3]] << 8);
		uint8_t f = list[(int8_t)s[2]];
		if (f == 0xFF) { s[0] = s[1] = 0; }
		else if (f == 0xFE) { s[8] = 1; s[9] = 0; s[2]++; }
	}
	for (int i = 0; i < 15; i++) {
		uint16_t e = 0x152E + i * 8;
		if ((int8_t)cav_ds[e] > st) break;
		if ((int8_t)cav_ds[e] == st) floor_add_sub(obj, e);
	}
}
/* 33FD:0878: the trob of the collapsing floor under (row, col, room) (0x18 is its right half) */
static trob_type *floor_trob(int8_t row, int8_t col, uint8_t room)
{
	uint8_t t = get_tile(row, col, room); int8_t tp = curr_tilepos;
	if (t == 0x18) tp--;
	trob_type *r = get_trob(tp, room);
	get_room_address(drawn_room);
	return r;
}
/* 33FD:0754: Char steps on a collapsing floor and goes down with it */
static void floor_collapse(void)
{
	trob_type *t = floor_trob(Char.curr_row, Char.curr_col, Char.room);
	if (!t) return;
	floor_objs[t->state][0] = 0x80;
	if (Char.charid == 0 && random_2751(3) == 0) { seqtbl_offset_char(0x74); floor_objs[t->state][0] |= 0x40; play_sound(0x63); }
	else { play_sound(Char.charid == 0 ? 0x63 : 0x62); seqtbl_offset_char(0x73); }
	Char.f23 = 0; Char.f0f = 1; Char.f24 = 5; Char.fall_x = Char.fall_y = 0;
	take_hp(100); play_seq();
	if ((t = floor_trob(Char.curr_row, Char.curr_col, Char.room)) != NULL) cur_trob = *t;   /* 33FD:08F4 redraws it */
}
/* 33FD:06E6 (kind 3, after a character moves): standing on a collapsing floor's surface */
void floor_touch_check(void)
{
	uint8_t t = get_tile(Char.curr_row, Char.curr_col, Char.room);
	int16_t surface = 63 * Char.curr_row + 0x29;
	int16_t d = col_x_left[Char.curr_col] - dx_weight() + 0x1A;
	if ((t == 0x17 || (t == 0x18 && d >= 0)) && (frame_flags & 0x40) && surface < Char.y && Char.f24 != 5) floor_collapse();
}
/* 33FD:0B0E (kind 3, a gate opening, si = its position): a gate rising under the prince in a squeeze */
int gate_squeeze(int si)
{
	int8_t tp = (Kid.curr_row >= 0 ? Kid.curr_row * 10 : Kid.curr_row * 10 + 9) + Kid.curr_col;   /* 0AFF:07D4 */
	if (cur_trob.room != Kid.room) { if (cur_trob.room == room_L) tp += 10; else if (cur_trob.room == room_R) tp -= 10; else tp = 0x1E; }
	if (cur_trob.tilepos != tp && cur_trob.tilepos != tp - 1) return si;
	if (Kid.f24 == 3) { si = 0xC; play_sound(9); }   /* 1375:2546 */
	else if (Kid.frame == 0x108) si -= door_speed_0776(cur_trob.state) + 2;
	else if (Kid.frame == 0x109) si = 0x24;
	else if (Kid.frame == 0x10A) cur_trob.state = 0xFF;   /* 1375:0388 redraws */
	return si;
}
