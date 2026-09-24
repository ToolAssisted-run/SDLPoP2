/* Level kind 4 (ruins, levels 6..9): OVL06 loaded at 347C. */
#include "types.h"
#include "globals.h"

/* 347C:0198 (level 6): leaving the first room (27) sends the prince to room 3, at the level's real entrance */
void level6_entrance(void)
{
	Char.direction = 0; seqtbl_offset_char(5);
	Char.room = next_room = 3; Char.curr_col = 4; Char.x = col_x_left[4] + 0xE; Char.curr_row = 2;
	char_y_to_floor(); play_seq(); Kid = Char;
	close_entrance_pub();   /* 169B:034A */
	control_rest(); ctrl1_shift = 0;   /* 0AFF:1376 */
}
/* 347C:0126: a crumbling floor (tile 0xF at curr_room/curr_tilepos) falls: a type-3 object replaces it */
void ruins_crumble(void)
{
	cur_mob.x = (curr_tilepos % 10) * 32; cur_mob.y = 63 * tile_row + 0x42; cur_mob.room = curr_room;
	cur_mob.w7 = 0; cur_mob.speed = 0; cur_mob.type = 3; cur_mob.row = tile_row; cur_mob.wd = 0;
	add_mob();
	remove_loose_pub(curr_tilepos, curr_room);   /* 1375:17FC */
	hook_mob_mark(0, 1);   /* 1375:2296(0), x shifted by -0x140 when in the left room */
}
/* 347C:12FA: set (open) or clear bit 0x80 of the current tile's attribute; a change looks at the neighbours (their
 * redraw) when the tile's low bits are clear */
static void tile7_mark(int open)
{
	uint16_t *a = (uint16_t *)&ROOM_ATTRS(curr_room)[curr_tilepos]; uint16_t si = *a; int changed;
	if (open) { changed = !(si & 0x80); if (changed) si |= 0x80; }
	else { changed = si & 0x80; if (changed) si &= 0xFF7F; }
	if (!changed) return;
	/* 1375:2620 / 0EB8 redraw it when visible */
	if (!(si & 3) && tile_col != 9) {
		uint8_t room = curr_room; int8_t row = tile_row;
		get_tile(row, tile_col + 1, room);   /* (redrawn) */
		get_tile(tile_row, tile_col - 1, curr_room);
	}
	*(uint16_t *)&ROOM_ATTRS(curr_room)[curr_tilepos] = si;
}
/* 347C:1226: the run of tiles 7 around column col of a row, opened or closed */
static void tile7_run(int open, uint8_t room, int8_t col, int8_t row)
{
	int8_t end = col + 3 < 10 ? 10 : col + 3, c = col - 2 > 0 ? 0 : col - 2;
	uint8_t t;
	do { t = get_tile(row, c, room); c++; } while (c != end && t != 7);
	if (t != 7) return;
	while (get_tile(tile_row, tile_col - 1, curr_room) == 7) ;
	get_tile(tile_row, tile_col + 1, curr_room);
	do { tile7_mark(open); } while (get_tile(tile_row, tile_col + 1, curr_room) == 7);
}
/* 347C:12C8 (34A3:1058; standing, crouching: 2FDF:06B2 / 0750 / 084E on kind 4): facing a tile 7 opens its run */
void ruins_open_tile7(void)
{
	if (Char.f19 == 0x11) return;
	if (get_tile_infrontof(1) != 7 || (curr_modifier & 3) == 3) return;
	tile7_run(1, Char.room, Char.curr_col, Char.curr_row);
}

/* 0AAC:0274 from the tick (a story scene in the middle of play): the platform's shell plays it (shell.c); headless,
 * nothing plays and it is not cut short. Returns 2 when a key cut it short. */

void sound_1611_0826(uint16_t n); void state_ds_range(uint16_t lo, uint16_t len, uint8_t *buf, int load);
/* 37F0:007C (OVL14, through 2A31:0E1B when the prince takes the sword in level 8's room 9): scene 6, then the level
 * reloaded in full (1286:01F2) with the level's state (DS:2BB8, 0x2EF9 bytes) put back, the prince in room 9 in seq
 * 0xE7, the sword taken (DS:2BB2) */
void sword_scene(void)
{
	static uint8_t saved[0x2EF9];
	state_ds_range(0x2BB8, sizeof saved, saved, 0);   /* (194C:17FC a block, the copy) */
	int r = core_play_scene(6);
	if (r == 2) drawn_room = 0;
	word_2b96 = 0;   /* 169B:018E */
	/* 1286:03B6(4, 5): the kind initialiser (kind 4 sets nothing kept) */
	loadkid(); Char.direction = 0; seqtbl_offset_char(0xE7);
	Char.room = next_room = 9; char_y_to_floor(); Char.hp_delta = Char.f12;
	last_scene = 6; load_level_ex(8, 1);   /* DS:0998 = 8; 1286:01F2(8) */
	state_ds_range(0x2BB8, sizeof saved, saved, 1);
	word_2bb2 = 1;
	if (r != 2) room_load(9);   /* 0CD6:02BE */
	/* 1375:0F5A(0xA, the prince's box) and 1375:0E8C(0x19): redraw marks */
	play_seq(); Kid = Char; control_rest(); ctrl1_shift = 0;
	if (!sound_playing(0x280F)) sound_1611_01a8(0xFF); else sound_1611_0826(0xFF);
}
