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
/* 2F86:0078 (2FDF:0ED9, a standing turn on kinds 2 and 6): turns in a row count; from the fourth each costs a hit
 * point and a point of the maximum, the eighth leaves the body (more than 4 hp) or kills */
void turn_count(void)
{
	if (Char.charid != 0 || Char.f19 != 5) { word_5cbe = 1; return; }
	if ((int16_t)++word_5cbe < 4) return;
	/* the fourth: 0FB3:2B1C palette flash (2F86:04AE) and sound 0x2815 */
	if (!take_hp(1)) Char.f13--; else seqtbl_offset_char(0x47);
	if (word_5cbe == 8) {
		if ((int8_t)Char.f12 > 4) {
			leave_body();
			Char.charid = 1; Char.pal_slot = 8;
			seqtbl_offset_char(2); play_sound(0x105); sound_1611_01a8(0x110);
		} else { seqtbl_offset_char(0x47); take_hp(100); }
		word_5cbe = 0;
	}
	play_sound(0x5D);
}
