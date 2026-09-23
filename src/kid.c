/* The prince's per-tick update: 169B:0692 play_kid_frame and the fall / landing / tile checks it runs
 * (0AFF:0AAA start_fall, OVL01 02FE86 / 02FFE0 landing, 1375:06F2 / 0758 dispatchers). */
#include "types.h"
#include "globals.h"

uint16_t word_5cd8;          /* DS:5CD8 (8618): nonzero freezes the kid (cutscene / pause) */
uint16_t word_6142, word_6146;   /* DS:6142 (8a82) fall scream played, DS:6146 (8a86) */
int16_t word_087e;           /* DS:087E (31be): current sound slot */
int16_t word_37e8;           /* DS:37E8: level-1 sea room counter (OVL02 hooks) */
static const int16_t y_land_tbl[6] = {66, 129, 192, 255, 6400, -3584};   /* DS:0D40 (rows 4/5 read the words that follow) */

/* 0AFF:095C: schedule a loss of n hp; 1 = the character dies from it */
int take_hp(int n)
{
	int died = 0;
	if (Char.charid != 4 || Char.curr_row == 0 || level_number != 5 || (Char.room != 10 && Char.room != 7 && Char.room != 12)) {
		if (n < Char.f12) { if (-(int)Char.hp_delta < n) Char.hp_delta = (int8_t)-n; else died = (int)Char.f12 + Char.hp_delta == 0; }
		else { if (-(int)Char.f12 < Char.hp_delta) Char.hp_delta = (int8_t)-Char.f12; died = 1; }
	}
	return died;
}
/* 0AFF:0DC4 */
void die_at_bottom(void)
{
	take_hp(100); Char.frame = 0xB9; seq_set_85f8(3); Char.y = 0x180; Char.fall_y = 0; Char.action = 1;
	if (Char.charid == 1) shadow_2fba4();
}
/* 0AFF:0D1E: the character is outside the level (room 0) */
void char_fell_out(void)
{
	fall_accel(); fall_speed();
	if (level_kind == 5) {
		if (Char.y < 0x181) { if (Char.charid == 2 && Char.frame != 0xB9 && !sound_playing(0x2729)) play_sound(0x19); return; }
		if (drawn_room != 0x13 && drawn_room != 0x10) { play_sound(0); take_hp(100); Char.frame = 0xB9; seq_set_85f8(3); }
		Char.y = 0x180; Char.fall_y = 0; Char.action = 1;
		if (Char.charid != 0 && Char.index == Kid.opp_index) Kid.opp_index = find_opponent(Char.direction);
	} else if (Char.y > 0x180) die_at_bottom();
}

/* OVL01 02DCC8: pick the live character nearest to the kid; mode 1 = either side, -1 = at or left of the kid, 0 = right of it */
int8_t find_opponent(int8_t mode)
{
	int8_t best = -1;
	if (Kid.room == 0 || drawn_room == 0) return -1;
	int8_t n = ROOM_REC(Kid.room)->nchars;
	for (int8_t i = 0; i < n; i++) {
		const level_char_init *rec = &ROOM_REC(drawn_room)->chars[i];
		const char_type *c = &chars[i];
		int dy = c->y - Kid.y, dx = -1; if (dy < 0) dy = -dy;
		if (c->alive < 0 && c->charid != 0xB && ((c->charid != 7 && c->charid != 8) || (uint8_t)rec->y != 2)) {
			if (mode == 1) { dx = c->x - Kid.x; if (dx < 0) dx = -dx; }
			else if (mode == -1 && c->x <= Kid.x) dx = Kid.x - c->x;
			else if (mode == 0 && c->x > Kid.x) dx = c->x - Kid.x;
			else dy = -1;
			if (dx < 0) dx = 0;
		} else dy = -1;
		if (dy < 0 || dx < 0) continue;
		if (best != -1) {
			int bdx = chars[best].x - Kid.x, bdy = chars[best].y - Kid.y; if (bdx < 0) bdx = -bdx; if (bdy < 0) bdy = -bdy;
			int dd = dy - bdy; if (dd < 0) dd = -dd;
			if ((dd < 0x2B || bdy <= dy) && (dd > 0x29 || bdx <= dx)) continue;
		}
		best = i;
	}
	return best;
}
/* OVL01 031BC4: is a live guard (with an engaged side, not a charid-11 one) in the prince's room? (sheathing picks 0x5D over 0x5C) */
int char_scan_31bc4(void)
{
	char_type saved = Char; int r = 0;
	loadkid();
	if (Char.f24 != 0xD && Char.charid != 1 && Char.action != 3 && Char.action != 4 && !is_dead_frame(Char.frame) && Kid.room != 0) {
		int8_t n = ROOM_REC(Kid.room)->nchars;
		for (int8_t i = 0; i < n && r == 0; i++) {
			load_char(i);
			if (Char.alive < 0 && Char.f23 > 0 && Char.charid != 0xB) {
				const level_char_init *rec = Char.room ? &ROOM_REC(Char.room)->chars[Char.index] : 0;
				r = !((Char.charid == 7 || Char.charid == 8) && rec && (uint8_t)rec->y != 1 && (uint8_t)rec->y != 3);
			}
		}
	}
	Char = saved; return r;
}
/* 0AFF:080A */
void load_opp_080a(int n) { if (n >= 0 && n < 5) { loadkid(); Opp = chars[n]; } }

/* OVL01 02F712: is this a striking frame for the character kind? */
int frame_is_strike_02f712(uint16_t frame, uint8_t charid)
{
	if (charid == 7 || charid == 8) return (frame > 0xA4 && frame < 0xA8) || frame == 0xB5;
	if (charid == 10) return frame == 0xDF || frame == 0xE0;
	if (charid == 0xB) return Char.f19 == 0xAD && (frame == 0x112 || frame == 0x118);
	if (frame == 0x99 || frame == 0x9A) return 1;
	if (charid != 0) return 0;
	return frame == 0xF4 || frame == 0xF5;
}

/* 0AFF:1514 / 14F6 */
uint8_t get_tile_above_front(void)  { return get_tile(Char.curr_row - 1, dir_front[Char.direction + 1] + Char.curr_col, Char.room); }
uint8_t get_tile_above_behind(void) { return get_tile(Char.curr_row - 1, dir_behind[Char.direction + 1] + Char.curr_col, Char.room); }
/* OVL02 33FD:0AA0 / 0ABA (level-kind 5, the sea room 19 of level 1) */
static int ovl_34a70(void) { return Char.room == 0x13 && Char.f19 == 0x3B; }
static int ovl_34a8a(void) { return Char.room == 0x13 && Char.curr_row < 3 && word_37e8 < 0x20 && Char.x < 0xD0; }

/* 0AFF:0F10: standing inside a wall tile: move to its edge and re-read the tile under the character */
static uint8_t push_out_of_wall(void)
{
	if (Char.f24 == 4 || Char.charid == 0xB || (Char.room == 3 && level_kind == 6 && Char.curr_col < 0)) return get_tile_at_char();
	int16_t d = distance_to_edge_weight(); uint8_t t = get_tile_infrontof(1);
	if (d < 11 && !tile_is_wall_kind(t)) d += 11; else d -= 32;
	Char.x = char_dx_forward(d); load_fram_det_col();
	return get_tile_at_char();
}
/* 0AFF:0ECC */
static int can_grab_ledge(void)
{
	uint8_t above = get_tile_above_char(); uint16_t m_above = curr_modifier;
	uint8_t front = get_tile_above_front();
	int r = can_climb_down_146e(curr_modifier, m_above, front, above);
	if (level_kind == 5 && r == 0) r = ovl_34a8a();
	return r;
}
/* 0AFF:0E28: grab the ledge above while jumping or falling past it (shift held) */
static void try_grab_ledge(void)
{
	int16_t x0 = Char.x;
	if (ctrl1_shift == 0 || Char.fall_y >= 32 || Char.alive >= 0 || Char.curr_row * 63 + 56 > Char.y + 25) return;
	Char.x = char_dx_forward(-16); load_fram_det_col();
	if (!can_grab_ledge()) { Char.x = x0; return; }
	int16_t d = distance_to_edge_weight();
	Char.x = char_dx_forward(d + 4 - (Char.direction == -1 ? 7 : 9));
	char_y_to_floor(); Char.fall_y = 0;
	seqtbl_offset_char(15); play_seq(); play_sound(0xB);
	word_8a84 = 0xC; word_6142 = 0;
}

void try_grab_ledge_pub(void) { try_grab_ledge(); }
/* 0AFF:0AAA: start falling from the current frame */
static void start_fall(void)
{
	uint16_t frame = Char.frame; int adjust = 0; int16_t id = -1;
	Char.curr_row++;
	if (Char.charid == 1) shadow_hook_2f9a2();
	if (Char.f10 == 1) Char.f10 = 0;
	if (Char.charid == 0 && level_number == 5 && Char.room == 10 && ((Char.direction == 0 && ctrl1_forward != 0) || (Char.direction == -1 && ctrl1_backward != 0))) { id = 0xC6; adjust = 1; }
	if (id == -1) {
		if (frame == 0x51) { id = 7; Char.y += 12; }
		else if (frame == 9) id = 7;
		else if (frame == 0xD) id = 0x13;
		else if (frame == 0x1A) { id = 0x12; adjust = 1; }
		else if (frame == 0x2C) { id = 0x15; adjust = 1; }
		else {
			adjust = 1;
			if (frame >= 0x51 && frame <= 0x55) id = 0x13;
			else if (frame >= 0x96 && frame <= 0xB3) {
				if (Char.charid == 0) { word_6146 = 1; id = (frame == 0x99 || Char.f19 == 0x38) ? 0x5F : 0x51; }
				else { word_6146 = 0; id = (Char.f19 == 0x56 || Char.f19 == 0x43 || Char.f19 == 0x6C) ? 0x53 : 0x52; }
			}
			else if (Char.charid == 2 && Char.f19 == 100) id = 0xBA;
			else id = 7;
		}
	}
	if (adjust) {
		if (tile_is_wall_kind(get_tile_infrontof(1)) || tile_is_wall_kind(get_tile_behind_char()) || !tile_is_empty_kind(get_tile_above_char())
		    || !tile_is_empty_kind(get_tile_above_front()) || !tile_is_empty_kind(get_tile_above_behind())) {
			int16_t d = distance_to_edge_weight();
			if (d < 8) Char.x = char_dx_forward(d - 11);
			else if (d > 0x18) Char.x = char_dx_forward(d - 0x15);
			if (!tile_is_empty_kind(get_tile_above_char()) || !tile_is_empty_kind(get_tile_above_front())) {
				int16_t yy = Char.curr_row * 63 + 1; if (Char.y < yy) Char.y = yy;
			}
		}
	}
	if (level_kind == 1) { id = 0x1B; ovl_349be(); seq_set_85f8(8); }
	else if (Char.charid == 0 && is_feather_fall != 0 && Char.f19 != 0xE4) id = 0xE4;
	seqtbl_offset_char(id); play_seq(); load_fram_det_col();
	if (!tile_is_wall_kind(get_tile_at_char())) {
		if (tile_is_wall_kind(get_tile_infrontof(1))) {
			if (frame == 0x2C && distance_to_edge_weight() < 14) { seqtbl_offset_char(0x72); play_seq(); }
			else Char.x = char_dx_forward(-2);
			load_fram_det_col();
		}
		return;
	}
	push_out_of_wall();
}

/* OVL01 0301D2: after landing, step away from an edge or a loose tile */
static void land_adjust(void)
{
	int16_t d = distance_to_edge_weight(); uint8_t t;
	if (d < 0x14) t = get_tile_infrontof(1); else if (d < 0x15) t = 1; else t = get_tile_behind_char();
	int kind = tile_is_empty_kind(t) ? 1 : ((t == 0xB || t == 0x1A || t == 0xF) ? 2 : 0);
	if (kind == 0) return;
	Char.x = char_dx_forward(d - 0x14);
	if (kind == 2 && Char.curr_row > 2) {
		int16_t s = d - 0x14 < 0 ? -1 : 1;
		Char.x = char_dx_forward(s * 0x12);
		if (t == 0x1A) Char.x = char_dx_forward(s * 9);
	}
}
/* OVL01 02FFE0: landing on a floor */
static void land(void)
{
	int died = 0, hard = 0; int16_t id = 0x11, snd = -1;
	word_6142 = 0;
	char_y_to_floor();
	uint8_t t = get_tile_at_char();
	if (t != 0x17 && t != 0x18) {
		int16_t d = distance_to_edge_weight(); uint8_t f = get_tile_infrontof(1);
		if (Char.charid != 7 && Char.charid != 8 && (tile_is_empty_kind(f) || tile_is_loose_kind(f)) && d < 11) Char.x = char_dx_forward(d - 13);
		if (Char.alive >= 0) hard = 1;
	}
	t = get_tile_at_char();
	if (t == 0x17 || t == 0x18) { ovl_34724(); return; }
	if (Char.fall_y < 0x16 || Char.charid == 1) {
		if ((Char.charid < 2 && Char.f10 != 1) || Char.charid == 6 || Char.charid == 1) id = 0x11;
		else { id = Char.f19 == 0xBA ? 0xBB : 0x3F; Char.f10 = 1; }
		snd = Char.charid == 4 ? 0x4C : 0x11;
	} else if (Char.fall_y < 0x21 && Char.index == 10) {
		if (!take_hp(1) && (Char.room != 0xB || level_kind != 5 || Char.curr_row < 3)) { snd = 0x10; id = 0x14; }
		else { died = 1; take_hp(100); }
	} else hard = 1;
	if (hard) { take_hp(100); died = 1; }
	if (died) {
		if (Char.charid == 4) { ovl_37826(); id = 0x70; } else { snd = 0; id = 0x16; }
		seq_set_85f8(3); land_adjust();
	}
	if (Char.charid == 7 || Char.charid == 8) { Char.x = char_dx_forward(6); id = 0x9B; }
	seqtbl_offset_char(id); play_seq(); Char.fall_y = 0;
	if (snd != -1) { play_sound(snd); if (Char.charid == 0) word_6140 = 1; }
}
/* OVL01 02FE86: a falling character: scream, ledge grab, landing or loose floor */
static void check_fall_landing(void)
{
	if (word_6142 == 0 && Char.fall_y > 0x1E) {
		if (Char.charid == 0) { fall_scream_room(Char.room); word_6142 = 1; }
		else if (Char.charid == 2) { play_sound(0x19); if (Char.curr_row > 5) die_at_bottom(); }
	}
	if (Char.y < Char.curr_row * 63 + 56) {
		try_grab_ledge();
		if ((Char.charid == 7 || Char.charid == 8) && Char.fall_y != 0) Char.curr_row = y_to_row(Char.y);
		return;
	}
	uint8_t t = get_tile_at_char();
	if (tile_is_wall_kind(t)) t = push_out_of_wall();
	if (tile_is_empty_kind(t)) { Char.curr_row++; return; }
	if (t != 0xB && t != 0xF && t != 0x1A) { land(); return; }
	if (t == 0xB) loose_floor_touch(Char.fall_y); else if (t == 0xF) ovl_348e6(); else ovl_3564e();
	if ((Char.fall_y > 32 || Char.index != 10) && Char.charid != 1) take_hp(100);
	Char.fall_y /= 2;
	Char.y = y_land_tbl[Char.curr_row & 7] - 7;
	if (word_087e == 1) word_087e = -1;
	sound_194c_83d2(0x2711);
	word_6142 = 0;
}
/* 0AFF:09F8: a standing-type frame (flag 0x40) over nothing starts a fall */
static void check_standing_on_air(void)
{
	if (!(frame_flags & 0x40) || Char.charid == 10) return;
	uint8_t t = get_tile_at_char();
	if (tile_is_wall_kind(t) && (t != 7 || ((Char.frame < 0xF6 || Char.frame > 0x105) && Char.charid != 0xB))) t = push_out_of_wall();
	if (tile_is_empty_kind(t) || (t == 0x1E && ((uint8_t)curr_modifier & 0xF) < 2)) {
		if (level_kind == 5 && ovl_34a70()) return;
		if (level_kind == 2 && ovl_35240(0x10)) return;
		if (Char.charid == 0xB) { Char.x = char_dx_forward(distance_to_edge_weight() - 42); return; }
		start_fall();
	}
}
/* 1375:06F2 */
void check_fall_or_ceiling(void)
{
	if (Char.charid == 7 || Char.charid == 8 || Char.action == 9) { if (Char.fall_y == 0) return; }
	else {
		if (Char.action == 2) return;
		if (Char.action == 3) { if (Char.frame >= 0x66 && Char.frame <= 0x69) try_grab_ledge(); return; }
		if (Char.action != 4) {
			if (Char.action == 5) { if (Char.frame != 0x6D) return; }
			else if (Char.action == 6) return;
			check_standing_on_air(); return;
		}
	}
	check_fall_landing();
}
/* 1375:0758: spikes / loose floors / chompers under or above the character */
void check_tile_effects(void)
{
	uint16_t frame = Char.frame; uint8_t t;
	if (Char.charid == 0xB || Char.charid == 10) return;
	if (Char.charid == 0 && is_feather_fall != 0) return;
	if (Char.charid == 1 && Char.f19 > 0x1C && Char.f19 < 0x2B) return;
	int a = Char.action;
	if ((frame < 0x57 || frame > 99) && (frame < 0x87 || frame > 0x8C)) {
		if (a > 1 && a != 7 && a != 5 && a != 8) t = 0;
		else {
			t = get_tile_above_char();
			if (frame == 0x4F && t == 0xB) loose_floor_touch(0);
			else if (frame == 0x4F && t == 0xF) ovl_348e6();
			else t = (frame_flags & 0x40) ? get_tile_at_char() : 0;
		}
	} else t = get_tile_above_char();
	if (t == 5 || t == 6 || t == 0x22) { if (Char.alive >= 0 && Char.charid != 4) press_button_hold(); else press_button(-1, 0); }
	else if (t == 0xB || t == 0xF) { word_6140 = 1; if (t == 0xB) loose_floor_touch(0); else ovl_348e6(); }
}
/* 169B:0E94: running into the opponent's drawn sword */
static void check_opp_bump(void)
{
	if (Opp.direction == 0x56 || Opp.f23 <= 1 || Char.f10 == 1 || Char.direction == Opp.direction || Opp.f10 == 0) return;
	if (Opp.action >= 2 || Opp.charid == 0xB || Opp.charid == 7 || Opp.charid == 8) return;
	if (Opp.charid == 0xC && Opp.frame >= 0xB3 && Opp.frame <= 0xEB) return;
	if (Opp.charid == 4 && Opp.frame == 0xCE) return;
	if (Opp.charid == 2 && Opp.f19 == 0x6E) return;
	if (Char.alive >= 0) return;
	int d = opp_distance(); if (d < 0) d = -d; if (d >= 16) return;
	char_y_to_floor(); Char.fall_y = 0;
	if (Char.frame > 0xF5 && Char.frame < 0x106) { Char.x = char_dx_forward(-6); return; }
	seqtbl_offset_char(0x2F); play_seq();
}

/* everything play_kid_frame runs after play_seq when a frame was produced (169B:06FB..0771) */
void kid_post_move(void)
{
	fall_accel(); fall_speed(); load_frame_to_obj(); load_fram_det_col(); set_char_collision();
	check_opp_bump(); check_collisions(); check_bumped(); check_gate_push();
	check_fall_or_ceiling(); check_tile_effects();
	if (level_kind == 2 || level_kind == 3 || level_kind == 4) level_kind_hooks();
	if (knock != 0) { shake_loose_row(Char.curr_row - (knock > 0), Char.room); knock = 0; }   /* 169B:0E50 */
}

/* 169B:0692: returns -1 quit, 0 moved, 1 frozen */
int play_kid_frame(void)
{
	int r;
	loadkid();
	if (char_out_of_level()) { r = play_kid_control(); char_fell_out(); goto done; }
	int8_t o = (int8_t)Kid.opp_index;
	if (o == -1) { o = find_opponent(1); if (o == -1) o = 0; }
	load_opp_080a(o);
	load_fram_det_col();
	if (play_kid_control() == -1) r = -1;
	else if (word_5cd8 == 0) { play_seq(); if (Char.frame != 0) kid_post_move(); r = 0; }
	else r = 1;
	if ((int8_t)byte_5cbb >= 0) {   /* 169B:0798: the level's entrance sound after the first ticks */
		if (byte_5cbb == 0 && Kid.alive < 0 && Kid.action != 3 && Kid.action != 4) play_sound(0x1A);
		byte_5cbb--;
	}
done:
	Kid = Char; return r;
}

/* 0AFF:1258: death counter, decisions, then the shared control state machine */
void char_control_step(void)
{
	if (Char.alive < 0) { if (Char.f12 == 0) Char.alive = 0; }
	else if (Char.alive < 6) { if (Char.f0f != 0) { if (Char.alive < 0x14) Char.alive++; else Char.f0f = 0; } }
	else { dead_char_sound_1611(); if (Char.index == Kid.opp_index) Kid.opp_index = 0xFF; }
	autocontrol();
	if (word_2ba8 != 0) ovl_15db_64();
	if (Char.charid != 1 && Char.charid != 12) control();
}
/* 169B:07EC: every character of the drawn room */
void play_all_chars(void)
{
	if (drawn_room == 0) return;
	int8_t n = room_nchars(drawn_room);
	for (int8_t i = 0; i < n; i++) {
		load_char(i);
		if ((uint8_t)Char.direction == 0x56 || char_out_of_level() || (Char.y > 0xFE && level_links(Char.room)[3] == 0)) char_fell_out();
		else {
			load_char(i); Opp = Kid;                                 /* 0AFF:0878 */
			load_fram_det_col();
			char_control_step();
			uint8_t dr = drawn_room;
			if (drawn_room != Char.room) { drawn_room = Char.room; set_neighbour_rooms(); }
			play_seq();
			if (Char.charid == 2) guard_after_seq();
			if ((Char.x > 0x21 && Char.x < 0x222) || (Char.curr_row != 0 && level_number == 5 && (Char.room == 10 || Char.room == 7 || Char.room == 12))
			    || ((Char.room == 7 || Char.room == 8) && level_kind == 6)) {
				fall_accel(); fall_speed(); load_frame_to_obj(); load_fram_det_col(); set_char_collision();
				if (Char.action == 9) Char.curr_row = y_to_row(Char.y);
				else {
					check_guard_bumped(); check_gate_guard(); check_fall_or_ceiling(); check_tile_effects();
					if (level_kind == 2 || level_kind == 3 || level_kind == 4 || level_kind == 6) level_kind_hooks_char();
				}
			}
			if (Char.charid == 12) ovl_37d28();
			if (Char.room != dr) { drawn_room = dr; set_neighbour_rooms(); }
		}
		if (level_kind == 6 && room_nchars(drawn_room) != n) { n--; i--; } else save_char();
	}
	for (int8_t i = 0; i < n; i++) {
		if ((uint8_t)chars[i].direction != 0x56) continue;
		load_char(i); remove_record_pub(i, drawn_room);
		if ((int8_t)Char.index < (int8_t)Kid.opp_index) Kid.opp_index--;
		else if (Char.index == Kid.opp_index) Kid.opp_index = find_opponent(Char.direction);
		i--; n--;
	}
	word_6140 = 0;
}
void land_adjust_pub(void) { land_adjust(); }
