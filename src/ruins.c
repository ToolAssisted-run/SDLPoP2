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
	/* 1375:2296 redraws (with x shifted by -0x140 when in the left room) */
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
