/* Rooms and the characters that live in them (OVL01 segment 2D3E, 0823:0E72, 0FB3:0026).
 * Every level room carries up to five 23-byte character records (level+0x1867). Only the drawn room's
 * characters are live, in chars[]; when the prince changes room the old room's characters are written back
 * (or follow him), the neighbours' records near the shared edge are pulled in, and chars[] is rebuilt. */
#include <string.h>
#include "types.h"
#include "globals.h"

uint8_t next_room;          /* DS:6B6D (94ad): room the prince moved to this tick */
int16_t exit_dir;           /* DS:68F2 (9232): 0 left, 1 right, 2 up, 3 down, -1 none */
uint16_t word_922a;         /* DS:68EA countdown */
uint8_t pal_slots[2];       /* DS:5D08 / 5D09: guard palette per slot */
uint16_t word_32d8;         /* DS:0998, compared with DS:5CEC */
uint8_t byte_9276;          /* DS:6936 (level kind 5: index of a character that may fall out) */
const uint8_t *type_to_charid, *charid_to_type;   /* DS:0096 / DS:00A2 */

static uint8_t *room_base(uint8_t room) { return (uint8_t *)&level + 0x17F3 + room * 0x74; }   /* DS:43AB + room*0x74 */
uint8_t room_nchars(uint8_t room) { return room_base(room)[0]; }
/* OVL01 02DC8C */
level_char_init *room_char_record(int i, uint8_t room) { return i < (int8_t)room_base(room)[0] ? (level_char_init *)(room_base(room) + 1 + i * 23) : NULL; }
/* OVL01 02DC44 */
static void add_record(level_char_init *rec, uint8_t room)
{
	int8_t n = room_base(room)[0];
	if (n >= 5) return;
	rec->index = n; room_base(room)[0]++;
	memcpy(room_base(room) + 1 + n * 23, rec, 23);
}
/* OVL01 02DB96: remove record i, shifting the rest down (and chars[] when it is the drawn room) */
static void remove_record(int i, uint8_t room)
{
	int8_t n = room_base(room)[0];
	if (i >= n) return;
	for (; i + 1 < n; i++) {
		memcpy(room_char_record(i, room), room_char_record(i + 1, room), 23);
		room_char_record(i, room)->index = i;
		if (room == drawn_room) { chars[i] = chars[i + 1]; chars[i].index = i; }
	}
	room_base(room)[0]--;
}
void remove_record_pub(int i, uint8_t room) { remove_record(i, room); }
/* OVL01 02DB1A */
static void record_fixup(level_char_init *rec)
{
	if (Char.charid == 2) {
		if (Char.f19 == 100 || Char.f19 == 0xBA) rec->y = Char.y;
		if (level.type == 7) rec->type = 7;
	} else {
		if (Char.charid == 0xB) ovl_37d2a();
		if (Char.charid == 6) ovl_352b4();
	}
}
/* 0AFF:0220: tile position of (row, col), 30 outside the grid; row -1 counts down from -1 */
static int8_t tilepos_or_30(int8_t row, int8_t col)
{
	if (row < 3 && row > -2 && col < 10 && col >= 0) return row == -1 ? -1 - col : row * 10 + col;
	return 30;
}
/* 0AFF:07D4 */
static int8_t row_base(int8_t row) { return row >= 0 ? row * 10 : row * 10 + 9; }

/* OVL01 02E71A: the room a character's out-of-grid position falls in (0 = outside the level); moves Char's coordinates */
static uint8_t room_of_char(void)
{
	uint8_t r = Char.room;
	if (Char.curr_col >= 0 && Char.curr_col < 10 && Char.curr_row >= 0 && Char.curr_row < 3) return r;
	for (;;) {
		uint8_t prev = r;
		if (Char.curr_row < 0) { r = level_links(r)[2]; if (r) { Char.y += 0xC0; Char.curr_row += 3; } else { r = prev; prev = 0; } }
		else if (Char.curr_row >= 3) { r = level_links(r)[3]; if (r) { Char.y -= 0xC0; Char.curr_row -= 3; } else { r = prev; prev = 0; } }
		if (Char.curr_col < 0) { r = level_links(r)[0]; if (r) { Char.x += 320; Char.curr_col += 10; } }
		else if (Char.curr_col >= 10) { r = level_links(r)[1]; if (r) { Char.x -= 320; Char.curr_col -= 10; } }
		else if (prev == 0) r = 0;
		if (r == 0) return 0;
		if (Char.curr_col >= 0 && Char.curr_col < 10 && Char.curr_row >= 0 && Char.curr_row < 3) return r;
	}
}
/* OVL01 02E6D4 */
static int char_in_kid_room(void) { char_type saved = Char; uint8_t r = room_of_char(); Char = saved; return r == Kid.room; }
/* OVL01 02E682: may this character follow the prince into the next room? */
static int can_follow(void) { return (int8_t)room_nchars(Kid.room) < 5 && (Char.f10 == 1 || Char.charid == 7 || Char.charid == 8) && Char.charid != 10; }

/* OVL01 02E834: step Char into the neighbouring room */
void change_room(int dir)
{
	Char.room = level_links(Char.room)[dir];
	if (dir == 0) { Char.x += 320; Char.curr_col = x_to_col(Char.x); }
	else if (dir == 1) { Char.x -= 320; Char.curr_col = x_to_col(Char.x); }
	else if (dir == 2) { Char.y += 0xBD; Char.curr_row = y_to_row(Char.y); }
	else { Char.y -= 0xBD; Char.curr_row = y_to_row(Char.y); }
}

/* OVL01 02D8AC: write a character that stays behind back into its record (1 = the record was removed) */
static int save_to_record(void)
{
	int r = 0;
	if ((uint8_t)Char.direction == 0x56 || Char.f24 == 5 || Char.f24 == 4 || Char.f19 == 0xC4) { remove_record(Char.index, Char.room); return 1; }
	level_char_init *rec = room_char_record(Char.index, Char.room);
	if (rec) {
		int8_t tp = tilepos_or_30(Char.curr_row, Char.curr_col);
		rec->tilepos = tp;
		if (tp == 30) {
			uint8_t nr = room_of_char();
			if (nr == 0 || Char.charid == 0xC) {
				if (Char.action == 9 && level_kind == 5) { sound_194c_83d2(0x2729); if (drawn_room != 0x13 && drawn_room != 0x10) play_sound(0); }
			} else {
				level_char_init t = *rec;
				t.tilepos = row_base(Char.curr_row); t.x = Char.x; t.direction = Char.direction; t.seq_id = Char.seq_id; t.seq_pos = Char.seq_pos;
				t.f10 = Char.f10; t.hp = Char.f12; t.f38 = Char.f38; t.opp_index = Char.opp_index;
				add_record(&t, nr);
			}
			remove_record(Char.index, Char.room); r = 1;
			if (room_nchars(drawn_room) == 0) chars[Char.index].direction = 0x56;
		} else {
			rec->x = Char.x; rec->direction = Char.direction;
			if (Char.alive < 0) rec->seq_pos = 0; else { rec->seq_pos = Char.seq_pos; rec->seq_id = Char.seq_id; }
			rec->f10 = Char.f10; rec->hp = Char.f12; rec->type = charid_to_type[Char.charid];
			record_fixup(rec);
			uint8_t t = get_tile_at_char();
			if (t == 5 || t == 6) press_button_hold();
		}
	}
	Char.f12 = 0;
	return r;
}
/* OVL01 02DA80: move a character's record into the room the prince went to */
static void follow_kid(void)
{
	level_char_init t = *room_char_record(Char.index, Char.room);
	remove_record(Char.index, Char.room);
	change_room(exit_dir);
	t.tilepos = row_base(Char.curr_row); t.x = Char.x; t.direction = Char.direction; t.seq_id = Char.seq_id; t.seq_pos = Char.seq_pos;
	t.f10 = Char.f10; t.hp = Char.f12;
	t.pal = Char.pal_slot == 4 ? pal_slots[0] : pal_slots[1];
	record_fixup(&t); add_record(&t, Char.room);
}
/* OVL01 02E532: the drawn room's characters when the prince leaves it */
static void chars_on_leave(void)
{
	int8_t n = room_nchars(drawn_room);
	for (int8_t i = 0; i < n; i++) {
		load_char(i);
		if ((uint8_t)Char.direction == 0x56) continue;
		int follow;
		if ((((Char.frame > 0xC9 && Char.frame < 0xD5) && Char.f38 == 1) || ((Char.frame > 0xB9 && Char.frame < 0xCA) && Char.f38 != 0)) && (Char.charid == 2 || Char.charid == 4)) follow = 1;
		else if (char_in_kid_room()) follow = 1;
		else if (Char.curr_row != 0 && level_number == 5 && (Char.room == 10 || Char.room == 7 || Char.room == 12)) follow = 1;
		else if (Char.charid == 0) follow = 1;
		else if (Char.charid == 6 && (Char.room == 7 || Char.room == 8) && level_kind == 6 && (next_room == 7 || next_room == 8)) follow = 1;
		else if (Char.alive < 0 && can_follow()) {
			if (exit_dir == 0) follow = Char.x < 0xD0;
			else if (exit_dir == 1) follow = Char.x >= 0x173;
			else if (exit_dir == 2) follow = Char.curr_row < 0;
			else follow = Char.curr_row >= 3;
		} else follow = 0;
		int removed;
		if (!follow) removed = save_to_record(); else { follow_kid(); removed = 1; }
		if (removed) { i--; n--; }
	}
}

/* OVL01 02E8BE: has the prince left the drawn room? returns the exit direction and moves him */
static int kid_exit_dir(void)
{
	int d;
	const uint8_t *lk = level_links(Char.room);
	if ((drawn_room == 7 || drawn_room == 8) && level_kind == 6 && (chars[0].f19 == 0xF3 || chars[1].f19 == 0xF3)) d = ovl_342b4();
	else if (word_32d8 == counter_5cec && Char.f19 != 0x46 && (Char.charid != 1 || Char.frame != 0xB9)) {
		if (Char.action == 5 || Char.action == 4 || Char.action == 3 || Char.y > 9 || Char.y < -16) {
			if (Char.y < 0xE7 || (lk[3] == 0 && Char.y < image_height + 0xE7) || (level_kind == 5 && Char.index == byte_9276)) {
				uint16_t f = Char.frame;
				if ((f < 0x87 || f > 0x95) && (f < 0x6E || f > 0x77) && (f < 0x96 || f > 0xA2) && (f < 0xA6 || f > 0xA8) && Char.action != 7) {
					if (Char.direction == -1) {
						if (char_x_left_coll < 0x7E && (lk[0] != 0 || char_x_left_coll <= 0x7D - image_width)) {
							if (level_number == 9 && Char.room == 8 && Char.curr_row == 0 && Char.frame == 0x28) { counter_5cec++; d = -1; } else d = 0;
						} else {
							d = -1;
							if (char_x_left_coll > 0x1C1 && (lk[1] != 0 || image_width + 0x1C1 <= char_x_left_coll) && (level_kind != 1 || Char.room != 3)) d = 1;
						}
					} else {
						d = -1; get_tile(Char.curr_row, 9, Char.room);
						if (char_x_right_coll < 0x1C6 || (lk[1] == 0 && char_x_right_coll < image_width + 0x1C6) || (level_kind == 1 && Char.room == 3)) {
							if (char_x_right_coll < 0x82 && (lk[0] != 0 || char_x_right_coll <= 0x82 - image_width)) d = 0;
						} else d = 1;
					}
				} else d = -1;
			} else d = 3;
		} else d = (level_number == 5 && Char.room == 3) ? -1 : 2;
	} else d = -1;
	if (d == 0) { if (Char.room == 1 && level_number == 2) d = -1; }
	else if (d == 1) {
		if (level_kind == 5 && Char.room == 0xF) { d = -1; if (Char.f19 != 0x47) ovl_34210(); }
		else if (Char.room == 3 && level_kind == 6) d = -1;
	} else if (d == 3) {
		uint8_t r = Char.room;
		if ((r == 0xB && level_kind == 5) || (r == 3 && level_kind == 6) || (r == 5 && level_kind == 6) || (level_number == 5 && r == 10)
		    || (r == 4 && level_number == 13) || (level_number == 13 && r == 13)
		    || (((r == 0x10 && level_number == 9) || (r == 0x1B && level_number == 6)) && (Char.f19 == 0x44 || Char.f19 == 0x19 || Char.f19 == 10))) {
			d = -1;
			if ((level_number == 5 && r == 10) || (r == 4 && level_number == 13) || (level_number == 13 && r == 13)) { take_hp(100); Char.room = 0; }
		}
	}
	if (d != -1) change_room(d);
	if (d == 0 && drawn_room == 0x1B && level_number == 6) ovl_34958();
	return d;
}

/* OVL01 02E46A (2D3E:108A): after everyone moved: did the prince leave the room? */
void check_kid_left_room(void)
{
	if (word_922a) { word_922a--; return; }
	loadkid(); load_frame_to_obj(); set_char_collision();
	exit_dir = kid_exit_dir();
	if (exit_dir != -1 && Char.room != 0) { Kid = Char; next_room = Char.room; chars_on_leave(); return; }
	if (Char.room == 0 && Char.alive < 0) {
		if (level_kind == 5 && exit_dir == 0) { Char.x = char_dx_forward(320); ovl_34370(); }
		else {
			fall_scream_1611_0030();
			if (level_kind != 5) { take_hp(100); Char.frame = 0xB9; seq_set_85f8(3); Kid = Char; apply_hp_deltas(); loadkid(); }
		}
	}
	Kid = Char;
}

/* 0823:1008: apply the pending hp changes of the prince and the drawn room's characters */
void apply_hp_deltas(void)
{
	if (Kid.hp_delta != 0) {
		if (Kid.charid == 1 && level_kind == 6) ovl_2f9f2();
		int v = (int8_t)Kid.f12 + Kid.hp_delta; if (v < 0) v = 0; if (v > (int8_t)Kid.f13) v = (int8_t)Kid.f13; Kid.f12 = (uint8_t)v;
	}
	for (int8_t i = 0; i < (int8_t)room_nchars(drawn_room); i++) {
		char_type *c = &chars[i];
		if (c->hp_delta != 0) { int v = (int8_t)c->f12 + c->hp_delta; if (v < 0) v = 0; if (v > (int8_t)c->f13) v = (int8_t)c->f13; c->f12 = (uint8_t)v; }
	}
}

/* 0FB3:0026: neighbours of the drawn room */
void set_neighbour_rooms(void)
{
	room_AL = room_AR = room_BL = room_BR = 0;
	if (drawn_room == 0) { room_L = room_R = room_A = room_B = 0; return; }
	get_room_address(drawn_room);
	const uint8_t *lk = level_links(drawn_room);
	room_L = lk[0]; room_R = lk[1]; room_A = lk[2]; room_B = lk[3];
	if (room_A == 0) { if (room_L) room_AL = level_links(room_L)[2]; if (room_R) room_AR = level_links(room_R)[2]; }
	else { room_AL = level_links(room_A)[0]; room_AR = level_links(room_A)[1]; }
	if (room_B == 0) { if (room_L) room_BL = level_links(room_L)[3]; if (room_R) room_BR = level_links(room_R)[3]; }
	else { room_BL = level_links(room_B)[0]; room_BR = level_links(room_B)[1]; }
}

/* OVL01 02D7BA: characters standing near the shared edge of the side rooms join the new room */
static void pull_neighbour_chars(void)
{
	for (int side = 0; side < 2; side++) {
		uint8_t room = side == 0 ? room_L : room_R; int16_t dx = side == 0 ? -320 : 320;
		if (room == 0) continue;
		int8_t n = room_nchars(room);
		for (int8_t i = 0; i < n; i++) {
			level_char_init *rec = room_char_record(i, room);
			if ((side == 0 && rec->x > 0x16F) || (side == 1 && rec->x < 0xD0) || ((room == 7 || room == 8) && level_kind == 6 && rec->type == 3)) {
				level_char_init t = *rec; t.x += dx;
				add_record(&t, drawn_room); remove_record(i, room); i--; n--;
			}
		}
	}
}
/* OVL01 02D422 / 02D3E8 */
static void set_max_hp(const level_char_init *rec) { Char.f13 = rec->max_hp ? rec->max_hp : 3; }
static void init_hp(const level_char_init *rec)
{
	set_max_hp(rec);
	if ((Char.charid == 4 && Char.f19 == 0x77) || (Char.f12 = Char.f13, Char.charid == 10 && Char.f19 == 0x69)) Char.f12 = 0;
	Char.hp_delta = Char.f12;
}
/* OVL01 02E28C (2D3E:0EAC; called only on level types 0/5/6): palette slot for the record's palette byte */
static void pick_pal_slot(uint8_t type)
{
	Char.pal_slot = 0;
	if (type == 0) type = 1;
	for (int i = 0; i < 2 && Char.pal_slot == 0; i++) if (pal_slots[i] == type) Char.pal_slot = 4 << i;
	for (int i = 0; i < 2 && Char.pal_slot == 0; i++) if (pal_slots[i] == 0) { pal_slots[i] = type; Char.pal_slot = 4 << i; }
	if (Char.pal_slot == 0) Char.pal_slot = 4;
}
/* OVL01 02D444: build chars[] from the drawn room's records */
void enter_room_chars(void)
{
	pull_neighbour_chars();
	int8_t n = room_nchars(drawn_room);
	for (int8_t i = 0; i < n; i++) {
		level_char_init *rec = room_char_record(i, drawn_room);
		uint8_t lt = level.type;
		if (lt != 5 && lt != 6 && rec->type != 1 && rec->type != 9 && rec->type != 10 && rec->type != 3) rec->type = lt;
		Char.charid = type_to_charid[rec->type];
		load_guard_sprites(rec->type);
		Char.index = i; Char.room = drawn_room;
		Char.curr_row = rec->tilepos / 10; if (rec->tilepos < 0) Char.curr_row--;
		if (Char.charid == 2 && rec->seq_id == 100) Char.y = rec->y;
		else if (Char.charid == 6 && rec->w15 != 0) { Char.y = rec->w15; if (rec->seq_id == 0xC) { Char.fall_x = 2; Char.fall_y = 0x12; } }
		else char_y_to_floor();
		Char.x = rec->x; Char.curr_col = x_to_col(Char.x); Char.direction = rec->direction;
		Char.f38 = rec->f38; Char.opp_index = rec->opp_index;
		Char.f2a = 0; Char.f26 = 0; Char.f28 = 0; Char.f24 = 0; Char.f23 = 0; Char.f3a = 0;
		if (Char.charid == 0) Char.pal_slot = 2;
		else if (Char.charid == 6) { Char.pal_slot = 4; ovl_guard6_sprites(); if (drawn_room == 6 && level_kind == 6) Char.direction = random_2751(1) - 1; }
		else if (lt == 0 || lt == 5 || lt == 6) pick_pal_slot(rec->pal);
		else Char.pal_slot = 4;
		if (rec->seq_pos == 0) {
			if (Char.charid == 4) rec = skel_room_entry(rec);
			else if (Char.charid == 7 || Char.charid == 8 || Char.charid == 0xB) rec = ovl_36ada(rec);
			else if (Char.charid == 10) {
				if (!tile_is_floor(get_tile_at_char())) { rec->seq_id = 0x6A; Char.f10 = 1; Char.f23 = 3; Char.direction = Char.x < Kid.x ? 0 : -1; }
				else { rec->seq_id = 0x69; Char.f10 = 0; }
			} else if (Char.charid != 6) { if (rec->seq_id != 0x16) rec->seq_id = 0x4D; Char.f10 = 0; }
			else rec->seq_id = 2;
			seqtbl_offset_char(rec->seq_id); init_hp(rec);
		} else {
			Char.f0f = 1; Char.seq_id = rec->seq_id; Char.seq_pos = rec->seq_pos; Char.f19 = rec->seq_id;
			Char.f10 = rec->f10; Char.f12 = rec->hp; Char.hp_delta = Char.f12;
			if (Char.charid != 0) set_max_hp(rec);
		}
		play_seq();
		if (!is_dead_frame(Char.frame) || Char.charid == 0) {
			Char.alive = -1; word_6140 = 0; word_68f0 = 0; word_922e = 0;
			if (Char.charid == 0xB && Char.frame == 0x108) Char.f24 = 6;
		} else {
			if (Char.charid == 2) ovl_36712();
			Char.alive = 7; Char.f0f = 0; Char.f12 = 0; Char.hp_delta = -(int8_t)Char.f13;
			if (Char.charid == 4 || Char.charid == 10) set_revive_timer(0, Char.index);
		}
		if (Char.charid == 6 && Char.f19 == 0xC) Char.action = 4;
		else { Char.fall_x = Char.fall_y = 0; Char.action = (Char.charid == 7 || Char.charid == 8) ? 0 : 1; }
		save_char();
	}
	if (n == 0) room_music_087e();
}

/* 0823:0E72: switch the drawn room to where the prince went */
void switch_room(void)
{
	if (next_room == 0 || next_room == drawn_room) return;
	drawn_room = next_room;
	set_neighbour_rooms();
	redraw_room();                   /* 0FB3:29B8, 0CD6:02BE, 1286:0AB2 sprites */
	loadkid();
	start_room_anims();              /* 0823:0B78 */
	pal_slots[0] = pal_slots[1] = 0;
	enter_room_chars();
	if (room_nchars(drawn_room) == 0) hp_bar_clear();
	else Kid.opp_index = find_opponent(1);
}

/* ---- guard spawn points: level+0x26D7 + room*0x22 (DS:528F): count, room skill, then 10-byte entries at +4:
 * +1 max guards already out on that side, +2 row, +3 col, +4 countdown, +5 reload, +6 -> Char+0x38, +7 other row,
 * +8 guards left, +9 hp (low nibble; 6+ = comes in with the sword drawn when the prince is within 3 columns) */
static uint8_t *spawn_block(uint8_t room) { return (uint8_t *)&level + 0x26D7 + room * 0x22; }
/* 2D3E:0E14 / 0CB6 / 0C66 */
static uint8_t *spawn_entry(int8_t i, uint8_t room) { return (room != 0 && i < (int8_t)spawn_block(room)[0]) ? spawn_block(room) + 4 + i * 10 : NULL; }
static int spawn_matches(const uint8_t *sp, int8_t row) { int d = (int8_t)sp[3] - Kid.curr_col; if (d < 0) d = -d; return ((int8_t)sp[2] == row || (int8_t)sp[7] == row) && d > 2 && sp[8] != 0; }
static uint8_t *find_spawn(int8_t row, uint8_t room)
{
	int8_t n = spawn_block(room)[0];
	for (int8_t i = 0; i < n; i++) { uint8_t *sp = spawn_entry(i, room); if (spawn_matches(sp, row)) return sp; }
	return NULL;
}
/* 1375:14C2: first wall column from col in direction dir (stops past the room edge) */
static int8_t scan_to_wall(int8_t dir, int8_t row, int8_t col, uint8_t room) { do col += dir; while (!tile_is_wall_kind(get_tile(row, col, room)) && col >= 0 && col <= 10); return col; }
/* 2D3E:0CF8 */
static int spawn_ok(uint8_t room, const uint8_t *sp)
{
	int8_t n = room_nchars(room);
	if (n >= 5 || (int8_t)word_32d8 != (int16_t)counter_5cec || tick % 3 == 0) return 0;
	int ok = 1, far = 0; int8_t sc = sp[3], kc = Kid.curr_col;
	int8_t wall = scan_to_wall(sc < 5 ? 1 : -1, sp[2], sc, room);
	for (int8_t i = 0; ok && i < n; i++) {
		const char_type *c = &chars[i];
		if (c->alive >= 0) continue;
		if (!((sc < kc && c->x < Kid.x) || (sc > kc && c->x > Kid.x))) continue;
		if (!((sc < wall && kc < wall) || (sc > wall && kc > wall))) continue;
		far++;
		if (sp[1] <= far) ok = 0;
		else if ((sc < kc && c->curr_col <= sc) || (sc > kc && c->curr_col >= sc)) ok = 0;
	}
	return ok;
}
/* 2D3E:0AA0: a guard walks in from the spawn point */
static void spawn_guard(uint8_t room, uint8_t *sp)
{
	int8_t n = room_nchars(room);
	if (n >= 5) return;
	room_base(room)[0]++;
	level_char_init *rec = room_char_record(n, room);
	int8_t col = sp[3];
	rec->tilepos = row_base(sp[2]) + col;
	if (col <= 5) { rec->x = col_x_left[col - 2]; rec->direction = 0; } else { rec->x = col_x_left[col + 2]; rec->direction = -1; }
	rec->f04 = spawn_block(room)[1]; rec->pal = pal_slots[0]; rec->index = n; rec->f10 = 0;
	rec->f38 = sp[6]; rec->w15 = (rec->w15 & 0xFF00) | sp[7]; rec->hp = rec->max_hp = sp[9] & 0xF;
	Char.index = n; Char.direction = rec->direction; Char.x = rec->x; Char.charid = type_to_charid[level.type];
	Char.curr_col = col; Char.curr_row = sp[2]; Char.f38 = sp[6]; Char.room = room;
	Char.f13 = Char.f12 = rec->hp; Char.hp_delta = rec->hp; Char.alive = -1; Char.pal_slot = 4;
	char_y_to_floor(); Char.fall_x = Char.fall_y = 0; Char.f24 = 0; Char.f0f = 1;
	int8_t kc = Kid.curr_col;
	uint16_t kf = Kid.frame;
	if (Char.direction != Kid.direction && ((kf >= 1 && kf <= 0xE) || (kf >= 0x31 && kf <= 0x38) || (kf >= 0x22 && kf <= 0x2C))) kc -= dir_behind[Kid.direction + 1];
	if (col + 3 >= kc && col - 3 <= kc && (sp[9] & 0xF) >= 6) { Char.f10 = 1; seqtbl_offset_char(0x5A); Char.f23 = 3; }
	else { Char.f10 = 0; seqtbl_offset_char(0x54); Char.f23 = 0; }
	play_seq();
	if (Kid.opp_index == 0xFF) Kid.opp_index = Char.index;
	save_char();
}
/* 2D3E:0A4A */
void spawn_guards(uint8_t room)
{
	if (Kid.alive >= 0 || Kid.charid == 1) return;
	uint8_t *sp = find_spawn(Kid.curr_row, Kid.room);
	if (!sp || !spawn_ok(room, sp)) return;
	if (sp[4] == 0) { spawn_guard(room, sp); sp[4] = sp[5]; sp[8]--; } else sp[4]--;
}
void init_hp_pub(level_char_init *r) { init_hp(r); }
