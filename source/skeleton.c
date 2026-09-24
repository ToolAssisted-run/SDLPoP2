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

/* 366C:1704 (sequence opcode FFF? control 3, from the drink of level 12's potion): the small prince (charid 0xC, type 9)
 * appears 0x55 pixels ahead of the prince, facing left, in the prince's room: a new record, sequence 0xB4 (he walks
 * to the door and opens it), music 0xC4 */
void small_prince_appears(void)
{
	Kid = Char;
	int16_t x = (int16_t)(Kid.x + 0x55 * (int8_t)ds_byte((uint16_t)(0xCF9 + (int8_t)Kid.direction)));
	int8_t col = col_from_x18(x), row = Kid.curr_row;   /* 0AFF:1010 */
	level_room *room = ROOM_REC(Kid.room); uint8_t n = room->nchars++;
	Char.index = n;
	level_char_init *rec = &room->chars[n];   /* 2D3E:08AC */
	Char.charid = 0xC; rec->type = 9;
	Char.room = Kid.room; Char.curr_row = row;
	rec->tilepos = (int8_t)(row_tilepos(row) + col);   /* 0AFF:07D4 */
	char_y_to_floor();                                  /* 0AFF:07B0 */
	rec->x = x; Char.x = x; Char.curr_col = col;
	rec->direction = -1; Char.direction = -1;
	rec->f38 = 0; Char.f38 = 0; rec->opp_index = 0; Char.opp_index = 0; Char.f23 = 0;
	Char.f2a = 0; Char.f26 = 0; Char.f28 = 0; Char.f24 = 0;
	Char.pal_slot = 4; Char.f0f = 1; Char.f10 = 1;
	rec->seq_id = 0xB4; rec->seq_pos = 0;
	seqtbl_offset_char(0xB4); sound_1611_01a8(0xC4);
	rec->max_hp = 6; init_hp_pub(rec);                  /* 2D3E:0008 */
	rec->f04 = 0xA;
	Char.alive = -1;
	word_6140 = 0; word_68f0 = 0; word_922e = 0;   /* (DS:6140, 68F0, 68EE) */
	Char.fall_x = Char.fall_y = 0;                      /* 0AFF:0952 */
	Char.action = 1;
	load_guard_sprites(rec->type);                      /* 1286:0066 */
	save_char(); loadkid();
}
/* 366C:1668 (every tick, the drawn room's charid-0xC character): gone once off the room (column below -1 or past 10) */
void small_prince_tick(void) { if (Char.curr_col < -1 || Char.curr_col > 10) clear_char(); }
