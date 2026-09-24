/* Level 13's room 4 (OVL13 at 37F0): the shadow. Walking left along row 1 past x 0xDA kills the prince (seq 0xE6);
 * dying there (frame 0xB9, once: DS:0996) raises the shadow (a charid-1 character, 02E4), which walks to him and,
 * lying on him (f24 0xD), gives him back full hp (0000). Tile 0x2B (background 0x20) animates. Palettes, drawing and
 * sounds are left out. Transcribed from the disassembly. */
#include "types.h"
#include "globals.h"

/* 37F0:0000: the shadow merges into the prince: full hp, f24 0xD; the shadow's character goes */
static void shadow_merge(void)
{
	int8_t idx = (int8_t)Char.index; save_char(); loadkid();
	seqtbl_offset_char(0xE7); Char.alive = -1; Char.f12 = Char.f13;   /* (0FB3:2104, 24EA hp display) */
	Char.f24 = 0xD; Char.f10 = 0;
	Kid = Char; load_char(idx); clear_char();
	sound_1611_01a8(0x3C);
}
/* 37F0:0078 (2D3E:1864, charid 1 in room 4 of level 13): the shadow's moves */
void ovl_shadow_37f0_78(void)
{
	int si = -1; uint16_t f = Char.frame;
	if (f == 0xF) {
		if (Char.direction == -1) {
			if (Char.curr_col >= 6) {
				if (Char.f24 == 0xD) si = 0x47;
				else if (Char.curr_col >= 8) si = 1;
				else { control_standing_step(0); if (Char.f19 == 0x2C) si = 3; }
			} else if (Char.f24 == 0xD || Char.f19 == 0xE8) { si = 5; sound_1611_01a8(0x3B); }
			else si = 0xE8;
		} else if (Char.f24 != 0xD) si = 5;
		else if (Char.curr_col < 6) { control_standing_step(0); if (Char.f19 == 0x2C) si = 3; }
		else {
			int16_t cx = Kid.x - Char.x; if (Kid.direction == Char.direction) cx += 6;
			if (cx) control_standing_step(cx);
			else si = Kid.direction != Char.direction ? 5 : 0x47;
		}
	} else if (((f >= 1 && f <= 0xE) || (f >= 0x31 && f <= 0x38)) && Char.curr_col >= 6) control_runjump(4);
	else if (f >= 0x67 && f <= 0x6A) { ctrl1_shift = -1; try_grab_ledge_pub(); }
	else if (f == 0x5B && !seq_peek_frame_decreases()) {
		control_hanging_climb();
		if (Char.f24 == 0xD) Char.y -= 2;
		if (Char.direction == 0) set_char_collision();   /* 3212:09BE (then a redraw) */
	}
	else if (f == 0x9A) *(uint16_t *)&ROOM_ATTRS(4)[11] = 0x85;   /* DS:310C */
	else if (f == 0x99 && Char.f24 == 0xD) { /* palette */ }
	else if (Char.f24 == 0xD && f == 0xB9) shadow_merge();
	else if (f == 0x2C) { si = 0xD; Char.x = char_dx_forward(-8); }
	if (si != -1) seqtbl_offset_char(si);
}
/* 37F0:02E4: the shadow rises where the prince died (a new record in room 4, charid 1, 1 hp) */
static void shadow_rises(void)
{
	if (Char.room != 4 || level_number != 13) return;
	Kid = Char;
	int8_t n = (int8_t)ROOM_REC(4)->nchars++;
	level_char_init *rec = room_char_record(n, 4);
	rec->tilepos = (int8_t)(row_tilepos(Char.curr_row) + Char.curr_col); rec->x = Char.x; rec->direction = Char.direction;
	rec->f04 = 0; rec->pal = 0; rec->index = (uint8_t)n; rec->f10 = 0; rec->f38 = 0; rec->hp = 1;
	Char.index = (uint8_t)n; Char.x = rec->x; Char.charid = 1; Char.f12 = 1; Char.alive = -1; Char.pal_slot = 8;
	char_y_to_floor(); Char.y--; Char.fall_x = Char.fall_y = 0;
	Char.f24 = 0; Char.f10 = 0; Char.f23 = 0;
	seqtbl_offset_char(0xE7); play_seq(); sound_1611_01a8(0x3F);
	save_char(); loadkid();
}
/* 37F0:0236 (347C:0FC4, level 13 with room 4 drawn) */
void shadow13_tick(void)
{
	loadkid();
	if (Char.curr_row == 1 && Char.f19 != 0xE6) {
		if (Char.x <= 0xDA) {
			if (GOD_KID) { Kid = Char; return; }   /* (god mode: the flames do not burn) */
			seqtbl_offset_char(0xE6); Char.x = 0xD2; take_hp(100); play_sound(0x39); seq_set_85f8(0x10);
			*(uint16_t *)&ROOM_ATTRS(4)[11] = 0x85;
		} else if (Char.x <= 0xEC && !sound_playing(0x274D)) sound_1611_01a8(0x3D);
		else if (Char.frame == 0xB9 && Char.curr_row == 1 && Char.f24 != 0xC && word_0996 == 1) { shadow_rises(); word_0996 = 0xFFFF; }
	}
	Kid = Char;
}
/* 37F0:03CA (2A31:0E11, the restart key in level 13's room 4): a shadow is there */
int shadow13_present(void)
{
	int8_t n = ROOM_REC(4)->nchars;
	for (int8_t i = 0; i < n; i++) if (chars[i].charid == 1) return 1;
	return 0;
}
/* 37F0:040A (2A31:0E07, tile 0x2B with background 0x20): a flame: bit 7 set counts the low nibble down, else 0..11 */
void anim_tile2b(void)
{
	if (room_background_id() != 0x20) { cur_trob.state = 0xFF; return; }
	uint16_t si = (uint16_t)anim_mod;
	if (si & 0x80) { if (si & 0xF) si--; else si = 0; }
	else si = si < 0xB ? si + 1 : 0;
	anim_mod = (anim_mod & 0xFFFF0000u) | si;   /* (1375:0F5A redraws) */
}
