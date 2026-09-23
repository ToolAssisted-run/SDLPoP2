/* Kid control (OVL01 2FDF:048C and its handlers). ctrl1_* are the facing-relative edge flags
 * (-1 = newly pressed, 0 = not pressed, 1 = consumed). seqtbl_offset_char(id) starts a sequence. */
#include "types.h"
#include "globals.h"

/* 0AFF:1010 / 0FFC: 32-pixel tiles; xl = (x-18) % 32 kept in DS:94B0 (obj_xl) */
int8_t obj_xl;
int8_t col_from_x18(int16_t x18)
{
	int16_t v = x18 - 4; obj_xl = (int8_t)(v % 32);
	int16_t col = (v < 0 ? -((-v) >> 5) : v >> 5) - 4;
	if (x18 < 0) col--;
	return (int8_t)col;
}
int8_t x_to_col(int16_t x) { return col_from_x18(x - 14); }
/* 0AFF:0FB0 / 0FC4 / 0FCE */
int16_t dx_weight(void) { return char_dx_forward((int8_t)(cur_frame.dx - (cur_frame.flags & 0x1F))); }
int16_t distance_to_edge(int16_t x) { x_to_col(x); return Char.direction == 0 ? 31 - obj_xl : obj_xl; }
int16_t distance_to_edge_weight(void) { return distance_to_edge(dx_weight()); }

/* 0AFF:1376 */
int control_rest(void) { ctrl1_down = ctrl1_up = ctrl1_forward = ctrl1_backward = 0; return 1; }

/* 2FDF:0E2E (030c1e): crouch */
void kid_crouch_pub(void);
static void kid_crouch(void)
{
	int id = Char.charid == 1 ? shadow_seq_2f86a() : -1;
	if (id == -1) id = 50;                       /* seq_50_crouch */
	seqtbl_offset_char(id);
	ctrl1_down = control_rest();
}

/* 2FDF:0C90 (030a80): down pressed while standing: crouch, or climb down when at an edge */
void control_standing_down(void)
{
	ctrl1_down = 1;
	if (tile_is_empty_kind(get_tile_infrontof(1)) && distance_to_edge_weight() < 3) { Char.x = char_dx_forward(10); load_fram_det_col(); return; }   /* at the edge: step off */
	if (!tile_is_empty_kind(get_tile_behind_char())) { kid_crouch(); return; }
	if (distance_to_edge_weight() < 8) { kid_crouch(); return; }
	uint8_t front = get_tile_behind_char(); uint16_t front_mod = curr_modifier;
	uint8_t here = get_tile_at_char();
	if (!can_climb_down_146e(curr_modifier, front_mod, here, front)) { kid_crouch(); return; }
	if (Char.direction != 0 && get_tile_at_char() == 4 && (curr_modifier & 0xFFFC) < 0x18) { kid_crouch(); return; }
	int16_t d = distance_to_edge_weight();
	Char.x = char_dx_forward(d - (Char.direction == 0 ? 0x17 : 0x13));
	seqtbl_offset_char(0x44);                    /* climb down (seq 68) */
}

/* 2FDF:1376 (031596): running jump (kind 4) / (kind 100): look two tiles ahead for a ledge to adjust x */
int control_runjump(int kind)
{
	if (!((kind == 4 && (Char.frame == 7 || Char.frame == 11)) || (kind == 100 && (Char.frame == 0xC0 || Char.frame == 0xC4)))) return 0;
	int16_t xx = char_dx_forward(9);
	int8_t col = x_to_col(xx); int n = 0, ok = 1, two = 0;
	do {
		col += dir_front[Char.direction + 1];
		uint8_t t = get_tile(Char.curr_row, col, Char.room);
		if (!tile_passable_2f800(curr_modifier, t) && t != 11) ok = 0;
		else if (++n == 2) two = 1;
	} while (ok && !two);
	if (!ok) {
		int16_t d = distance_to_edge(xx) + n * 32 - (kind == 4 ? 0x1F : 0x23);
		if (d < -20 || d > 9) { if (d < 0 && kind == 4) return 0; d = -9; }
		Char.x = char_dx_forward(d + 9);
	}
	seqtbl_offset_char(kind);
	ctrl1_up = control_rest();
	return 1;
}

/* 2FDF:19D4 (0317c4, Ctrl while standing, the prince or the spirit): draw the sword (seq 0x37, sound 0x13), first
 * stepping back from an edge or a wall in front (a temple wall, DS 1375:14FC) so the stance fits; -1 when there is no
 * room behind either. Level 14: not in rooms 1/2; the spirit in rooms 7/8 casts instead (33FD:1E4A). */
int sword_seq_0317c4(void)
{
	int r = 0x37;
	if ((Char.frame >= 0xF6 && Char.frame <= 0x105) || ((Char.room == 1 || Char.room == 2) && level_number == 14)) r = -1;
	else if (Char.charid == 1 && (Char.room == 7 || Char.room == 8) && level_kind == 6) {
		r = spirit_cast();
		int8_t i = 0;
		if (chars[0].charid != 0) while (i < 5) { i++; if (i < 5 && chars[i].charid == 0) break; }
		Char.f10 = i < 5 ? chars[i].f10 : 0;   /* the body's (index 5 would read past chars[]) */
	} else {
		int16_t si = -1, di = level_kind == 2 ? ovl_352ca() : 0;   /* 1375:14FC */
		if (di) { int c = x_to_col(di) - Char.curr_col; if (c < 0) c = -c; if (c > 1) di = 0; }
		uint8_t t = Char.direction == 0 ? get_tile_at_char() : 0;
		if (t == 4 && can_bump_into_gate()) si = -0x20;
		else if (di && ((Char.direction == -1 && Char.x >= di) || (Char.direction == 0 && Char.x <= di))) {
			int16_t w = distance_to_edge_weight(), d = di - Char.x; if (d < 0) d = -d; d -= w;
			si = (d + 1 == 0 ? -2 : d) - 4;
		} else {
			t = get_tile_infrontof(1);
			if (!tile_is_floor(t) || t == 0xB || (t == 4 && can_bump_into_gate())) si = 0;
		}
		if (si != -1) {
			t = get_tile_behind_char();
			int bad = !tile_is_floor(t) || t == 0xB || (t == 4 && can_bump_into_gate());
			if (!bad && di && !((Char.direction == -1 && Char.x >= di) || (Char.direction == 0 && di >= Char.x))) bad = 1;
			if (!bad && Char.f24 == 4) bad = 1;
			if (bad) { Char.f10 = 0; r = -1; }
			else {
				int16_t e = distance_to_edge_weight() + si;
				if (Char.direction == 0) { if (e < 0x15) Char.x = char_dx_forward(e - 0x15); }
				else if (Char.direction == -1) { if (e < 0xF) Char.x = char_dx_forward(e - 0xF); }
			}
		}
	}
	if (r != -1 && r != 0xF2) play_sound(0x13);
	else if (Char.f10 != 0xFF) Char.f10 = 0;
	return r;
}
/* 2FDF:1544 (031764): shift alone while standing: draw/put away sword (charid dependent) */
void control_standing_shift(void)
{
	ctrl1_forward = control_rest(); ctrl1_shift = 2; Char.f10 = 1;
	int id = -1;
	if (Char.charid == 0 || Char.charid == 1) id = sword_seq_0317c4();
	else if (Char.charid == 7 || Char.charid == 8) id = -1;
	else id = 0x5A;
	if (id != -1) seqtbl_offset_char(id);
}

/* 2FDF:0AF6 (030d16) */
void control_running(void)
{
	if (ctrl1_forward == 0 && ctrl1_backward == 0 && (Char.frame == 7 || Char.frame == 11)) { ctrl1_forward = control_rest(); seqtbl_offset_char(13); }
	else if (ctrl1_backward != 0) { ctrl1_backward = control_rest(); seqtbl_offset_char(6); }
	else if (ctrl1_up != 0) { if (ctrl1_up < 0) control_runjump(4); }
	else if (ctrl1_down < 0) { ctrl1_down = control_rest(); seqtbl_offset_char(26); }
	if (ctrl1_forward < 0) ctrl1_forward = 1;
}
/* 2FDF:09AA (030bca) */
void control_turning(void)
{
	if (Char.index == 10 && Char.f19 == 5) ovl_2f86_0a5c();
	if (Char.frame == 48 && ctrl1_shift >= 0 && ctrl1_forward != 0 && ctrl1_up == 0 && ctrl1_down == 0) {
		seqtbl_offset_char(43);
		if (Char.charid == 0 && Char.pal_slot != 0) Char.pal_slot = 0;   /* 2FDF:0E18 */
	}
}
/* 2FDF:0E18 (031038) / 0E36 (031056) */
void control_start_run(void) { if (ctrl1_up < 0 && ctrl1_forward < 0) { ctrl1_forward = control_rest(); ctrl1_up = ctrl1_forward; control_jump_031062(); } }
void control_jumpup(void)    { if (ctrl1_forward != 0) control_jump_031062(); }

/* 2FDF:048C */
void control(void)
{
	uint16_t frame = Char.frame;
	if (Char.alive >= 0) {
		if (Char.charid != 0 && Char.index == Kid.opp_index) Kid.opp_index = find_opponent(1);   /* 2FDF:0655 */
		if (Char.charid == 1) { if (Char.alive > 6) shadow_2fba4(); }
		else if (Char.room == 3 && level_kind == 6 && Char.charid == 2) ovl_34024();
		return;
	}
	if (Char.action == 5 || Char.action == 4 || Char.action == 9 || Char.f19 == 0x1B || Char.f19 == 0x6E) {
		control_rest();
		if (level_number == 12 && chars[0].charid == 12 && chars[0].direction != 0x56 && chars[0].x < word_3bf62) seqtbl_offset_char(0xD4);
		return;
	}
	if (Char.f10 == 1) { control_2fdf_1bfa(); return; }
	if (Char.charid >= 2) { control_by_charid_cc1e(); return; }
	if (frame == 15 || (frame >= 50 && frame <= 52)) { control_standing(); return; }
	if (frame >= 45 && frame <= 49) { control_turning(); return; }
	if (frame >= 1 && frame <= 3) { control_start_run(); return; }
	if (frame >= 67 && frame <= 69) { control_jumpup(); return; }
	if (frame < 15) { control_running(); return; }
	if (frame >= 87 && frame <= 99) { if (level_kind != 1) control_hanging(); return; }
	if (frame == 81 && Char.f19 == 0x44) { control_frame81_0313c6(); return; }
	if (frame == 109) { control_crouched(); return; }
	if ((frame >= 0xF6 && frame <= 0x105) || (frame >= 0x100 && frame <= 0x107)) { control_with_sword(); return; }
	if (frame >= 0xD9 && frame <= 0xE2) { control_0d9_0e2(); return; }
	if (is_dead_frame(frame) || (frame > 0xB2 && frame < 0xB8)) { control_dead_0307a2(); return; }
	if (frame == 0x127) { ovl_383fa(); return; }
	if (frame >= 0x110 && frame <= 0x119) { ovl_35f5a(); return; }
	if (frame == 44 || frame == 26) control_rest();
}

/* 2FDF:068E (03047e): with the sword drawn (frames 0xF6..0x105) */
void control_with_sword(void)
{
	if (Char.frame < 0xF6 || Char.frame > 0x105) return;
	if (level_kind == 4) ovl_35a88();
	uint8_t here = get_tile(Char.curr_row, Char.curr_col, Char.room);
	if (here != 7 && here != 12 && here != 13 && get_tile_infrontof(1) != 7 && get_tile_infrontof(1) != 12 && get_tile_infrontof(1) != 13
	    && get_tile_behind_char() != 7 && get_tile_behind_char() != 12 && get_tile_behind_char() != 13 && Char.f19 != 0x7C) {
		seqtbl_offset_char(0x7C); return;
	}
	here = get_tile(Char.curr_row, Char.curr_col, Char.room);
	if (Char.frame != 0xFB) { control_rest(); return; }
	if (level_kind == 4) ovl_35a88();
	if (ctrl1_up == 0 || here == 7) {
		if (ctrl1_forward < 0 && Char.f0f != 0) { control_rest(); seqtbl_offset_char(0x7A); }
		else if (ctrl1_backward < 0) { control_rest(); seqtbl_offset_char(0x7B); }
	} else {
		uint8_t front = get_tile_behind_char(); int16_t d;
		if (front == 7) d = Char.direction == 0 ? (Char.x - col_x_left[Char.curr_col]) - 0x26 : (col_x_right[Char.curr_col] - Char.x) - 10;
		else if (front == 12 || front == 13) d = ovl_34350() == 0 ? 0x20 : -0x20;
		else d = 0x20;
		if (d >= 0) { seqtbl_offset_char(0x7C); ctrl1_up = control_rest(); if (d < 6) Char.x = char_dx_forward(-(d - 6)); }
	}
	if (ctrl1_forward == 0 && Char.f0f == 0) Char.f0f = 1;
}

/* 2FDF:1530 (031320): climb up from hanging (up pressed) */
void control_hanging_climb(void)
{
	int id;
	if (level_number == 5 && Char.room == 10 && Char.f10 != (uint8_t)-1) { id = 0xEC; Char.f10 = (uint8_t)-1; play_sound(0x13); }
	else id = 10;                                 /* seq_10_climb_up */
	ctrl1_up = control_rest(); ctrl1_shift = ctrl1_up;
	uint8_t above = get_tile_above_char();
	if (Char.charid == 1 || above != 4 || Char.direction != -1 || (curr_modifier & 0xFFFC) > 0x17) {
		if (level_kind == 2 && ovl_35240(0)) id = 0x49;
		else if (level_kind == 5 && ovl_34ab2()) id = 0x3B;
	} else id = 0x49;
	seqtbl_offset_char(id);
	if (Char.direction == 0 || level_kind != 3) Char.x = char_dx_forward(-2);
}

/* 2FDF:16F6 (0314e6): would the next sequence item lower the frame number? (peek without side effects on the sequence) */
int seq_peek_frame_decreases(void)
{
	uint16_t f19 = Char.f19, id = Char.seq_id, pos = Char.seq_pos; uint16_t frame = Char.frame;
	play_seq();
	Char.seq_id = id; Char.seq_pos = pos; Char.f19 = f19;
	return Char.frame <= frame;
}

/* 2FDF:08A0 (030630): crouched (frame 109) */
void control_crouched(void)
{
	int id;
	if (level_kind == 4) ovl_35a88();
	if (level_number == 5 && Char.room == 3 && Char.f19 != 0x11 && Char.f19 != 0x14) {
		if (get_tile_at_char() == 0x12 || get_tile(Char.curr_row, Char.curr_col - 1, Char.room) == 0x12) { ovl_384e8(); return; }
	}
	if (Char.f19 == 0x6F || control_sword_check_030e3c()) { ctrl1_shift = control_rest(); return; }
	id = -1;
	if (ctrl1_down == 0) {
		if (!ovl_32a0e()) {
			if (word_6d46 != 0 && ovl_34350()) goto forward;
			id = 0x31;                            /* seq 49: stand up from crouch */
		} else {
		forward:
			if (ctrl1_forward >= 0) goto done;
		}
	} else {
		if (ctrl1_forward >= 0) goto done;
		ctrl1_forward = control_rest();
		uint8_t t = get_tile_infrontof(1);
		if ((t == 7 || t == 12 || t == 13) && (t != 7 || (curr_modifier & 3) != 3)) {
			int8_t c = tile_col_in_drawn_room(); int16_t d;
			if (Char.direction == 0) { d = col_x_left[c] - Char.x; d = (d != -6 && -d > 5) ? d + 6 : 0; }
			else if (Char.direction == -1) { d = Char.x - col_x_right[c]; d = d < 0x16 ? d - 0x16 : 0; }
			else d = 0;
			if (d) Char.x = char_dx_forward(d);
			id = 0x7D;
			goto done;
		}
		id = 0x4F;                                /* seq 79: crouch hop */
	}
done:
	if (ctrl1_down != 0) ctrl1_down = control_rest();
	if (id != -1) seqtbl_offset_char(id);
}

/* 2FDF:14EA (03127a): hanging (frames 87..99) */
void control_hanging(void)
{
	if (Char.alive < 0) {
		if (word_8a84 == 0 && ctrl1_up != 0) { control_hanging_climb(); return; }
		if (ctrl1_shift == 0 || Char.f24 == 0xB) { Char.f24 = 0; control_frame81_0313c6(); return; }
		uint8_t t = get_tile_at_char();
		if (Char.action != 6 && tile_is_wall_kind(t)) { seqtbl_offset_char(0x19); return; }
		if ((Char.direction == -1 && t == 4) || (level_kind == 5 && Char.room == 15)) { seqtbl_offset_char(0x19); return; }
		if (!tile_is_empty_kind(get_tile_above_char())) return;
		if (level_kind == 5 && ovl_34ab2()) return;
	}
	control_frame81_0313c6();
}

/* 2FDF:15D6 (0313c6): release the ledge: pick the landing/fall sequence from the tiles around */
void control_frame81_0313c6(void)
{
	ctrl1_down = control_rest();
	uint8_t front = get_tile_behind_char(), here = get_tile_at_char(); int id, dx = 0;
	if (tile_is_empty_kind(front) && tile_is_empty_kind(here)) { id = seq_peek_frame_decreases() ? 99 : 0x17; seqtbl_offset_char(id); return; }
	int here_solid = tile_is_solid_floor(here), front_solid = tile_is_solid_floor(front);
	if (!here_solid && !front_solid) {
		if (tile_is_floor(front) && tile_is_wall_kind(here)) front_solid = 1;
		else if (tile_is_floor(here) && tile_is_wall_kind(front)) here_solid = 1;
	}
	if (!tile_is_wall_kind(here)) {
		if (!here_solid && front_solid) { dx = -8; id = 0xB; }
		else if (here_solid && !front_solid) { dx = 6; id = 0xB; }
		else id = (here_solid && front_solid) ? 0xB : 0x17;
	} else { dx = -14; id = front_solid ? 0xB : 0x17; }
	if (dx) Char.x = char_dx_forward(dx);
	seqtbl_offset_char(id);
}

/* 2FDF:0E72 (031062): standing jump (forward + up) */
void control_jump_031062(void) { ctrl1_forward = 1; ctrl1_up = 1; seqtbl_offset_char(3); }

/* 2FDF:0A5A (030c4a): turn around (backward pressed) */
void control_standing_turn(void)
{
	ctrl1_backward = control_rest(); Char.f0f = 1;
	int16_t d = distance_to_edge_weight();
	if (get_tile_infrontof(1) == 4 && gate_blocks_0329b6()) {
		int16_t lim = level_kind == 3 ? 7 : (drawn_room == 9 && level_number == 8) ? -100 : 4;
		if (d < lim) Char.x = char_dx_forward(d - lim);
	}
	if (level_kind == 2 || (level_kind == 6 && ((Char.room != 1 && Char.room != 2) || level_number != 14))) ovl_2f86_08d8();
	if (Char.f19 != 0x47) seqtbl_offset_char(5);
}

/* 2FDF:0B9C (030d8c): safe step towards an edge; dist 0 = compute it */
int control_standing_step(int dist)
{
	byte_2ab4 = 1; int r = 1; ctrl1_forward = 1; ctrl1_shift = 1; int id;
	if (dist == 0) { dist = get_edge_distance(); r = dist; }
	if (edge_type == 1) dist--;
	if (dist < 1) {
		if (Char.f0f != 0 && (edge_type != 1 || curr_tile == 12 || curr_tile == 13)) { Char.f0f = 0; id = 0x2C; goto set; }
		id = 0x2A;
	} else {
		Char.f0f = 1;
		if (dist < 0x1C) {
			id = dist == 1 ? -1 : dist / 2 + 0x1C; r = dist / 2;
			if (dist % 2) { Char.x = char_dx_forward(1); r = Char.x; }
			goto set;
		}
		id = 0x2A;
	}
set:
	if (id != -1) seqtbl_offset_char(id);
	byte_2ab4 = 0; return r;
}

/* 2FDF:0AF0 (030ce0): forward pressed while standing: run, or a step when close to an edge */
void control_standing_forward(void)
{
	int d = get_edge_distance();
	if (edge_type == 1 && d < 0x12) { if (ctrl1_forward < 0) control_standing_step(0); }
	else seqtbl_offset_char(1);
	ctrl1_forward = control_rest();
}

/* 2FDF:1740 (031530): grab the ledge above: seq 8 (straight up) at the edge, else seq 0x18 (jump up and grab) */
static void jumpup_grab(int from_jump)
{
	int16_t d = distance_to_edge_weight();
	if (from_jump && d == 0x20) d = 0;
	if (d < 4) { int e = get_edge_distance(); if (e < 4 && edge_type != 1) { Char.x = char_dx_forward(d); seqtbl_offset_char(8); return; } }
	Char.x = char_dx_forward(d - (Char.direction == -1 ? 7 : 9)); seqtbl_offset_char(0x18);
}
/* 2FDF:13BA (0311aa): plain jump up; seq 0xE when something is above the head, else 0x1C */
static void jumpup_plain(void)
{
	ctrl1_up = control_rest();
	int e = get_edge_distance(); if (e < 4 && edge_type == 1) Char.x = char_dx_forward(e - 6);
	uint8_t t = get_tile(Char.curr_row - 1, col_from_x18(dx_weight() - 6), Char.room);
	int id = 0x1C;
	if (!tile_is_empty_kind(t) && (level_kind != 5 || Char.room != 0xF)
	    && (Char.room != 7 || level_kind != 6 || (Char.curr_row == 2 && (Char.curr_col == 2 || Char.curr_col == 6)))
	    && (Char.room != 8 || level_kind != 6 || (Char.curr_row == 2 && Char.curr_col != 3 && Char.curr_col != 4))) id = 0xE;
	seqtbl_offset_char(id);
}
/* 2FDF:139A (03118a) */
static void jumpup_step_back_grab(void) { get_tile_above_char(); Char.x = char_dx_forward(distance_to_edge_weight() - 0x18); seqtbl_offset_char(0x10); }
/* 2FDF:1348 (031138): the ledge is above and behind */
static void jumpup_behind(void)
{
	int16_t d = distance_to_edge_weight(); if (level_kind == 2) ovl_352ca();
	if (d < 0x10) { jumpup_plain(); return; }
	if (tile_is_empty_kind(get_tile_behind_char())) { jumpup_step_back_grab(); return; }
	Char.x = char_dx_forward(d - 0x20); load_fram_det_col(); jumpup_grab(1);
}
/* 2FDF:1284 (031074): up while standing: grab the ledge above (in front, then behind) or jump up */
void control_jumpup_grab_031074(void)
{
	ctrl1_up = control_rest();
	uint8_t above = get_tile_above_char(); uint16_t m_above = curr_modifier;
	uint8_t front = get_tile_above_front(); if (front == 4) curr_modifier = 200;
	int16_t lim = level_kind == 2 ? ovl_352ca() : 0;
	if (lim != 0) { int dl = lim - Char.x; if (dl < 0) dl = -dl; if (dl > 0x20) lim = 0; }
	if (lim == 0 && can_climb_down_146e(curr_modifier, m_above, front, above)) { jumpup_grab(0); return; }
	uint8_t behind = get_tile_above_behind(); uint16_t m_behind = curr_modifier;
	above = get_tile_above_char(); if (above == 4) curr_modifier = 200;
	if (lim == 0 && can_climb_down_146e(curr_modifier, m_behind, above, behind)) { jumpup_behind(); return; }
	jumpup_plain();
}

/* 1375:0CFA: the level door at curr_tile is open */
static int level_door_open(void) { return (uint8_t)curr_modifier >= 0x2A; }
/* 2FDF:0D62: step into the open level door (facing left, lined up with it) */
static void enter_level_door(void) { Char.x = col_x_left[tile_col - 1] + 0x1E; Char.direction = -1; seqtbl_offset_char(0x46); }
/* 2FDF:0C42 (030a32): up pressed while standing */
void control_standing_up(void)
{
	if (level.start_room != drawn_room && Char.charid == 0
	    && (get_tile_at_char() == 0x11 || get_tile_behind_char() == 0x11 || get_tile_infrontof(1) == 0x11)
	    && level_door_open()) { enter_level_door(); return; }
	if (ctrl1_forward != 0) { control_jump_031062(); return; }
	control_jumpup_grab_031074();
}

/* 2FDF:0684 (030874): standing (frames 15, 50..52); every branch ends in the tail below (2FDF:0C23) */
static void control_standing_branches(void)
{
	if (level_kind == 2 || level_kind == 6) ovl_2f86_0a5c();
	if (ctrl1_shift == -1 && control_sword_check_030e3c()) return;
	if (Char.index != 10) {
		if (ctrl1_down < 0 && ctrl1_forward < 0) { control_standing_shift(); return; }
	} else {
		if ((Char.f10 != (uint8_t)-1 || (level_kind == 6 && Char.charid == 1)) && ctrl1_shift == -2
		    && ctrl1_forward == 0 && ctrl1_backward == 0 && ctrl1_up == 0 && ctrl1_down == 0) {
			ctrl1_shift = 2; control_rest(); control_standing_shift();
			Char.opp_index = find_opponent(Char.direction); if (Char.opp_index == (uint8_t)-1) Char.opp_index = 0; return;
		}
		if (Opp.f23 > 1 && Char.charid != 1 && Char.f24 != 0xD && Opp.charid != 6) {
			int d = opp_distance();
			if (d > -11 && d < 0xD0 && d < 0 && d > -15) { control_standing_turn(); return; }
		}
	}
	if (ctrl1_shift == -1 || ctrl1_shift == 1) {
		if (ctrl1_backward < 0) { control_standing_turn(); return; }
		if (ctrl1_up < 0) { control_standing_up(); return; }
		if (ctrl1_down < 0) { control_standing_down(); return; }
		if (ctrl1_forward < 0 && ctrl1_up == 0 && ctrl1_down == 0) { control_standing_step(0); return; }
		if (ctrl1_forward != 0) return;
	}
	if (ctrl1_forward < 0) {
		if (ctrl1_up >= 0) { control_standing_forward(); return; }
	} else {
		if (ctrl1_backward < 0) { control_standing_turn(); return; }
		if (ctrl1_up >= 0) {
			if (ctrl1_down < 0) { control_standing_down(); return; }
			if (ctrl1_forward == 0) return;
			control_standing_forward(); return;
		}
		if (ctrl1_forward >= 0) { control_standing_up(); return; }
	}
	control_jump_031062();
}
void control_standing(void)
{
	control_standing_branches();
	if (Char.f19 != 5 && Char.charid == 0 && Char.pal_slot != 0) Char.pal_slot = 0;   /* no longer turning: the flash ends */
}

/* ---- sword drawn (Char.f10 == 1): OVL01 2FDF:1BFA and helpers ---- */
/* 2FDF:1CB2 (031ea2): strike */
static void sword_strike(void)
{
	if (Char.charid == 7 || Char.charid == 8) return;
	int id;
	if (Char.frame != 0x9D && Char.frame != 0x9E && Char.frame != 0xAA && Char.frame != 0xAB && Char.frame != 0xA5)
		id = (Char.frame == 0x96 || Char.frame == 0xA1) ? 0x42 : -1;
	else if (Char.charid < 2) id = Opp.charid == 0xB ? 0x60 : 0x4B;
	else if (Char.charid == 10) id = 0x6D;
	else id = 0x3A;
	if (id != -1) { seqtbl_offset_char(id); ctrl1_shift = 2; control_rest(); }
}
/* 2FDF:1D3E (031f2e): parry */
static void sword_parry(void)
{
	if (Char.charid == 7 || Char.charid == 8) return;
	int id = 0x3E, replay = 0;
	if (Char.frame == 0x9E || Char.frame == 0xAA || Char.frame == 0xAB || Char.frame == 0xA8 || Char.frame == 0xA5) {
		if (Char.charid == 0 || Char.charid == 1) {
			if (Opp.frame != 0xA8) { if (Opp.frame != 0x97 && Opp.frame != 0x98 && Opp.frame != 0xA2 && Opp.frame == 0x99) replay = 1; }
			else id = -1;
		} else {
			if (opp_distance() < 0x33) { if (Opp.frame != 0x98) id = -1; }
			else { sword_retreat(); id = -1; }
		}
	} else if (Char.frame == 0xA7) id = 0x3D;
	else id = -1;
	if (id != -1) { ctrl1_up = 1; seqtbl_offset_char(id); if (replay) play_seq(); }
}
/* 2FDF:1516 (031706): advance */
static void sword_advance(void)
{
	if (Char.charid == 7 || Char.charid == 8 || Char.charid == 0xB) return;
	if (Char.frame != 0x9E && Char.frame != 0xAA && Char.frame != 0xAB) return;
	seqtbl_offset_char(Char.charid <= 1 ? 0x38 : Char.charid == 10 ? 0x6C : 0x56);
	ctrl1_forward = control_rest();
}
/* 2FDF:14CC (0316bc): retreat */
void sword_retreat(void)
{
	if (Char.charid == 7 || Char.charid == 8 || Char.charid == 0xB) return;
	if (Char.frame != 0x9E && Char.frame != 0xAA && Char.frame != 0xAB) return;
	seqtbl_offset_char(Char.charid == 10 ? 0x68 : 0x39);
	ctrl1_backward = control_rest();
}
/* 2FDF:1B6A (031d5a): put the sword away */
static void sword_sheathe(void)
{
	Char.f10 = 0; ctrl1_down = control_rest(); int id;
	if (Char.charid == 0 || Char.charid == 1) { word_922e = 9; id = char_scan_31bc4() ? 0x5D : 0x5C; }
	else if (Char.charid == 10) id = 0x5A;
	else id = 0x4D;
	seqtbl_offset_char(id);
}
/* 2FDF:1C20 (031e10): kid steps toward the opponent before engaging */
static void sword_kid_engage(void)
{
	int d = -1; uint8_t t = get_tile_infrontof(1);
	if (!tile_is_floor(t) || (t == 4 && gate_blocks_0329b6())) d = 0;
	else { t = get_tile_infrontof(2); if (!tile_is_floor(t) || (t == 4 && gate_blocks_0329b6())) d = 0x20; }
	if (Char.f19 == 0x5E) d += 0x12;
	if (d != -1) { int e = distance_to_edge_weight(); if (e + d < 0x38) Char.x = char_dx_forward(e + d - 0x38); }
	seqtbl_offset_char(0x7F);
}
/* 2FDF:1BB6 (031da6): engage / turn to face */
static void sword_engage(void)
{
	if (Char.charid == 7 || Char.charid == 8) { if (Char.f19 == 0x9C || Char.f19 == 0xA6 || ovl_377c6()) return; seqtbl_offset_char(0x9C); return; }
	if (Char.charid == 0 || Char.charid == 1) { sword_kid_engage(); return; }
	if (Char.charid == 10 && Char.frame > 0xD4 && Char.frame < 0xDA) return;
	seqtbl_offset_char(0x3C);
}
/* 2FDF:1AB6 (031ca6): fighting actions from the controls */
static void sword_actions(void)
{
	if (Char.charid == 7 || Char.charid == 8) { ovl_3741a(); return; }
	if (Char.frame == 0xA1 && ctrl1_shift >= 0) { seqtbl_offset_char(0x39); return; }
	if (ctrl1_shift == -2) { if (Char.charid == 0 || Char.charid == 1) word_68ec = 0xF; sword_strike();   /* DS:68EC: guards hold back */ return; }
	if (ctrl1_down < 0) { if (Char.frame == 0x9E || Char.frame == 0xAA || Char.frame == 0xAB) sword_sheathe(); return; }
	if (ctrl1_up < 0) { sword_parry(); return; }
	if (ctrl1_forward < 0) { sword_advance(); return; }
	if (ctrl1_backward < 0 && ctrl1_shift == 0) sword_retreat();
}
/* 2FDF:1BFA */
#ifdef CTL_DEBUG
#include <stdio.h>
#define DBG(...) fprintf(stderr, __VA_ARGS__)
#else
#define DBG(...)
#endif
void control_2fdf_1bfa(void)
{
	DBG("1bfa: index %u action %u f12 %u f14 %d f10 %u\n", Char.index, Char.action, Char.f12, Char.hp_delta, Char.f10);
	if (Char.index == 10) {
		Kid = Char; int8_t n = -1;
		if (Char.opp_index != (uint8_t)-1 && Char.opp_index < room_nchars(Char.room)) { load_opp_080a(Char.opp_index); if ((int8_t)Char.f12 > 0) n = Char.opp_index; }
		if (n == -1) { n = find_opponent(1); if (n == -1) n = 0; else Char.opp_index = n; }
		Char.opp_index = n == -1 ? Char.opp_index : n; load_opp_080a(n);
	}
	DBG("1bfa: after opp: index %u opp_index %u\n", Char.index, Char.opp_index);
	if (Char.action > 1) return;
	{ int hd = Char.hp_delta; if (hd < 0) hd = -hd; if ((int)Char.f12 <= hd) return; }
	int fall_through = 1;
	uint8_t t = get_tile_at_char();
	DBG("1bfa: tile %u opp.f23 %u dist %d charid %u\n", t, Opp.f23, opp_distance(), Char.charid);
	if ((t == 0xB || t == 0xF) || Opp.f23 >= 2) {
		int d = opp_distance();
		if (d < -10 || d > 0xCF) {
			if (d >= 0) goto tail;
			if (d > -5 || (d > -0x1F && (Opp.charid == 7 || Opp.charid == 8))) { sword_actions(); fall_through = 0; goto tail; }
			if (Char.charid != 0xB) {
				if ((Char.index == 10 && word_8604 == 0) || Opp.charid == 6
				    || (Char.curr_row != 0 && level_number == 5 && (Char.room == 10 || Char.room == 7 || Char.room == 12) && rtlink_0dd5())) { sword_actions(); fall_through = 0; goto tail; }
				sword_engage();
			}
		} else { sword_actions(); }
		fall_through = 0;
	}
tail:
	if (Char.index == 10 && ctrl1_backward < 0 && (ctrl1_shift == -1 || ctrl1_shift == 1) && (level_number != 5 || Char.room != 10 || word_927e < 1)) {
		sword_engage(); ctrl1_shift = 2; ctrl1_backward = control_rest(); word_8604 = word_8604 == 0;
		Char.opp_index = find_opponent(~Char.direction); return;
	}
	if (fall_through) { if (Char.index == 10 && ctrl1_down < 0) { sword_sheathe(); return; } sword_actions(); }
}

/* 0AFF:1C2E: control for charids 2.. (guards): frame 0xA6 with down pressed */
void control_by_charid_cc1e(void)
{
	if (Char.frame != 0xA6 || ctrl1_down >= 0) return;
	if (ctrl1_forward < 0) {
		uint8_t t = get_tile_behind_char(); uint16_t m = curr_modifier;
		if (!tile_passable_2f800(m, t)) Char.x = char_dx_forward(5);
		control_standing_shift();
		return;
	}
	ctrl1_down = 1;
	seqtbl_offset_char(0x50);
}
void kid_crouch_pub(void) { kid_crouch(); }
/* 3212:08EE: crouched under a gate that is not open enough to stand up (not level 8's room 9) */
int under_gate(void)
{
	if (Char.room == 9 && level_number == 8) return 0;
	if (get_tile_at_char() != 4 && get_tile_infrontof(1) != 4 && get_tile_behind_char() != 4) return 0;
	uint16_t pos = curr_modifier & 0xFF;   /* the gate found last */
	int16_t x = char_dx_forward(-6) - 0xE;
	return pos != 0 && pos < 0xC8 && col_x_left[tile_col] + 0xC <= x && col_x_right[tile_col] + 4 >= x;
}
