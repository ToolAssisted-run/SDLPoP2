/* Sword hits, hurting and dying (OVL01 2D3E:1F48 / 19C2 / 1AAA / 1D74), guards' line of sight (169B:0FF0).
 * Transcribed from the disassembly. */
#include <stdlib.h>
#include "types.h"
#include "globals.h"
#include "settings.h"

/* 0AFF:089A / 0AFF:0840: write Char back and restore Kid from Opp; write Kid and chars[Opp.index] back */
static void save_char_restore_kid(void) { if ((int8_t)Char.index >= 0 && (int8_t)Char.index < 5) save_char(); Kid = Opp; }
static void save_kid_and_opp(void) { if ((int8_t)Opp.index >= 0 && (int8_t)Opp.index < 5) { Kid = Char; chars[Opp.index] = Opp; } }

/* 2D3E:23C8 (02F7A8): the frame whose sword actually connects */
static int sword_hit_frame(uint16_t f, uint8_t charid)
{
	if (charid == 10) return f == 0xE0;
	if (charid == 11) return Char.f19 == 0xAD && f == 0x118;
	if (f == 0x9A) return 1;
	return charid == 0 && f == 0xF5;
}
/* 2D3E:1FB0: Char strikes at Opp: parried, hit (Opp.action = 99) or missed */
static void check_strike(void)
{
	if (Char.f10 != 1 || Char.curr_row != Opp.curr_row) return;
	if (abs(Char.y - Opp.y) >= 7 && Char.charid != 7 && Char.charid != 8 && Opp.charid != 7 && Opp.charid != 8) return;
	if (!frame_is_strike_02f712(Char.frame, Char.charid)) return;
	if (Opp.alive >= 0 || Opp.charid == 10 || Opp.charid == 1 || Opp.action == 8 || Opp.f19 == 0x6E) return;
	if (Char.charid == 1 && Opp.charid == 0) return;
	int16_t d = opp_distance(), near = 0, far = (Char.charid == 7 || Char.charid == 8) ? 0x1C : 0x2C;
	if (Char.direction != Opp.direction && d >= 0 && far >= d && Opp.charid != 7 && Opp.charid != 8 && Opp.charid != 0xB && Char.charid != 0xB
	    && (Opp.frame == 0xA1 || Opp.frame == 0x96)) {
		Opp.frame = 0xA1;   /* parried */
		if (Char.charid == 0) Char.opp_index = Opp.index; else { Opp.opp_index = Char.index; word_68f0 = 4; }
		int id = 0x45;
		if (Char.charid == 7 || Char.charid == 8) id = ovl_366c_6ac(); else if (Char.charid == 10) id = ovl_366c_1580();
		if (id != -1) seqtbl_offset_char(id);
		play_seq();
	} else if (sword_hit_frame(Char.frame, Char.charid)) {
		sword_range_pub(&far, &near);
		if (near <= d && d <= far) { Opp.action = 99; if (Char.charid == 0) Char.opp_index = Opp.index; }
	} else if ((Char.charid == 7 || Char.charid == 8) && Char.f24 == 1 && word_68f0 == 0) Opp.action = 99;
	if (sword_hit_frame(Char.frame, Char.charid) && (Opp.frame != 0xA1 || Opp.charid == 7 || Opp.charid == 8) && Opp.action != 99) play_sound(Char.charid == 10 ? 0xC1 : 0xD);
}
/* 2D3E:1F48: each character of the drawn room strikes at the prince, and the prince at each of them */
void check_sword_hits(void)
{
	if (Kid.frame == 0 || (Kid.frame >= 0xDB && Kid.frame < 0xE5)) return;
	int8_t n = room_nchars(drawn_room);
	for (int8_t i = 0; i < n; i++) {
		load_char(i); Opp = Kid; if (!cheat_god) check_strike(); save_char_restore_kid();   /* (god mode: nothing strikes the prince) */
		load_opp_080a(i); check_strike(); save_kid_and_opp();
	}
}

/* 2D3E:1D74 (02F154): the character dies from a sword hit */
static void char_dies(void)
{
	int si = -1;
	Char.f23 = 0; Char.f0f = 1;
	if (Char.charid == 4) skel_collapse();
	else if (Char.charid == 7 || Char.charid == 8) { Char.direction = ~Kid.direction; Char.f12 = 0; si = 0x9A; }
	else if (Char.charid == 0xB) si = 0xAE;
	else if (Char.charid == 6) si = 0xC4;
	else {
		uint8_t t = get_tile_behind_char();
		if (Char.charid != 0 && Char.direction == -1 && (t == 4 || get_tile_at_char() == 4)) {
			int8_t c = tile_col_in_drawn_room();
			Char.x = col_x_left[c - (curr_tile != 4 ? 1 : 0)] + 14;
			Char.x = char_dx_forward(0x14);
		}
		if (Char.charid == 0 && Char.frame >= 0xF6 && Char.frame <= 0x105) {
			si = 0x62;
			play_sound(Opp.charid == 0xB ? 0x45 : (Opp.charid == 7 || Opp.charid == 8) ? 0x59 : 0xE);
		} else if (Opp.charid == 0xB) { si = 0x61; play_sound(0x45); }
		else {
			if (Opp.charid == 6) ovl_33fd_6ae();
			si = 0x55;
			if (Char.charid != 0 && ovl_366c_fc()) { si = 0xB9; Char.x = char_dx_forward(-8); }
		}
		if (Char.charid == 0) {   /* death music by the killer's kind */
			static const uint8_t music[10] = {0, 2, 2, 2, 0x11, 6, 6, 2, 0xC, 7};
			int k = Opp.charid - 2; uint8_t m = (k >= 0 && k < 10) ? music[k] : 2;
			if (k == 0) m = level.type == 0 ? 14 : 10;
			seq_set_85f8(m);
		}
	}
	if (Opp.charid == 0 || Opp.charid == 1) { save_char_restore_kid(); Opp.opp_index = find_opponent(Opp.direction); }
	if (si != -1) seqtbl_offset_char(si);
}
/* 2D3E:1AAA (02EE8A): Char was hit (action 99) */
static void char_hurt(void)
{
	int hp = 1, si = 0, d;
	if (Char.alive >= 0) return;
	if (Char.charid == 0 && Char.f19 == 5 && (level_kind == 2 || level_kind == 6)) music_1286_07ce(level_kind);
	if ((int8_t)Char.f10 == -1 || Char.f19 == 0xA || Char.f19 == 0x10 || Char.f19 == 0x1C || Char.f19 == 0xE) { take_hp(100); char_dies(); goto fall_check; }
	if (Char.charid == 7 || Char.charid == 8) { si = 0x99; hp = byte_5cba == 2 ? 1 : 2; if (hp < (int8_t)Char.f12) ovl_366c_f24(); }
	else if (Char.charid == 0xB || Opp.charid == 0xB) hp = 100;
	else {
		hp = (Char.charid == 6 || Opp.charid == 6) ? 100 : 1;
		si = Char.direction == Opp.direction ? 0x5E : 0x4A;
		if (Opp.charid == 7 || Opp.charid == 8) {
			if (Char.f10 != 1 && !(frame_table_kid[Char.frame * 7 + 6] & 0x40)) si = 0x2D;
			else if (Char.frame >= 0xF6 && Char.frame <= 0x105) {
				if (tile_is_wall_kind(get_tile_behind_char())) Char.x = char_dx_forward(16);
				else if (tile_is_wall_kind(get_tile_infrontof(1))) Char.x = char_dx_forward(-16);
			}
		}
		if (Char.charid == 0 && si != 0x2D) Char.f10 = 1;
	}
	if (!take_hp(hp) && Char.charid != 0xB) { seqtbl_offset_char(si); goto on_floor; }
fall_check:
	load_frame(); d = distance_to_edge_weight();
	if ((tile_is_empty_kind(get_tile_at_char()) || (tile_is_empty_kind(get_tile_behind_char()) && d >= 4))
	    && Char.charid != 7 && Char.charid != 8 && Char.charid != 0xB && Char.charid != 6) {
		d -= 8; if (Char.curr_col != tile_col) d -= 0x20;
		Char.x = char_dx_forward(d); load_fram_det_col(); Char.curr_row++; seqtbl_offset_char(0x51);
		goto sound;
	}
	char_dies();
on_floor:
	char_y_to_floor(); Char.fall_y = 0;
sound:
	if (Opp.charid != 7 && Opp.charid != 8 && Opp.charid != 0xB) {
		int s = Char.charid == 0 ? (Opp.charid == 10 ? 0xC2 : 0xE) : Char.charid == 4 ? 0x48 : (Char.charid == 7 || Char.charid == 8) ? 0x56 : Char.charid == 0xB ? 0x46 : 0x15;
		play_sound(s);
	}
	play_seq();
	if (Char.f19 == 0x55) {
		load_fram_det_col(); uint8_t t = get_tile_at_char();
		if (tile_is_empty_kind(t) || t == 0xB || t == 0x1A || t == 0xF) { Char.x = char_dx_forward(-24); load_fram_det_col(); }
		land_adjust_pub();
	}
}
/* 2D3E:19C2: characters marked hit (action 99) take the hit */
void process_hurt(void)
{
	int8_t n = room_nchars(drawn_room), i;
	for (i = 0; i < n; i++) {
		load_char(i);
		if (Char.action != 99) continue;
		load_char(i); Opp = Kid;
		if (Opp.action == 99) Opp.action = 1;
		char_hurt();
		{ uint8_t skill = ((uint8_t *)&level)[0x17F3 + Char.room * 0x74 + 1 + (int8_t)Char.index * 23 + 4];
		  word_922e = pop2_settings_game && skill < SETTINGS_SKILLS ? pop2_settings_game->refractimer[skill] : refract_timer[skill]; }   /* DS:13D0[skill] (SDLPoP2.ini [Skill N] refractimer) */
		save_char_restore_kid();
	}
	if (Kid.action == 99) {
		for (i = 0; i < n; i++) {
			load_char(i);
			if (Char.charid == 7 || Char.charid == 8) { if (Char.frame == 0xA7 || Char.frame == 0xB5) break; }
			else if (sword_hit_frame(Char.frame, Char.charid)) break;
		}
		if (i < n) { load_opp_080a(i); char_hurt(); save_kid_and_opp(); }
	}
}

/* 169B:0FF0: which guards can see the prince (Char+0x23: 0 no, 1 blocked by a gap/gate, 2 sees, 3 in line and engaged) */
void guards_see_kid(void)
{
	uint16_t kf = Kid.frame;
	if (Kid.room == 0) return;
	int8_t right = find_opponent(0), left = find_opponent(-1);
	int8_t n = room_nchars(Kid.room);
	for (int8_t i = 0; i < n; i++) {
		load_char(i);
		if (kf == 0 || (kf >= 0xD9 && kf < 0xE2) || Kid.alive >= 0 || Char.alive >= 0 || (uint8_t)Char.direction == 0x56
		    || (Char.curr_row != Kid.curr_row && Char.charid != 7 && Char.charid != 8) || (Char.charid == 0xB && Char.f24 == 6)
		    || (Kid.charid == 1 && level_kind == 2)) { Char.f23 = 0; save_char(); continue; }
		Char.f23 = 3;
		int8_t a = Kid.curr_col, b = Char.curr_col;
		if (a > b) { int8_t t = a; a = b; b = t; }
		if (get_tile(Kid.curr_row, b, Kid.room) == 4) b--;
		if (b > a) {
			for (int8_t c = a; c <= b && Char.f23 == 3; c++) {
				uint8_t t = get_tile(Kid.curr_row, c, Kid.room);
				if (tile_is_wall_kind(t) && !(Char.charid == 0xB && t == 7 && ((uint8_t)curr_modifier & 0x80))) Char.f23 = 0;
				else if ((t == 4 && (uint8_t)curr_modifier < 0x70) || (tile_is_loose_kind(t) && Char.charid != 10)
				         || (tile_is_empty_kind(t) && Char.charid != 10 && Char.charid != 7 && Char.charid != 8)) Char.f23 = 1;
			}
		}
		if (Char.f23 == 3 && Char.charid != 0xB) {
			if (left == i || right == i) { if (Char.charid == 10) Char.f10 = 1; }
			else Char.f23 = 2;
		}
		save_char();
	}
}
void char_dies_pub(void) { char_dies(); }
void save_char_restore_kid_pub(void) { save_char_restore_kid(); }
/* 1611:0068 (0AFF:1258, a character dead for 6 ticks): the victory music, once (Char+0x0F), unless it would clash */
void dead_char_music(void)
{
	if (Char.f0f == 0) return;
	int play = 0, si = 0x4A;
	switch (Char.charid) {
	case 2: play = Char.alive >= 6; si = 0xD1; break;
	case 7: case 8: play = !sound_playing(0x2768); si = 0x93; break;
	case 10: play = !sound_playing(0x27D3); si = 0xC0; break;
	case 11: Char.f0f = 0; /* fall through */
	default:
		play = Kid.alive < 0 && Char.f19 == 0x78 && (int8_t)Char.alive > 4 && !sound_playing(0x275C) && !sound_playing(0x2771) && !sound_playing(0x2772) && Kid.alive < 0;
		si = 0x4A; break;
	}
	if (!play) return;
	if (!char_scan_31bc4() && !find_spawn_pub(Char.curr_row, Char.room) && level_kind != 5) sound_1611_01a8(si);
	Char.f0f = 0;
}
