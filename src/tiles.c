/* Tile addressing (0AFF:000C..0174, 17C1:0008) and tile-class predicates (0FB3:2810..290A). */
#include "types.h"
#include "globals.h"

uint8_t curr_tile; uint16_t curr_modifier; uint8_t curr_tilepos, curr_room; int8_t tile_col, tile_row;
uint8_t *curr_room_tiles; uint32_t *curr_room_attrs;   /* DS:613C / 613A */

/* 17C1:0008 */
void get_room_address(uint8_t room)
{
	if (room) { curr_room_tiles = level.tiles[room - 1]; curr_room_attrs = level.attrs[room]; }
}
/* 0AFF:07D4: tilepos of the first column of a row (negative rows wrap like PoP1's tbl_line) */
static int8_t row_to_tilepos(int8_t row) { return row >= 0 ? row * 10 : row * 10 + 9; }

/* 0AFF:000C: resolve tile_col/tile_row into the neighbouring room through roomlinks[curr_room] */
uint8_t find_room_of_tile(void)
{
	uint8_t next = curr_room;
	if (tile_col < 0) {
		if (curr_room && (next = level_links(curr_room)[0]) != 0) { tile_col += 10; curr_room = next; return find_room_of_tile(); }
	} else if (tile_col > 9) {
		if (curr_room && (next = level_links(curr_room)[1]) != 0) { tile_col -= 10; curr_room = next; return find_room_of_tile(); }
	}
	if (curr_room) {
		if (tile_row < 0) { tile_row += 3; next = level_links(curr_room)[2]; curr_room = next; return find_room_of_tile(); }
		if (tile_row > 2) { tile_row -= 3; next = level_links(curr_room)[3]; curr_room = next; return find_room_of_tile(); }
	}
	return curr_room;
}

/* 0AFF:00FA */
uint8_t get_tile(int8_t row, int8_t col, uint8_t room)
{
	curr_room = room; tile_col = col; tile_row = row;
	curr_room = find_room_of_tile();
	curr_tilepos = row_to_tilepos(tile_row) + tile_col;
	if (curr_room == 0) { curr_modifier = 0; curr_tile = level_edge_tile(row, col); }   /* 0AFF:0174: per-level edge rules */
	else { get_room_address(curr_room); curr_tile = curr_room_tiles[curr_tilepos]; curr_modifier = (uint16_t)curr_room_attrs[curr_tilepos]; }
	return curr_tile;
}
uint8_t get_tile_at_char(void)       { return get_tile(Char.curr_row, Char.curr_col, Char.room); }           /* 0AFF:09E6 */
uint8_t get_tile_above_char(void)    { return get_tile(Char.curr_row - 1, Char.curr_col, Char.room); }       /* 0AFF:14E2 */
uint8_t get_tile_behind_char(void)  { return get_tile(Char.curr_row, dir_behind[Char.direction + 1] + Char.curr_col, Char.room); } /* 0AFF:0F94 (table DS:0CFA) */
uint8_t get_tile_infrontof(int8_t n)   { return get_tile(Char.curr_row, dir_front[Char.direction + 1] * n + Char.curr_col, Char.room); } /* 0AFF:0F6C */

/* 0FB3:28D4 / 290A / 2810 / 283C / 2878 - tile classes (PoP1 numbering plus PoP2 additions) */
int tile_is_empty_kind(uint8_t t) { return t == 0 || t == 9 || t == 33 || t == 35 || t == 27 || t == 37; }
int tile_is_wall_kind(uint8_t t)  { return t == 20 || t == 2 || t == 7 || t == 25 || t == 43; }
int tile_is_floor(uint8_t t)      { return !tile_is_wall_kind(t) && !tile_is_empty_kind(t); }
int tile_is_loose_kind(uint8_t t) { return t == 11 || t == 15 || t == 26 || t == 12 || t == 13 || t == 23 || t == 24; }
int tile_is_solid_floor(uint8_t t){ return tile_is_floor(t) && !tile_is_loose_kind(t) && t != 6 && t != 34; }

/* OVL01 2FDF:000E - start a sequence (PoP1 seqtbl_offset_char) */
void seqtbl_offset_char(uint16_t seq_id)
{
	if (Char.charid == 1) shadow_hook_2f9a2();
	if (!get_seq_resource(seq_id)) return;
	Char.seq_id = seq_id; Char.f19 = seq_id; Char.seq_pos = 0;
	if ((Char.charid == 7 || Char.charid == 8) && ((seq_id > 0x81 && seq_id < 0x8C) || seq_id == 0xA6) && Char.direction == 0) {
		Char.direction = -1;
		if (Char.frame == 0x9E || Char.frame == 0x96) Char.x = char_dx_forward(14);
		load_fram_det_col();
	}
}

/* 0AFF:0174 - what a tile outside the level (room 0) counts as: wall (20) or empty (0), per level type */
uint8_t level_edge_tile(int8_t row, int8_t col)
{
	switch (level_kind) {
	case 2:
		if (drawn_room == 4 && level_number == 13) return 0;
		if (level_number == 13 && drawn_room == 13) return 0;
		return 20;
	case 4:
		if (room_A == 0 && row == -1) return 0;
		if (drawn_room == 27 && level_number == 6) return 0;
		if (drawn_room == 16 && level_number == 9) return 0;
		return 20;
	case 5:
		return 0;
	case 6:
		if (drawn_room == 7 || drawn_room == 8) return 20;
		if (drawn_room == 3 && (col < 0 || col == 9) && row == 2) return 20;
		return 0;
	default:
		return 20;
	}
}
