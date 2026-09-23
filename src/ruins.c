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
