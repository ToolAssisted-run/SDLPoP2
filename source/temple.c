/* Level kind 2 (temple, levels 10..13): OVL07 loaded at 347C. */
#include "types.h"
#include "globals.h"

/* 347C:0000: a torch flickers: off -> 1 or 2, lit -> off */
int temple_torch(int cur) { return cur ? 0 : random_2751(2) + 1; }
/* 347C:0ECA: the tile 0x1A at (room, tp) gives way: the tile empties and a type-10 object (the falling slab) starts */
static void slab_drop(uint8_t room, int8_t tp)
{
	uint16_t mod = curr_modifier;
	*(uint16_t *)((uint8_t *)&level + 0x348 + (room * 30 + tp) * 4) = 0x81;   /* DS:2F00 */
	curr_room_tiles[tp] = 0;
	cur_mob.w7 = 0; cur_mob.speed = 0; cur_mob.x = (tp % 10) * 32;   /* DS:0D26 */
	cur_mob.wd = (mod & 0x40) ? 0x4B : 1;
	cur_mob.room = room;
	static const int16_t row_y[3] = {66, 129, 192};   /* DS:0D40 */
	cur_mob.y = row_y[tile_row]; cur_mob.row = tp / 10; cur_mob.type = 10;
	add_mob();
	play_sound(0x5E);   /* 1375:2296(0) redraws */
}
/* 347C:0E48 (after a character moves): standing on an intact 0x1A slab drops it */
void slab_step(void)
{
	if (!(frame_flags & 0x40) || 63 * Char.curr_row + 0x38 != Char.y || Char.charid == 1) return;
	if (get_tile(Char.curr_row, Char.curr_col, Char.room) == 0x1A && !(curr_modifier & 0x1F)) slab_drop(curr_room, curr_tilepos);
}
/* 347C:0E8E (a landing shakes the row, curr_tile = 0x1A): slabs not under the character get marked first */
void slab_shake(void)
{
	if (curr_modifier & 0x40) return;
	if (curr_tilepos % 10 != Char.curr_col) { curr_modifier |= 0x40; curr_room_attrs[curr_tilepos] = (curr_room_attrs[curr_tilepos] & ~0xFFFFu) | curr_modifier; }
	slab_drop(curr_room, curr_tilepos);
}
/* 347C:0E10: mob type 10 (a fallen slab) settles */
void slab_mob(void)
{
	if (cur_mob.speed == -1) return;
	if ((cur_mob.wd & 0x1F) <= 0x10) cur_mob.wd++;
	/* sound 0x5F unless 0x276E / 0x276F play */
}
/* 347C:0C22 (drawing a type-10 mob, 1375:1FBA every frame): a settled slab goes back up to its place */
void slab_draw_state(void)
{
	int8_t col = (int8_t)((cur_mob.x < 0 ? -((-cur_mob.x) >> 5) : cur_mob.x >> 5));
	int8_t tp = (int8_t)(row_tilepos((int8_t)cur_mob.row) + col);
	if ((cur_mob.wd & 0x1F) > 0x10) {
		cur_mob.speed = -1;
		*(uint8_t *)&ROOM_ATTRS(cur_mob.room)[tp] = 0; ROOM_TILES(cur_mob.room)[tp] = 0x1A;
		/* sound 0x276F stops, sound 3 */
	}
}
/* 347C:0FC4: the kind's tick */
void temple_tick(void) { if (drawn_room == 4 && level_number == 13) shadow13_tick(); }   /* 37F0:0236 (shadow13.c) */
