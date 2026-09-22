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
int16_t dx_weight(void) { return char_dx_forward((int8_t)(frame_dx - (frame_flags & 0x1F))); }
int16_t distance_to_edge(int16_t x) { x_to_col(x); return Char.direction == 0 ? 31 - obj_xl : obj_xl; }
int16_t distance_to_edge_weight(void) { return distance_to_edge(dx_weight()); }

/* 0AFF:1376 */
int control_rest(void) { ctrl1_down = ctrl1_up = ctrl1_forward = ctrl1_backward = 0; return 1; }

/* 2FDF:0E2E (030c1e): crouch */
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
	if (!tile_is_empty_kind(get_tile_n_ahead(1))) {
		if (distance_to_edge_weight() <= 2) { Char.x = char_dx_forward(10); load_fram_det_col(); return; }
	}
	if (!tile_is_empty_kind(get_tile_infrontof_char())) { kid_crouch(); return; }
	if (distance_to_edge_weight() < 8) { kid_crouch(); return; }
	uint8_t front = get_tile_infrontof_char(); uint16_t front_mod = curr_modifier;
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
		if (Char.charid == 0 && kid_f34 != 0) kid_f34 = 0;
	}
}
/* 2FDF:0E18 (031038) / 0E36 (031056) */
void control_start_run(void) { if (ctrl1_up < 0 && ctrl1_forward < 0) { ctrl1_forward = control_rest(); ctrl1_up = ctrl1_forward; control_jump_031062(); } }
void control_jumpup(void)    { if (ctrl1_forward != 0) control_jump_031062(); }

/* 2FDF:048C */
void control(void)
{
	uint8_t frame = Char.frame;
	if (Char.alive >= 0) {
		if (Char.charid != 0 && Char.index == kid_84af) kid_84af = find_char_02dcc8();
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
	if (here != 7 && here != 12 && here != 13 && get_tile_n_ahead(1) != 7 && get_tile_n_ahead(1) != 12 && get_tile_n_ahead(1) != 13
	    && get_tile_infrontof_char() != 7 && get_tile_infrontof_char() != 12 && get_tile_infrontof_char() != 13 && Char.f19 != 0x7C) {
		seqtbl_offset_char(0x7C); return;
	}
	here = get_tile(Char.curr_row, Char.curr_col, Char.room);
	if (Char.frame != 0xFB) { control_rest(); return; }
	if (level_kind == 4) ovl_35a88();
	if (ctrl1_up == 0 || here == 7) {
		if (ctrl1_forward < 0 && byte_5cc5 != 0) { control_rest(); seqtbl_offset_char(0x7A); }
		else if (ctrl1_backward < 0) { control_rest(); seqtbl_offset_char(0x7B); }
	} else {
		uint8_t front = get_tile_infrontof_char(); int16_t d;
		if (front == 7) d = Char.direction == 0 ? (Char.x - col_x_left[Char.curr_col]) - 0x26 : (col_x_right[Char.curr_col] - Char.x) - 10;
		else if (front == 12 || front == 13) d = ovl_34350() == 0 ? 0x20 : -0x20;
		else d = 0x20;
		if (d >= 0) { seqtbl_offset_char(0x7C); ctrl1_up = control_rest(); if (d < 6) Char.x = char_dx_forward(-(d - 6)); }
	}
	if (ctrl1_forward == 0 && byte_5cc5 == 0) byte_5cc5 = 1;
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
	uint16_t f19 = Char.f19, id = Char.seq_id, pos = Char.seq_pos; uint8_t frame = Char.frame;
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
		uint8_t t = get_tile_n_ahead(1);
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
	uint8_t front = get_tile_infrontof_char(), here = get_tile_at_char(); int id, dx = 0;
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
