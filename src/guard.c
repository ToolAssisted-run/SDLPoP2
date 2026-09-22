/* Guard decision making (OVL01 2D3E:1864 dispatch, OVL10 segment 366C for charid-2 guards).
 * Like PoP1's autocontrol: each tick the guard's controls are cleared and then set by these routines as if a
 * player pressed keys; control() then runs the same state machine as for the prince.
 * Transcribed from the disassembly (the decompiler loses most branches in this overlay). */
#include "types.h"
#include "globals.h"
#include <stdlib.h>

uint32_t random_seed;        /* DS:2B7A */
uint16_t word_68ec;          /* DS:68EC (922c) countdown: an unskilled guard holds back while it runs */
uint8_t byte_5cba;           /* DS:5CBA: 2 shortens the sword reach by 5 */
#define word_4400 (*(uint16_t *)((uint8_t *)&level + 0x1848))   /* DS:4400 level header */
#define word_6d54 (*(uint16_t *)((uint8_t *)&level + 0x185C))   /* DS:4414 level header */
static const uint16_t *prob_strike, *prob_restrike, *prob_block, *prob_impblock, *prob_advance;   /* DS:1BB6.. 12 words each, by skill */
void guard_set_prob_tables(const uint8_t *ds) { const uint16_t *t = (const uint16_t *)(ds + 0x1BB6); prob_strike = t; prob_restrike = t + 12; prob_block = t + 24; prob_impblock = t + 36; prob_advance = t + 48; }

/* 2751:008C */
int random_2751(int max) { random_seed = random_seed * 0x343FD + 0x269EC3; return (int)((random_seed >> 16) % (uint32_t)(max + 1)); }

/* 2D3E:2420 (02F800): can a character walk onto this tile? */
int tile_passable_2f800(uint16_t mod, uint8_t t)
{
	int r = !tile_is_wall_kind(t) && !(t == 4 && (uint8_t)mod < 0x70);
	if (r && Char.charid != 10) { r = !tile_is_loose_kind(t); if (r && Char.charid != 7 && Char.charid != 8) r = !tile_is_empty_kind(t); }
	return r;
}

/* OVL10 366C:0002..0042: the guard "presses" a control (sets the axis and a fresh ctrl1 edge) */
static void move_forward(void)   { control_x = -1; ctrl1_forward = -1; }
static void move_backward(void)  { ctrl1_backward = -1; control_x = 1; }
static void move_up(void)        { control_y = -1; ctrl1_up = -1; }
static void move_down(void)      { ctrl1_down = -1; control_y = 1; }
static void move_down_back(void) { ctrl1_down = -1; move_backward(); }
static void move_down_forw(void) { ctrl1_down = -1; move_forward(); }
static void move_shift(int8_t v) { control_shift = v; ctrl1_shift = v; }
/* 2D3E:18FE */
static void clear_controls(void) { ctrl1_forward = ctrl1_backward = ctrl1_up = ctrl1_down = ctrl1_shift = 0; control_x = control_y = control_shift = 0; }

static int lvl5_water(void) { return Char.curr_row != 0 && level_number == 5 && (Char.room == 10 || Char.room == 7 || Char.room == 12); }
static int frame_running(uint16_t f) { return (f >= 1 && f <= 0xE) || (f >= 0x31 && f <= 0x38) || (f >= 0x22 && f <= 0x2C); }
/* the skill byte of the guard's room record (read without a count check, DS:43B0 + index*23 + room*0x74) */
static uint8_t guard_skill(void) { return ((uint8_t *)&level)[0x17F3 + Char.room * 0x74 + 1 + Char.index * 23 + 4]; }

/* 2D3E:21C6 (02F5A6): sword reach against the opponent: *near (bx) and *far (ax) */
static void sword_range(int16_t *far_ax, int16_t *near_bx)
{
	*near_bx = 0; *far_ax = -1;
	uint8_t c = Char.charid;
	if (c == 0 || c == 1) {
		if (Opp.charid == 0xB) {
			if (Opp.f24 != 6 && Opp.f19 != 0xAB && Char.f19 == 0x60) { if (Char.direction == Opp.direction) { *near_bx = 0x1F; *far_ax = 0x4B; } else { *near_bx = -7; *far_ax = 0x2D; } }
		} else if (Opp.charid == 7 || Opp.charid == 8) { *near_bx = 0x13; *far_ax = 0x2E; }
		else if (Opp.charid == 6) { *near_bx = 0xD; *far_ax = Char.direction == Opp.direction ? 0x35 : 0x39; }
		else { *near_bx = 0x13; *far_ax = 0x2E; if (Opp.f23 == 0) { *near_bx += 0xE; *far_ax += 0xE; } if (Opp.charid == 4) *far_ax += 2; }
		if (byte_5cba == 2) *far_ax -= 5;
		return;
	}
	if (c == 10) { *near_bx = -10; *far_ax = 0x26; return; }
	if (c == 0xB) {
		if (63 * (int8_t)Opp.curr_row + 0x33 > Opp.y) return;
		if (Char.direction == Opp.direction) { *near_bx = -2; *far_ax = 0x1B; } else { *near_bx = -27; *far_ax = -4; }
		return;
	}
	if (c == 6) { *near_bx = 0xD; *far_ax = 0x35; return; }
	if (Char.direction != Opp.direction) { *near_bx = 0x13; *far_ax = c == 4 ? 0x30 : 0x2E; return; }
	if (Opp.f10 == 1) { *near_bx = 0x22; *far_ax = 0x39; } else { *near_bx = 0x16; *far_ax = 0x2A; }
}

/* 366C:0CAA advance, 0D4A block, 0DC4 strike (PoP1's per-skill probabilities) */
static void guard_advance(void)
{
	if (lvl5_water() && rtlink_0dd5()) return;
	uint8_t skill = guard_skill();
	if (skill != 0 && word_68ec != 0) return;
	if (lvl5_water() && !ovl_383d2()) return;
	if (prob_advance[skill] > (uint16_t)random_2751(255)) move_forward();
}
static void guard_block(void)
{
	if (Char.charid == 10) return;
	if (Opp.frame != 0x98 && Opp.frame != 0x99 && Opp.frame != 0xA2) return;
	uint8_t skill = guard_skill();
	uint16_t p = word_68f0 ? prob_impblock[skill] : prob_block[skill];
	if (p > (uint16_t)random_2751(255)) move_up();
}
static void guard_strike(void)
{
	if (Opp.frame == 0xA9 || Opp.frame == 0x97) return;
	uint8_t skill = guard_skill();
	uint16_t p = (Char.charid != 7 && Char.charid != 8 && (Char.frame == 0xA1 || Char.frame == 0x96)) ? prob_restrike[skill] : prob_strike[skill];
	if (p > (uint16_t)random_2751(255)) move_shift(-2);
}
/* 366C:00DE: put the sword away */
static void guard_sheathe(void) { seqtbl_offset_char(Char.charid == 4 ? 0x66 : 0x54); Char.f10 = 0; Char.f23 = 0; }
/* 366C:0C42: in reach: block and strike, else advance */
static void guard_in_reach(int16_t d)
{
	int16_t far, near; sword_range(&far, &near);
	if (near > d || d >= far || (lvl5_water() && rtlink_0dd5())) { guard_advance(); return; }
	guard_block();
	if (word_922e == 0) guard_strike();
}
/* 366C:0B60: nothing in reach: back off from a ledge, sheathe when the prince runs away, else advance */
static void guard_idle_armed(void)
{
	uint8_t t1 = get_tile_infrontof(1), t2 = get_tile_infrontof(2);
	if (tile_is_empty_kind(t1) && tile_is_empty_kind(t2)) {
		if (lvl5_water() && rtlink_0dd5()) return;
		move_backward(); return;
	}
	if (Char.direction == Opp.direction && frame_running(Opp.frame) && Opp.action != 7 && Char.f38 != 0) {
		if (lvl5_water()) guard_advance(); else guard_sheathe();
		return;
	}
	guard_advance();
}
/* 366C:0360 */
static void guard_close(int16_t d)
{
	if (Opp.f10 == 1 && Char.direction != Opp.direction) { guard_in_reach(d); return; }
	if (word_922e != 0) return;
	int16_t far, near; sword_range(&far, &near);
	if (far > d) { move_shift(-2); return; }
	if (lvl5_water() && !ovl_383d2()) return;
	move_forward();
}
/* 366C:023E: the prince dropped from a ledge in front: follow or back away */
static void guard_after_drop(void)
{
	if (Opp.action == 2 || Opp.action == 6) return;
	int back = 0;
	uint8_t t = get_tile_infrontof(1);
	if (wall_type(t)) back = 1;
	else if (!tile_is_empty_kind(t)) { move_forward(); return; }
	else {
		t = get_tile(tile_row + 1, tile_col, curr_room);
		if ((tile_is_loose_kind(t) && Char.charid != 10) || wall_type(t) || tile_is_empty_kind(t) || Opp.curr_row != Char.curr_row + 1) back = 1;
		else {
			if (word_4400 != 0 && get_tile(tile_row, tile_col - 1, curr_room) == 2) back = 1;
			if (!back) { move_forward(); return; }
		}
	}
	if (back) { word_6146 = 0; move_backward(); }
}
/* 366C:0774: sword drawn */
static void guard_armed(void)
{
	uint16_t f = Char.frame;
	if (f == 0xA6 || f < 0x96) return;
	if (Char.f23 == 0) {
		if (word_6146 != 0) { guard_after_drop(); return; }
		if (Char.charid == 4) return;
		move_down_back(); return;
	}
	uint16_t of = Opp.frame; int16_t d = opp_distance();
	if (d >= 0x1B && of >= 0x66 && of < 0x76 && Opp.action == 5) return;
	int16_t far, near; sword_range(&far, &near);
	if (far + 12 > d) {
		if (near <= d) { guard_close(d); return; }
		if (lvl5_water() && rtlink_0dd5()) { move_forward(); return; }
		uint8_t t = get_tile_behind_char();
		if (tile_passable_2f800(curr_modifier, t) && d > 0) { move_backward(); return; }
		if (!lvl5_water()) { move_forward(); return; }
		if (ovl_383d2()) move_forward();
		return;
	}
	if (word_922e != 0) return;
	if (Char.direction != Opp.direction) {
		if (of >= 7 && of < 0xF) { if (d < 0x47) move_shift(-2); return; }
		if (of >= 0x22 && of < 0x2C) { if (d < 0x5E) move_shift(-2); return; }
	}
	guard_idle_armed();
}
/* 366C:03E2: sword not drawn */
static void guard_unarmed(void)
{
	if (Opp.alive >= 0) return;
	int16_t d = opp_distance();
	if (Opp.curr_row == Char.curr_row && d < 0 && d >= -18) {
		if (Char.f23 < 2 || Char.frame != 0xA6) return;
		move_down_forw(); return;
	}
	if (word_6140 != 0) { if (d < -4) move_down(); return; }
	if (d < 0 || Char.f23 == 0) return;
	move_down_forw();
}
/* 366C:11F8 */
static int room_draws_sword(uint8_t room) { return level_number == 4 && room >= 0x16 && room <= 0x1C && room != 0x17; }
/* 366C:0668: a pit in front: maybe jump it (2FDF:17A6), else turn back (seq 0x65) */
static void guard_pit_ahead(void)
{
	uint8_t t = get_tile_infrontof(1), t2 = get_tile_infrontof(2); int8_t base, k = 0;
	if (!tile_is_empty_kind(t2)) return;
	if (tile_is_empty_kind(t)) base = 1; else { base = 2; t = t2; }
	while (tile_is_empty_kind(t) && k <= 4) { k++; t = get_tile(Char.curr_row, dir_front[Char.direction + 1] * (k + base) + Char.curr_col, Char.room); }
	int jump = 1;
	if (k >= 4 && (k != 4 || random_2751((int8_t)Char.f38 - 1) != 0)) {
		if (!(Char.room == 13 && level_number == 13 && Kid.f24 != 13)) {
			level_char_init *rec = room_char_record(Char.index, Char.room);
			if (rec == NULL || (int8_t)rec->w15 == Char.curr_row) jump = 0;
		}
	}
	if (!(jump && control_runjump(100))) { seqtbl_offset_char(0x65); Char.f23 = 2; }
}
static char_type *char_slot(int8_t i) { return i == -1 ? &Kid : &chars[i]; }   /* chars[-1] is Kid in the DOS data segment */
/* 366C:043C: guard frames 0xC2..0xC7 (odd) and 0xD4: notice the prince and draw, or turn back */
static void guard_turning(void)
{
	uint16_t f = Char.frame;
	if (!((f >= 0xC2 && f <= 0xC7 && (f & 1)) || f == 0xD4)) {
		if ((int8_t)Char.f38 < 1 || (f != 0xC0 && f != 0xC4)) return;
		guard_pit_ahead(); return;
	}
	int8_t o = find_opponent(~Char.direction);
	int di = o == (int8_t)Char.index ? 0x79 : abs(Char.x - char_slot(o)->x);
	uint8_t front = get_tile_infrontof(1);
	int si = abs(Char.x - Kid.x);
	if (frame_running(Kid.frame)) si += Kid.direction == Char.direction ? 0x20 : -0x20;
	else if (Kid.f10 != 1) si += 0x20;
	level_char_init *rec = room_char_record(Char.index, Char.room);
	int8_t home_row = rec ? (int8_t)rec->w15 : 0;
	int id = 0x65;
	if ((Char.curr_row == Kid.curr_row || home_row == Kid.curr_row) && tile_passable_2f800(curr_modifier, front)) {
		int notice = 0;
		if (Char.curr_row == Kid.curr_row) {
			if (Char.direction != Opp.direction || !frame_running(Opp.frame) || Opp.action == 7) { if (di <= 0x78 || si <= 0x78) notice = 1; }
		}
		if (!notice && ((Char.direction == -1 && Char.curr_col < Kid.curr_col) || (Char.direction == 0 && Char.curr_col > Kid.curr_col))) notice = 1;
		if (notice) Char.f23 = 2;
		else if (Kid.alive >= 0 || Kid.f24 == 0xD) Char.f23 = 0;
		else { if (room_draws_sword(Char.room) && si <= 0x50) { id = 0x57; Char.f10 = 1; } else id = -1; }
	} else {
		int16_t d = distance_to_edge_weight();
		if (Char.curr_row == Kid.curr_row || home_row == Kid.curr_row) {
			if ((int8_t)Char.f38 >= 1 && d < 0x1B) Char.f23 = 0;
			else if ((int8_t)Char.f38 >= 1 && front != 4) id = -1;
			else Char.f23 = 0;
		} else Char.f23 = 0;
	}
	if (id == -1) return;
	seqtbl_offset_char(id);
	uint8_t t = get_tile_behind_char();
	if (Char.frame == 0xD4 && !tile_passable_2f800(curr_modifier, t)) Char.x = char_dx_forward(8);
}
/* 2D3E:192C (02ED0C): charid-2 guards */
static void guard_ai(void)
{
	if (Char.alive >= 0 || Char.f12 == 0 || (int8_t)Char.f12 + Char.hp_delta == 0) return;
	if (Char.f10 == 0) {
		if (Char.frame >= 0xBA && Char.frame <= 0xD4) guard_turning(); else guard_unarmed();
		if (word_6140 != 0 && (int8_t)room_nchars(Char.room) - (int8_t)Char.index - 1 == 0) word_6140 = 0;
	} else {
		if (Kid.f24 != 0xD) { guard_armed(); return; }
		if (word_6d54 != 0 && Char.f19 != 0x6E) seqtbl_offset_char(0x6E);
	}
}
/* 2D3E:1864 (02EC44): per-charid control for a non-player character */
void autocontrol(void)
{
	clear_controls();
	if (word_68f0) word_68f0--;
	if (word_68ec) word_68ec--;
	if (word_922e) word_922e--;
	switch (Char.charid) {
	case 1: if (Char.room == 4 && level_number == 13) ovl_shadow_37f0_78(); break;
	case 4: case 10: ovl_366c_10cc(); break;
	case 6: ovl_33fd_694(); break;
	case 7: case 8: ovl_366c_e0a(); break;
	case 11: ovl_366c_11(); break;
	default: if (Char.charid != 0) guard_ai(); break;
	}
}

/* 366C:0082: the sequence a dead guard switches to (odd-index type-0 guards: 0xC3; type 7: 0xBF + index%5 - 1) */
static int dead_guard_seq(uint8_t type)
{
	int si = 0, di = 0;
	if (type == 7) { si = 0xBF; di = 4; }
	else if (type == 0 && level_kind != 6) { si = 0xC3; di = 1; }
	if (si) { int r = (int8_t)Char.index % (di + 1); if (r) return si + r - 1; }
	return 0;
}
/* 366C:0052: charid-2 guards after play_seq */
void guard_after_seq(void)
{
	if (Char.frame != 0xB9) return;
	int id = dead_guard_seq(level.type);
	if (id) { seqtbl_offset_char(id); play_seq(); }
	if (Char.alive < 0x14) Char.alive++;
}
void sword_range_pub(int16_t *far_ax, int16_t *near_bx) { sword_range(far_ax, near_bx); }
