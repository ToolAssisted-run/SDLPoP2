/* The turn counter of the temple and final levels (OVL01 2F86): turning back and forth (seq 5) counts in DS:5CBE;
 * from the fourth turn the prince flashes, and the eighth (with more than 4 hp) leaves his body behind as a character
 * while he goes on as the spirit (charid 1). Transcribed from the disassembly. */
#include "types.h"
#include "globals.h"

/* 2F86:0456: during the turn (frames 0x2E..0x33), which frames show the flash palette */
static int turn_flash_frame(uint16_t frame, int16_t n)
{
	if (n < 4 || frame <= 0x2D || frame >= 0x34) return 0;
	int16_t di = n - 4; uint16_t bx = frame - 0x2E;
	if (di < 2) return bx % (uint16_t)(3 - di) == 0;
	return (bx + 1) % 3;
}
/* 2F86:01FC (the prince, 2FDF:0DDA turning and 2FDF:0A94 standing on kinds 2/6) */
void turn_flash(void)
{
	if (Char.frame == 0xF) { word_5cbe = 0; return; }
	if (Char.frame >= 0x2D && Char.frame <= 0x34 && !((Char.room == 1 || Char.room == 2) && level_number == 14)) {
		if ((int16_t)word_5cbe >= 4) Char.pal_slot = turn_flash_frame(Char.frame, (int16_t)word_5cbe) ? 8 : 0;
		return;
	}
	if (Char.f19 == 5 || word_5cbe == 0) return;
	music_1286_07ce(level_kind);   /* 1286:07CE: the level's music again */
	word_5cbe = 0;
}

/* 2F86:0264: the prince's body stays behind: a new character (charid 0, seq 0x47) in the room's list, placed off a
 * gap; Kid keeps the prince (the caller turns him into the spirit) */
static void leave_body(void)
{
	if (tile_is_empty_kind(get_tile_behind_char())) Char.x = char_dx_forward(0x10);
	else if (tile_is_empty_kind(get_tile_infrontof(1))) Char.x = char_dx_forward(-0x10);
	Kid = Char;
	level_room *room = ROOM_REC(Char.room); uint8_t n = room->nchars++;
	level_char_init *rec = &room->chars[n];   /* 2D3E:08AC */
	rec->tilepos = (int8_t)(row_tilepos(Char.curr_row) + Char.curr_col);
	rec->x = Char.x; rec->direction = Char.direction; rec->f04 = 0; rec->pal = 0; rec->index = n;
	rec->f10 = 0; rec->f38 = 0; rec->hp = Char.f12; rec->type = 0xA; rec->max_hp = Char.f13;
	Char.index = n; Char.charid = 0; Char.pal_slot = 2;
	seqtbl_offset_char(0x47); play_seq(); save_char(); loadkid();
}
/* 2F86:04AE: the spirit shows as the flame: +0x24 is 0xD, or on the final level (kind 6) more than 2 hit points */
int spirit_flame_shows(void) { return Char.f24 == 0xD || (level_kind == 6 && (int8_t)Char.f12 > 2); }
/* 2F86:0142 (0AFF:0AAA start_fall and 1375 of the spirit): on level 13's room 4 merging (+0x24 0xD) the saved colors
 * back (0FB3:294C); after a cast (seq 0xF2) with no fireball in flight and 2 hit points or fewer, the shadow's colors */
void shadow_hook_2f9a2(void)
{
	if (Char.room == 4 && level_number == 13 && Char.f24 == 0xD) { hook_pal_restore(); return; }
	if (Char.f19 == 0xF2 && !find_mob_pub(1, 0xC) && (int8_t)Char.f12 <= 2) hook_pal_load(0, 0x10, 0x30, 2000);
}
/* 2F86:0192 (0823:1008 on the final level, the prince's hit points changing as the spirit): down to 2 or fewer (not
 * casting) the shadow's colors, up from 2 or fewer the flame's */
void ovl_2f9f2(void)
{
	int hp = (int8_t)Kid.f12, after = hp + (int8_t)Kid.hp_delta, sub = -1;
	if (hp > 2 && after <= 2 && Kid.f19 != 0xF2) sub = 0;
	else if (hp <= 2 && after > 2) sub = 1;
	if (sub >= 0) hook_pal_load(sub, 0x10, 0x30, 2000);
}
/* 2F86:0078 (2FDF:0ED9, a standing turn on kinds 2 and 6): turns in a row count; from the fourth each costs a hit
 * point and a point of the maximum, the eighth leaves the body (more than 4 hp) or kills */
void turn_count(void)
{
	if (Char.charid != 0 || Char.f19 != 5) { word_5cbe = 1; return; }
	if ((int16_t)++word_5cbe < 4) return;
	/* the fourth: the spirit's colors at 0x30 (0FB3:2B1C: PALS 2000, sub-palette 1, the flame's, when 2F86:04AE says
	 * the flame shows, else 0, the shadow's) and sound 0x2815 */
	if (word_5cbe == 4) hook_pal_load(spirit_flame_shows() ? 1 : 0, 0x10, 0x30, 2000);
	if (cheat_god) {}   /* (god mode: turning costs nothing) */
	else if (!take_hp(1)) Char.f13--; else seqtbl_offset_char(0x47);
	if (word_5cbe == 8) {
		if ((int8_t)Char.f12 > 4) {
			leave_body();
			Char.charid = 1; Char.pal_slot = 8;
			seqtbl_offset_char(2); play_sound(0x105); sound_1611_01a8(0x110);
		} else if (!cheat_god) { seqtbl_offset_char(0x47); take_hp(100); }
		word_5cbe = 0;
	}
	play_sound(0x5D);
}

/* (SDLPoP2, the shadow / flame cheat, cheats.c) the prince leaves his body now, as the eighth turn does, without its
 * cost */
int spirit_leave_body(void)
{
	load_fram_det_col();
	leave_body();
	Char.charid = 1; Char.pal_slot = 8;
	seqtbl_offset_char(2); play_sound(0x105); sound_1611_01a8(0x110);
	word_5cbe = 0; Kid = Char;
	return 1;
}
/* 2F86:03CC: the drawn room's first charid-0 character (the prince's body), or the count */
int8_t body_index(void)
{
	int8_t n = room_nchars(drawn_room), i = 0;
	while (i < n && chars[i].charid != 0) i++;
	return i;
}
/* 2F86:040C: the spirit's x distance to its body (Opp = the body; loads Char = Kid), 999 on another row */
static int16_t body_distance(void)
{
	load_opp_080a(body_index());
	if (Char.curr_row != Opp.curr_row) return 999;
	int16_t cx = Char.x - Opp.x;
	if (Char.direction != Opp.direction || Char.frame == 0xB9) return cx;
	return cx + (int8_t)ds_byte(0xCFB + Char.direction) * 6;
}
/* 2F86:000A (crouching, the spirit): close to the body (< 0x20), it lies down into it (seq 0x47) */
int shadow_seq_2f86a(void)
{
	uint8_t opp = Opp.index; int16_t si = body_distance(); int r = -1;
	if ((si < 0 ? -si : si) < 0x20) {
		if (Char.direction != Opp.direction) { Char.direction = ~Char.direction; Char.x = char_dx_forward(6); }
		Char.x -= si; Char.y--; Char.f0f = 0; Kid = Char; r = 0x47;
	}
	load_opp_080a(opp);
	return r;
}
/* 2F86:04CE: the spirit (lying on its body) rejoins it: the body's character is removed, the prince is charid 0 again */
static void spirit_rejoin(void)
{
	if (Opp.charid != 0) return;
	Kid = Char; load_char(Opp.index); clear_char(); save_char(); loadkid();
	Char.charid = 0; Char.pal_slot = 0; Char.f0f = 1; cheat_spirit = 0; cheat_form = 0;
	seqtbl_offset_char(0xE7); play_seq(); Kid = Char;
}
/* 2F86:0344 (the spirit dies): the body becomes the prince again (index 0xA), with the spirit's hp; its character is removed */
void shadow_2fba4(void)
{
	Kid = Char;   /* (0FB3:24EA hp bars) */
	load_char(body_index());
	char_type body = Char;
	Char.index = 0xA; Char.pal_slot = 0; Char.f12 = Kid.f12; Char.f13 = Kid.f13; Char.hp_delta = Kid.hp_delta;
	Kid = Char; Char = body;
	clear_char(); loadkid(); cheat_spirit = 0; cheat_form = 0;
}
/* 2FDF:09B2 (control, dead frames and 0xB3..0xB7) */
void control_dead_0307a2(void)
{
	if (is_dead_frame(Char.frame)) {
		if (level_kind == 1 && Char.frame == 0xB9 && Char.f12 != 0) { if (!GOD_KID) take_hp(Char.f12); return; }
		if (Char.charid == 1 && Char.f0f == 0) { int16_t d = body_distance(); if (d >= -1 && d <= 1) spirit_rejoin(); }
		return;
	}
	/* the body lying (0xB4..0xB6): the spirit loses a point of hp (or of the maximum) and the body one */
	if (Char.charid != 0 || Char.index == 0xA || Char.frame == 0xB3 || Char.frame == 0xB7 || cheat_god || cheat_spirit) return;   /* (god mode, a spirit out by the cheat: no drain) */
	int8_t idx = (int8_t)Char.index; save_char(); loadkid();
	if (!take_hp(1)) Char.f13--;
	Kid = Char; load_char(idx); take_hp(1);
}
