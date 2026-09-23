/* Skeletons (charid 4, level type 2) and charid-10 risers: collapsing, the revive timer, waking up when the prince
 * comes close (OVL10 366C:0F60, 0E4A, 10CC, 1166, 1220, 125E, 1294, 1328, 13E8, 140E, 14D8). */
#include "types.h"
#include "globals.h"

static int room_draws_sword4(uint8_t room) { return level_number == 4 && room >= 0x16 && room <= 0x1C && room != 0x17; }   /* 366C:11F8 */
/* 366C:1220: collapse stages before getting up again (record byte +0x16) */
static void set_stage(uint8_t index)
{
	level_char_init *r = room_char_record(index, drawn_room);
	if (r) ((uint8_t *)r)[0x16] = r->f04 ? (r->f04 + 1) % 2 : 3;
}
/* 366C:125E: revive timer (record word +0x13) */
void set_revive_timer(uint16_t v, uint8_t index)
{
	if (level.type != 2 && chars[0].charid != 10) return;
	level_char_init *r = room_char_record(index, drawn_room);
	if (r) r->w13 = v;
}
/* 366C:1166: the skeleton collapses into a heap of bones */
void skel_collapse(void)
{
	seqtbl_offset_char(0x78); play_sound(0x4C);
	Char.f0f = 1; Char.f10 = 0;
	if (Char.alive < 0) Char.alive = 0;
	if (Char.direction == 0 && tile_is_empty_kind(get_tile_behind_char())) {
		int d = 0x20 - distance_to_edge_weight();
		if (d <= 0x23) Char.x = char_dx_forward(0x23 - d);
	}
	save_char();
	hp_bar_draw(Char.index, 0, Char.f13);   /* 0FB3:25D4 */
	set_revive_timer(0x78, Char.index); set_stage(Char.index);
	load_char(Char.index);
}
/* 366C:14D8 */
static void skel_turn(void)
{
	if (Char.x < Kid.x) { if (Char.direction != 0) { Char.direction = 0; Char.x = char_dx_forward(10); } }
	else if (Char.direction != -1) { Char.direction = -1; Char.x = char_dx_forward(10); }
}
/* 366C:10CC: skeleton / charid-10 decisions */
void skel_ai(void)
{
	if (Char.alive >= 0) {   /* a heap of bones: count the revive timer down */
		level_char_init *r = room_char_record(Char.index, Char.room);
		if (r && (int16_t)r->w13 > 0) r->w13--;
		return;
	}
	int go = 1;
	if (Char.curr_row != 0 && level_number == 5 && (Char.room == 10 || Char.room == 7 || Char.room == 12)) go = ovl_2a31_ddf();
	else if (Kid.alive >= 5 && Char.charid == 4 && Char.f19 != 0x78) { skel_collapse(); go = 0; }
	if (!go) return;
	if (Char.f19 == 0x58 && Char.frame == 0xB1) { skel_turn(); return; }
	guard_ai_pub();
}
/* 366C:0E4A: is the prince in line of sight within range columns (no walls, loose floors or low gates in between)? */
static int wake_in_range(int range, int8_t krow, int8_t kcol, int8_t i)
{
	level_char_init *r = room_char_record(i, drawn_room);
	if (!r || Char.alive < 0 || Char.f24 == 5 || Kid.charid != 0) return 0;   /* only lying (alive >= 0) ones get up */
	if (r->w13 != 0 && !room_draws_sword4(Char.room)) return 0;
	int8_t end, step;
	if (Char.curr_col > kcol) { end = Char.curr_col + 1; step = 1; } else { end = Char.curr_col - 1; step = -1; }
	if (Char.curr_row != krow || kcol - range > Char.curr_col || kcol + range < Char.curr_col) return 0;
	int ok;
	do {
		uint8_t t = get_tile(krow, kcol, Char.room);
		ok = !((tile_is_loose_kind(t) && Char.charid != 10) || tile_is_wall_kind(t) || (t == 4 && can_bump_into_gate()));
		kcol += step;
	} while (ok && kcol != end);
	return ok;
}
/* 366C:140E: ready to get up? (a stage countdown; level 4: when the door at room 22 is open) */
static int skel_ready(uint8_t index)
{
	level_char_init *r = room_char_record(index, drawn_room);
	if (!r) return 0;
	int ready = 0, set_timer = 0;
	if ((uint16_t)r->y == 0xFFFF) return 0;
	if (room_draws_sword4(Char.room)) {
		get_tile(2, 4, 0x16);
		if (curr_modifier > 0x29 && Char.room == Kid.room && Char.x > Kid.x) return 1;
		set_timer = r->w13 == 0 && Char.f19 == 0x70;
	} else if (((uint8_t *)r)[0x16] != 0) { ((uint8_t *)r)[0x16]--; set_timer = 1; }
	else ready = 1;
	if (set_timer) set_revive_timer(random_2751(0x1E) + 0xF, index);
	return ready;
}
/* 366C:0F60: every tick on level type 2: bones near the prince get up */
void skeleton_wake(void)
{
	int range = room_draws_sword4(Kid.room) ? 8 : level.type == 2 ? 4 : 5;
	int8_t n = room_nchars(drawn_room);
	/* SI starts as an uninitialised stack word (BP-6) and is not reset per character: on level type 2 a record with y == -1
	 * keeps the previous value. The captures show it nonzero on the first character, so bones with y == -1 get up. */
	int up = 1;
	for (int8_t i = 0; i < n; i++) {
		load_char(i);
		if (Kid.alive < 0 && Kid.action != 4 && Kid.action != 3 && Kid.action != 5 && wake_in_range(range, Kid.curr_row, Kid.curr_col, i)) {
			level_char_init *r = room_char_record(Char.index, Char.room);
			if (level.type == 2) {
				if ((uint16_t)r->y != 0xFFFF) {
					uint16_t timer = r->w13;
					if (skel_ready(Char.index)) { seqtbl_offset_char(0x58); up = 1; Char.charid = 4; }
					else { if (timer == 0 && Char.f19 != 0x70) seqtbl_offset_char(0x70); up = 0; }
				}
			} else { seqtbl_offset_char(0x6A); up = 1; Char.charid = 10; }
			play_seq();
			if (up) {
				init_hp_pub(r);
				Char.alive = -1; Char.f10 = 1; Char.f23 = 3;
				if (Char.charid == 10) Char.direction = Char.x >= Kid.x ? -1 : 0;
				word_6140 = 0; word_922e = 0; Char.fall_x = Char.fall_y = 0;
				if (Kid.opp_index == 0xFF) Kid.opp_index = Char.index;
			}
		}
		save_char();
	}
}
/* 366C:1328: a skeleton record entering the drawn room */
level_char_init *skel_room_entry(level_char_init *r)
{
	if (r->y == 1) { r->seq_id = 0x3F; Char.f10 = 1; }
	else {
		r->seq_id = (Char.index == 0 && r->y == 0) ? 0x70 : 0x77;
		r->w13 = r->y;
		set_stage(Char.index);
		Char.f10 = 0;
		if ((Char.direction == -1 && tile_is_empty_kind(get_tile_infrontof(1))) || (Char.direction == 0 && tile_is_empty_kind(get_tile_behind_char()))) {
			int d = distance_to_edge_weight(); if (Char.direction == 0) d = 0x20 - d;
			if (d <= 0x23) { d = 0x23 - d; if (Char.direction == -1) d = -d; Char.x = char_dx_forward(d); }
		}
	}
	r->seq_pos = 0; Char.pal_slot = 8;
	return r;
}
/* 366C:13E8: a wall blade hits a skeleton */
void skel_blade_hit(void) { if (take_hp(1)) { char_dies_pub(); return; } seqtbl_offset_char(0x7E); play_sound(0x48); }
/* 366C:1294 (type 2): a heavy landing in a row brings standing-up bones down */
void skel_row_shake(int8_t row, uint8_t room)
{
	char_type saved = Char;
	int8_t n = room_nchars(room);
	for (int8_t i = 0; i < n; i++) {
		load_char(i);
		if (Char.curr_row == row && Char.frame == 0xCE && Char.f24 != 5) {
			level_char_init *r = room_char_record(Char.index, Char.room);
			if (r && r->y == 0) { seqtbl_offset_char(0x70); save_char(); }
		}
	}
	Char = saved;
}
