/* Animated tiles (1375:0006 / 0096, PoP1's trobs): a list of {tile position, room, state, tile} at DS:6676,
 * count DS:6670. Each tick the tile's handler updates a copy of the tile's attribute (DS:5CF0) that is written
 * back afterwards; a negative state removes the entry. Handlers for most tiles live in the level-kind overlays. */
#include <stdio.h>
#include "types.h"
#include "globals.h"

trob_type trobs[20]; uint16_t trob_count;        /* DS:6676, DS:6670 */
trob_type cur_trob;                              /* DS:6672 */
uint32_t anim_mod;                               /* DS:5CF0 */
uint8_t anim_tile;                               /* DS:6B72 */
static uint32_t *anim_attrs; static uint8_t *anim_tiles;

/* 17C1:0000: tile and attribute pointers of any room (room 0 = the dummy room) */
static void room_pointers(uint8_t room) { anim_tiles = room ? level.tiles[room - 1] : tiles0; anim_attrs = (uint32_t *)((uint8_t *)&level + 0x348) + room * 30; }
/* 1375:2620: is the tile on screen (drawn room, or the edge column/row shared with the left, lower and lower-left rooms)? */
static int tile_visible(int8_t tilepos, uint8_t room)
{
	if (room == drawn_room) return 1;
	if (room == room_L && tilepos % 10 == 9) return 1;
	if (room == room_B && tilepos < 10) return 1;
	if (room == room_BL && tilepos == 9) return 1;
	return 0;
}
/* kind 5 (level 1) handlers, OVL 33FD */
static void anim_torch_25(void)   /* 33FD:0652: cycles 0..7 while visible */
{
	if (!tile_visible(cur_trob.tilepos, cur_trob.room)) { cur_trob.state = 0xFF; return; }
	uint16_t v = (uint16_t)anim_mod & 0xF; v = v == 7 ? 0 : v + 1;
	anim_mod = (anim_mod & ~0xFu) | v;
}
static void anim_26(void) { uint16_t v = (uint16_t)anim_mod + 1; anim_mod = (anim_mod & 0xFFFF0000u) | v; if (v > 0xB0) cur_trob.state = 0xFF; }   /* 33FD:08A2 */
static void anim_27(void)         /* 33FD:058A */
{
	if ((uint16_t)anim_mod >= 0xB) { cur_trob.state = 0xFF; return; }
	anim_mod = (anim_mod & 0xFFFF0000u) | (uint16_t)((uint16_t)anim_mod + 1);
	if ((uint16_t)anim_mod == 1) play_sound(0x31);
}
/* 1375:0096 */
static void animate_tile(void)
{
	room_pointers(cur_trob.room);
	anim_mod = anim_attrs[(int8_t)cur_trob.tilepos]; anim_tile = anim_tiles[(int8_t)cur_trob.tilepos];   /* 1375:1A20 */
	uint8_t t = cur_trob.tile;
	if (level_kind == 5 && t == 0x25) anim_torch_25();
	else if (level_kind == 5 && t == 0x26) anim_26();
	else if (level_kind == 5 && t == 0x27) anim_27();
	else if (t < 4 || t > 0x2C) cur_trob.state = 0xFF;
	else anim_tile_other(t);
	((uint16_t *)&anim_attrs[(int8_t)cur_trob.tilepos])[0] = (uint16_t)anim_mod;
}
/* 1375:0006 */
void animate_tiles(void)
{
	if (trob_count == 0) return;
	int removed = 0;
	for (int i = 0; i < (int16_t)trob_count; i++) {
		cur_trob = trobs[i]; animate_tile(); trobs[i].state = cur_trob.state;
		if ((int8_t)cur_trob.state < 0) removed = 1;
	}
	if (removed) { int n = 0; for (int i = 0; i < (int16_t)trob_count; i++) if ((int8_t)trobs[i].state >= 0) trobs[n++] = trobs[i]; trob_count = n; }
}
/* 1375:1414: start (or restart) the animation of a tile */
void add_trob(uint8_t tile, uint8_t state, int8_t tilepos, uint8_t room)
{
	for (int i = 0; i < (int16_t)trob_count; i++) if (trobs[i].tilepos == tilepos && trobs[i].room == room) { trobs[i].state = state; return; }   /* 1375:2598 */
	if (trob_count < 20) { trob_type t = {tilepos, room, state, tile}; trobs[trob_count++] = t; }
}

/* 0823:0B78: start the animations of the tiles on screen: the drawn room, the left room's right column and
 * the lower room's top row (the left column comes from the draw buffer DS:5C5E in the original; same tiles) */
void start_room_anims(void)
{
	uint8_t room = drawn_room; int8_t tp = 0;
	for (int si = 0; si < 0x2B; si++, tp++) {
		uint8_t t;
		if (si == 0x1E) { if (room_L) { room = room_L; room_pointers(room); } else si += 3; }
		if (si == 0x21) { if (!room_B) break; room = room_B; room_pointers(room); }
		if (si < 0x1E) { room_pointers(room); tp = si; anim_mod = anim_attrs[si]; t = anim_tiles[si]; }
		else if (si < 0x21) { tp = (si - 0x1E) * 10 + 9; t = anim_tiles[tp]; }
		else { tp = si - 0x21; anim_mod = anim_attrs[tp]; t = anim_tiles[tp]; }
		switch (t) {
		case 0x1C: case 0x1D: case 0x1F: case 0x25: case 0x26: case 0x27: case 0x28: case 0x2B: add_trob(t, 1, tp, room); break;
		case 0x02: case 0x0A: case 0x13: case 0x20: case 0x17: case 0x1E: case 0x2C: anim_start_other(t, tp, room, si); break;
		default: if ((room == 6 || room == 7 || room == 8) && level_kind == 6 && (anim_mod & 0x1000)) anim_start_other(t, tp, room, si); break;
		}
	}
	room_pointers(drawn_room);
}
