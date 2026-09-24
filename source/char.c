/* Character helpers reconstructed from 0AFF (core character logic segment). */
#include "types.h"
#include "globals.h"

/* 0AFF:08E4 - same rule as PoP1's fall_accel */
void fall_accel(void)
{
	if (Char.action == 4 || Char.action == 9) {
		if (is_feather_fall == 0) { Char.fall_y += 3; if (Char.fall_y > 33) Char.fall_y = 33; }
		else { Char.fall_y += 1; if (Char.fall_y > 4) Char.fall_y = 4; }
	}
}

/* 0AFF:091C - y += fall_y; in free fall (or for charids 7/8) x follows fall_x and the frame/column are reloaded */
void fall_speed(void)
{
	if (Char.y < 0x780) {
		Char.y += Char.fall_y;
		if (Char.action == 4 || Char.charid == 7 || Char.charid == 8) {
			Char.x = char_dx_forward(Char.fall_x);
			load_fram_det_col();
		}
	}
}

/* 0AFF:1CC2 / 0AFF:1D0E / 0AFF:1D60 */
void save_char(void)    { if (Char.index < 5) chars[Char.index] = Char; else rtlink_fatal(0x176); }
void load_char(int n)   { if (n >= 0 && n < 5) Char = chars[n]; else rtlink_fatal(0x191); }
void loadkid(void) { Char = Kid; }
/* 0AFF:0878 */
void load_char_and_opp(int n) { load_char(n); Opp = Kid; }
/* 0AFF:0DF6: 1 = the character is outside the level (room 0, or level kind 6 rooms 3/5 from row 11: level 14's pits) */
int char_out_of_level(void)
{
	if (Char.room != 0 && (((Char.room != 3 || level_kind != 6) && (Char.room != 5 || level_kind != 6)) || Char.curr_row < 11)) return 0;   /* DS:43FD: the level kind (level 14's pits) */
	return 1;
}

/* 0AFF:146E: may the character step/climb down here? (mod_here, mod_front, tile_here, tile_front) */
int can_climb_down_146e(uint16_t mod_here, uint16_t mod_front, uint8_t here, uint8_t front)
{
	if (!tile_is_empty_kind(front)) return 0;
	if (level_kind == 2 && (mod_front & 0x80)) return 0;
	if ((here == 11 || here == 15) && (mod_here & 0xF)) return 0;
	if (!tile_is_floor(here)) return 0;
	if (here == 4 && mod_here == 0 && Char.direction == -1) return 0;
	if (level_kind == 5 && Char.room == 15 && Char.curr_row != 0) return 0;
	return 1;
}

/* 0AFF:1C78: signed distance to the opponent along the facing direction (999 when not comparable) */
int opp_distance(void)
{
	if (Opp.f12 == 0 || Char.room != Opp.room || Char.curr_row != Opp.curr_row || Opp.direction == 0x56) return 999;
	int d = Opp.x - Char.x; if (Char.direction != 0) d = -d;
	if (d >= 0 && Char.direction != Opp.direction) return d + 13;
	return d;
}

/* 0AFF:1AC2: is this Char's lying-dead frame? */
int is_dead_frame(uint16_t f)
{
	if (f == 0xB9) return 1;
	switch (Char.charid) {
	case 0:
		if (f == 0xF2 || f == 0xF3 || f == 0xB9 || f == 0x10F || f == 0x10A) return 1;
		return level_number == 5 && Char.room == 3 && Char.alive >= 0 && Char.f24 == 10;
	case 2:
		if (level.type == 0) return f == 0xE4;
		return f == 0xF5 || f == 0xF8 || (f >= 0xFB && f <= 0xFF);
	case 4: return f == 0xCE || f == 0xCF;
	case 10: return f == 0xD5;
	case 0xB: return f == 0x111;
	}
	return 0;
}
/* 0AFF:1BA2 (sequence opcode FFEF): the character leaves the level */
void clear_char(void)
{
	if (Char.room == 6 && level_kind == 6 && Char.f19 == 0xF0 && Kid.alive < 0) jaffar_leaves_room6();   /* 33FD:1416 (final.c) */
	/* 0AFF:1BCE -> 0FB3:25D4 (the hp display) first reloads the character's record, dropping this tick's steps */
	int8_t idx = (int8_t)Char.index;
	if (idx == -1) { idx = find_opponent(1); if (idx == -1 && room_nchars(drawn_room) != 0) idx = 0; }
	if (idx >= 0 && idx < 5) load_char(idx);
	Char.direction = 0x56; Char.action = 0; Char.alive = 0; Char.f12 = 0;
	if (Char.index != 0 && Char.index == Kid.opp_index) Kid.opp_index = (uint8_t)find_opponent(1);   /* 2D3E:08E8 */
	Char.room = 0;
}
