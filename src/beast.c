/* Charid 11 in the kind-4 guard overlay (OVL09 at 366C, levels 6..9): a creature that waits, turns toward the prince,
 * charges along its row and pounces (seq 0xAA wait, 0xAB align, 0xAC turn, 0xAD pounce, 0xB0 run, 0xB1 stop, 0xA8 halt).
 * Its record's y byte (+0x11) is a wait timer while >= 0. */
#include "types.h"
#include "globals.h"

static int blocks(uint8_t t) { return tile_is_wall_kind(t) && !(t == 7 && (curr_modifier & 0x80)); }   /* 7 with bit 7: an open door */
static int standable(uint8_t t) { return tile_is_floor(t) || (t == 7 && (curr_modifier & 0x80)); }
/* 366C:1632: facing the prince? */
static int beast_facing(void)
{
	if (Char.f19 == 0xAB || Char.f23 < 2) return 0;
	return (Char.direction == 0 && Char.curr_col <= Opp.curr_col) || (Char.direction == -1 && Char.curr_col >= Opp.curr_col);
}
static int opp_frame_open(void) { uint16_t f = Opp.frame; return (f >= 1 && f <= 0xE) || (f >= 0x31 && f <= 0x38); }   /* running / running jump */
/* 366C:122A: in reach: pounce when the prince comes running, else run at him */
static void beast_attack(void)
{
	int pounce = 0, d = opp_distance();
	if (d >= 0xD && d <= 0x2D) {
		if (Char.frame == 0x116 || Char.frame == 0x117) pounce = 1;
		else if (Char.f19 == 0xB0 && opp_frame_open()) pounce = 1;
		else if (Char.f19 != 0xB0 && Char.f19 != 0xAD) { seqtbl_offset_char(0xB0); play_sound(0x47); }
	} else {
		if (Char.f19 == 0xB0 && opp_frame_open() && d > 0xD && d < 0x32) pounce = 1;
		else if (Char.f19 == 0xB0 && Opp.frame >= 0x87 && Opp.frame <= 0x95 && d > 0xD && d < 0x2D) pounce = 1;
		else if ((Char.frame == 0x116 || Char.frame == 0x117) && (d < 0xD || d > 0x31) && standable(get_tile_infrontof(1))) seqtbl_offset_char(0xB1);
	}
	if (pounce && Char.f19 != 0xAD) { Char.x = char_dx_forward(d - 0x20); seqtbl_offset_char(0xAD); }
}
/* 366C:1368: waiting; when the timer runs out, face the prince if he is in reach on this row, else turn around */
static void beast_wait(level_char_init *r)
{
	uint8_t *timer = (uint8_t *)&r->y;
	if ((int8_t)--*timer > 0) return;
	*timer = 0xFF;
	int8_t left = scan_to_wall_pub(-1, Char.curr_row, Char.curr_col, Char.room), right = scan_to_wall_pub(1, Char.curr_row, Char.curr_col, Char.room);
	uint8_t t = get_tile_at_char();
	if (Opp.room == Char.room && Opp.curr_row == Char.curr_row && Char.curr_col > left && Char.curr_col < right)
		Char.direction = (Char.curr_col > Opp.curr_col || blocks(t)) ? -1 : 0;
	else Char.direction = ~Char.direction;
	if (blocks(t)) { seqtbl_offset_char(0xAA); Char.x = char_dx_forward(0x26); }
	else seqtbl_offset_char(0xAC);
	Char.f24 = 0;
}
/* 366C:1474: hunting (timer < 0, not facing the prince) */
static void beast_hunt(level_char_init *r)
{
	if (Opp.alive >= 0) { if (Char.f19 != 0xB0 && Char.frame != 0x116 && Char.frame != 0x117) seqtbl_offset_char(0xB0); return; }
	if (Char.f19 != 0xAB && Char.f19 != 0xAC && Char.f24 != 9) {
		get_tile_at_char();
		if (((curr_modifier >> 8) & 0xF) == 0xC) {   /* a ledge mark: line up with it */
			int si = (Char.x - 0x90) % 32;
			if (si < 0xE || si > 0x12) return;
			si = 0x10 - si; if (Char.direction == -1) si = -si; else si += 8;
			Char.x = char_dx_forward(si); seqtbl_offset_char(0xAB); return;
		}
		uint8_t t = get_tile_infrontof(1);
		if (Char.f19 == 0xB0 || Char.f19 == 0xA8 || Char.frame == 0x116 || Char.frame == 0x117) { if (standable(t)) seqtbl_offset_char(0xB1); return; }
		if (standable(t)) return;
		if (!tile_is_wall_kind(t)) { seqtbl_offset_char(0xB0); return; }
		if (t == 7 || (Char.direction != 0 && Char.frame == 0xFE)) { seqtbl_offset_char(0xA8); return; }
		if (Char.direction == 0) Char.f24 = 9;
		return;
	}
	if (Char.f24 == 9) {
		uint8_t t = get_tile_at_char(); int16_t d = distance_to_edge_weight();
		if (tile_is_wall_kind(t) && d < 8) { Char.frame = 0x108; seqtbl_offset_char(0xA8); }
	}
	if (Char.frame == 0x108) { Char.f24 = 6; *(uint8_t *)&r->y = Char.f23 == 2 ? 6 : random_2751(0x3C) + 0x24; }
}
/* 366C:11DA */
void beast_ai(void)
{
	if (Char.alive >= 0) return;
	level_char_init *r = room_char_record(Char.index, Char.room);
	if (!r) return;
	load_char(Char.index); Opp = Kid;   /* 0AFF:0878 */
	if ((int8_t)*(uint8_t *)&r->y < 0) { if (beast_facing()) beast_attack(); else beast_hunt(r); }
	else beast_wait(r);
	save_char_restore_kid_pub();   /* 0AFF:089A */
}
/* 366C:041A (kind 4): a charid 7/8/11 record entering the drawn room */
level_char_init *beast_room_entry(level_char_init *r)
{
	if (r->type == 0) { r->type = 5; Char.charid = type_to_charid[5]; char_y_to_floor(); }
	uint8_t *y = (uint8_t *)&r->y;
	if (Char.charid == 0xB) {
		if (Char.alive < 0) {
			if ((int8_t)*y <= 0) { r->seq_id = 0xAA; r->seq_pos = 0; *y = 0xFF; }
			else { r->seq_id = 0xAF; r->seq_pos = 0; Char.f24 = 6; }
			Char.f10 = 1; play_sound(0x47);
		} else Char.f10 = 0;
	} else {
		Char.x = col_x_left[Char.curr_col] + 0x1E; Char.direction = -1; r->seq_id = 0x82; r->seq_pos = 0; Char.f10 = 0; *y = 0;
	}
	return r;
}
/* 366C:166A (the room record written back as the charid-11 creature leaves the drawn room): resting, it sleeps a
 * random while; walking at a gap or a wall (not an open door 7), it turns and steps back */
void beast_record_fixup(level_char_init *rec)
{
	if (Char.f19 == 0xAB || Char.f24 == 9) { Char.f24 = 6; *(uint8_t *)&rec->y = (uint8_t)(random_2751(0x3C) + 0x24); return; }
	uint8_t t = get_tile_infrontof(1);
	if (tile_is_floor(t) || (t == 7 && (curr_modifier & 0x80))) return;
	if (Char.f19 == 0xB0 || Char.f19 == 0xA8 || Char.f19 == 0xAA) { rec->direction = ~Char.direction; rec->x = char_dx_forward(-0x20); }
}
