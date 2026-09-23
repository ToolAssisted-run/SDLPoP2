/* The room's tiles for drawing (17C1) and the tile pass that fills the draw tables (0FB3:0122 / 01CA / 03DC / 0594 /
 * 0624 / 0712 / 0858, the table adders 0993:0124 / 019C / 0220 / 0330). Transcribed from the disassembly; the tile
 * types' own drawers are per level kind (render_kind_*.c, the far pointers at DS:[0x6188]). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "globals.h"
#include "render.h"
#include "render_tiles.h"

/* the tiles around the one being drawn (tile byte + 4-byte modifier each, 5 bytes in the original) */
tile_mod left_col[3], right_col[3];        /* DS:5CF4 (column -1 of rows 0..2) / DS:619A (column 10) */
tile_mod row_above[11], row_below[11];     /* DS:6148 / DS:61AA: columns -1..9 of the row above / below */
tile_mod cur_tile, left_tile, right_tile;  /* DS:6B72+5CF0 / DS:618E / DS:6194 */
int8_t draw_row, draw_col;                 /* DS:6B6E / DS:6B6F */
uint16_t redraw_all_flag;                  /* DS:610E: a whole-room redraw in progress */
const kind_drawers *tile_drawers;          /* DS:6188: the level kind's tile drawers */

static const uint8_t *room_tiles(uint8_t room) { return room ? level.tiles[room - 1] : tiles0; }   /* DS:2B9A + 30 * room */
int description_row2_check(void) { return render_desc_loaded() && render_desc_bg() == 0x1F; }   /* 0CD6:0666: background 0x1F */

/* 17C1:03A8: the tile (and modifier) at (col, row) of room, from the caches for columns -1 / 10; room 0 is outside
 * the level: `outside` (with a modifier by level kind) */
static uint8_t tile_for_draw(uint8_t room, int8_t col, int8_t row, tile_mod *out, uint8_t outside)
{
	if (row == -1) row = 2;
	if (col == -1) { *out = left_col[row]; return out->tile; }
	if (col == 10) { *out = right_col[row]; return out->tile; }
	if (room != 0) {
		int tp = (int8_t)(row_tilepos(row) + col);
		out->tile = curr_room_tiles[tp]; out->mod = curr_room_attrs[tp];
		return out->tile;
	}
	out->tile = outside;
	if (level_kind == 3 && (outside == 1 || outside == 0x14) && room_B != 0) {
		uint8_t t = room_tiles(room_B)[col];   /* (the room below's top row) */
		if (!tile_is_wall_kind(t)) { int v = col & 7; if (v < 1) v = 1; out->mod = (uint32_t)v << 8; }
		else out->mod = 0;
	} else if (level_kind == 2) out->mod = 0x00090000u;
	else if (level_kind == 4) out->mod = 0;
	else out->mod = (uint32_t)(0x09 | (col % 2) << 8) << 16;
	return out->tile;
}
/* 17C1:0008 */
void get_room_address_draw(uint8_t room) { if (room) { curr_room_tiles = level.tiles[room - 1]; curr_room_attrs = ROOM_ATTRS(room); } }
/* 17C1:08B6: column -1 (the left room's column 9) and column 10 (the right room's column 0) of rows 0..2 */
void load_side_columns(void)
{
	uint8_t outside = drawn_room == 0 ? 0 : 0x14;   /* (cmp [5cde],1; cmc; sbb; and 0x14: 0x14 unless drawn_room is 0) */
	get_room_address_draw(room_L);
	for (int8_t r = 0; r < 3; r++) {
		tile_for_draw(room_L, 9, r, &left_col[r], outside);
		/* the description's background 0x1F keeps row 2's flags; otherwise bits 0xC000 of the modifier byte 1 */
		if (room_bg != 0 && !(room_bg - 1 == 0x1F && r == 2)) left_col[r].mod |= 0xC000u; else left_col[r].mod &= ~0xC000u;
	}
	get_room_address_draw(room_R);
	for (int8_t r = 0; r < 3; r++) tile_for_draw(room_R, 0, r, &right_col[r], outside);
	get_room_address_draw(drawn_room);
}
/* 17C1:01BA: the row above (draw_row - 1: this room's, or the room above's row 2) */
void load_row_above(void)
{
	if (draw_row == -1) return;
	uint8_t save_B = room_B, room, left;
	if (draw_row == 0) { room = room_A; left = room_AL; room_B = drawn_room; }
	else { room = drawn_room; left = room_L; }
	int8_t row = (int8_t)(draw_row - 1);
	uint8_t outside = (byte_5ce7 == 0 && level_kind != 4 && level_kind != 5) ? 1 : 0;
	get_room_address_draw(room);
	for (int8_t c = 1; c < 11; c++) {
		tile_for_draw(room, (int8_t)(c - 1), row, &row_above[c], outside);
		if (byte_5ce7 && draw_row == 0 && level_kind == 3 && tile_is_floor(room_tiles(drawn_room)[c - 1]))
			row_above[c].mod = (row_above[c].mod & ~0xC000u) | 0x4000u;   /* (byte 1: and 0x3F, or 0x40) */
	}
	get_room_address_draw(left);
	tile_for_draw(left, 9, row, &row_above[0], outside);
	if (room_bg != 0 && draw_row == 0) row_above[0].mod |= 0xC000u;   /* (17C1:02D4: the last column's entry, as the original) */
	get_room_address_draw(drawn_room);
	room_B = save_B;
}
/* 17C1:02FE: the row below */
void load_row_below(void)
{
	uint8_t room, left; int8_t row;
	if (draw_row == 2) { room = room_B; left = room_BL; row = 0; }
	else { room = drawn_room; left = room_L; row = (int8_t)(draw_row + 1); }
	get_room_address_draw(room);
	for (int8_t c = 1; c < 11; c++) tile_for_draw(room, (int8_t)(c - 1), row, &row_below[c], 0);
	get_room_address_draw(left);
	tile_for_draw(left, 9, row, &row_below[0], room == 0 ? 0 : 0x14);   /* (cmp room,1; cmc; sbb; and 0x14) */
	get_room_address_draw(drawn_room);
}
/* 17C1:07D4: the tile being drawn and its left and right neighbours */
void load_cur_tiles(void)
{
	uint8_t outside;
	if ((draw_row == 2 && !redraw_all_flag) || draw_row == -1) outside = (level_kind == 4 || level_kind == 5) ? 0 : 1;
	else outside = 0x14;
	if (!redraw_all_flag) tile_rect(draw_row, draw_col, draw_clip);   /* 17C1:0112 */
	else memcpy(draw_clip, screen_rect, sizeof draw_clip);
	tile_for_draw(drawn_room, draw_col, draw_row, &cur_tile, outside);
	tile_for_draw(drawn_room, (int8_t)(draw_col - 1), draw_row, &left_tile, outside);
	tile_for_draw(drawn_room, (int8_t)(draw_col + 1), draw_row, &right_tile, outside);
	if (draw_row == -1 && room_bg != 0) { cur_tile.mod |= 0xC000u; left_tile.mod |= 0xC000u; right_tile.mod |= 0xC000u; }
}

/* ---- helpers of 17C1 used by the drawers ---- */
int16_t screen_rect[4] = {0, 0, 192, 320};   /* DS:097E (loaded from the data segment by render_tiles_init) */
/* 17C1:0112: the box of tile (col, row): 32 wide, 63 high up to the row's floor line, cut to the screen; with a
 * description, row 0's box starts 3 higher */
void tile_rect(int8_t row, int8_t col, int16_t *r)
{
	int16_t x = (int16_t)ds_word(0x0D26 + 2 * col), y = (int16_t)ds_word(0x0D40 + 2 * row);
	r[1] = x; r[3] = (int16_t)(x + 0x20); r[2] = y; r[0] = (int16_t)(y - 0x3F);
	int16_t t[4]; memcpy(t, r, sizeof t);
	int16_t top = t[0] > screen_rect[0] ? t[0] : screen_rect[0], left = t[1] > screen_rect[1] ? t[1] : screen_rect[1];
	int16_t bot = t[2] < screen_rect[2] ? t[2] : screen_rect[2], right = t[3] < screen_rect[3] ? t[3] : screen_rect[3];
	if (top >= bot || left >= right) r[0] = r[1] = r[2] = r[3] = 0; else { r[0] = top; r[1] = left; r[2] = bot; r[3] = right; }
	if (byte_5ce7 && row == 0) r[0] -= 3;
}
/* 17C1:06BA: the tile left of (col, row) (column 9 of the room left for column 0), 0x14 outside */
uint8_t tile_at_left(int8_t row, int8_t col)
{
	uint8_t room;
	if (col != 0) { room = (row == 0 && draw_row == -1) ? room_B : drawn_room; col--; }
	else { room = (row == -1 && draw_row == -1) ? room_AL : room_L; col = 9; }
	if (!room) return 0x14;
	return room_tiles(room)[(int8_t)(row_tilepos(row) + col)];
}
/* 17C1:04F6: the tile above (col, row) (row 2 of the room above for row 0; columns out of the room in the room left or
 * right), 1 outside the level */
uint8_t tile_above(int8_t row, int8_t col)
{
	uint8_t room;
	if (row > 0) { room = drawn_room; row--; } else { room = room_A; row = 2; }
	if (col < 0) { room = level_links(room)[0]; col += 10; } else if (col >= 10) { room = level_links(room)[1]; col -= 10; }
	return room ? room_tiles(room)[(int8_t)(row_tilepos(row) + col)] : 1;
}
/* 17C1:0584: the tile above and right of (col, row) */
uint8_t tile_above_right(int8_t row, int8_t col)
{
	uint8_t room;
	if (row > 0) { room = drawn_room; row--; } else { room = room_A; row = 2; }
	if (col < -1) { room = level_links(room)[0]; col += 9; } else if (col >= 9) { room = level_links(room)[1]; col -= 9; } else col++;
	return room ? room_tiles(room)[(int8_t)(row_tilepos(row) + col)] : 1;
}
/* 17C1:0738: the tile right of (col, row) (column 0 of the room right for column 9); row 3 is the room below's row 0 */
uint8_t tile_at_right(int8_t row, int8_t col)
{
	uint8_t room;
	if (col != 9) { room = (row == 0 && draw_row == -1) ? room_B : drawn_room; col++; }
	else { room = (row == -1 && draw_row == -1) ? room_AR : room_R; col = 0; }
	if (row == 3) { room = room == room_R ? room_BR : room_B; row = 0; }
	if (!room) return 0x14;
	return room_tiles(room)[(int8_t)(row_tilepos(row) + col)];
}

/* ---- the table adders ---- */
uint8_t *piece_table;
static int add_piece(draw_entry *tab, uint16_t *count, int max, uint8_t tile, int off, uint8_t chtab, int16_t id, int8_t col, int8_t row, uint8_t mode, uint8_t mirror)
{
	if (*count >= max || !piece_table) return 0;
	const uint8_t *p = piece_table + 0x13 * tile + off;
	int16_t piece[3] = {(int16_t)(p[0] | p[1] << 8), (int16_t)(p[2] | p[3] << 8), (int16_t)(p[4] | p[5] << 8)};
	draw_entry *e = &tab[*count];
	if (!render_set_entry(e, chtab, id, piece, (uint8_t)col, (uint8_t)row, mode, mirror)) return 0;
	e->piece = piece_table[0x13 * tile];
	(*count)++;
	return 1;
}
/* 0993:0124 (table 0, the piece at +1) */
int add_back_piece_a(uint8_t tile, int16_t id, int8_t col, int8_t row, uint8_t mode, uint8_t mirror) { return add_piece(back_table, &table_counts[0], BACK_MAX, tile, 1, 4, id, col, row, mode, mirror); }
/* 0993:019C (table 0, the piece at +7; nothing for image id 0) */
int add_back_piece_b(uint8_t tile, int16_t id, int8_t col, int8_t row, uint8_t mode, uint8_t mirror) { return id ? add_piece(back_table, &table_counts[0], BACK_MAX, tile, 7, 4, id, col, row, mode, mirror) : 0; }
/* 0993:0220 (table 1, the piece at +7) */
int add_fore_piece_b(uint8_t tile, int16_t id, int8_t col, int8_t row, uint8_t mode, uint8_t mirror) { return add_piece(fore_table, &table_counts[1], FORE_MAX, tile, 7, 4, id, col, row, mode, mirror); }
/* 0993:0330 (table 1, the piece at +0xD; tiles 0x0A and 0x3C from image set 1, 0x16 from set 0) */
int add_fore_piece_c(uint8_t tile, int16_t id, int8_t col, int8_t row, uint8_t mode, uint8_t mirror)
{
	uint8_t chtab = (tile == 0x0A || tile == 0x3C) ? 1 : tile == 0x16 ? 0 : 4;
	return add_piece(fore_table, &table_counts[1], FORE_MAX, tile, 0xD, chtab, id, col, row, mode, mirror);
}

/* 0FB3:24B4: a potion's bubble colours (the draw mode) from its modifier's top bits */
static uint8_t potion_mode(uint8_t m)
{
	switch (m & 0xE0) { case 0xA0: return 0xEE; case 0x20: case 0x40: return 0xEA; default: return 0xEC; }
}
/* 0FB3:2394: tile 0x0A (a floor with a potion), the same for every kind: the kind's floor under it (outside
 * descriptions, with the modifier's low byte 0), then in the foreground the flask (image set 1: 1, or 2 for the
 * modifier top bits 0x40..0x80) and the two bubble pieces */
void draw_tile_0a(tile_args *a)
{
	if (tile_drawers && tile_drawers->by_tile[1] && room_bg == 0) { tile_args b = *a; b.mod &= ~0xFFu; tile_drawers->by_tile[1](&b); }
	if (a->layer != 1) return;
	int si = a->mod & 0xE0, di = si >= 0x40 && si < 0xA0;
	add_fore_piece_c(0x0A, (int16_t)(di + 1), a->col, a->row, 0xA, 0);
	if (!si) return;
	si = a->mod & 0x1F;
	if (si == 0x1C || (si >= 0xA && si <= 0xD) || si - 0xE >= 0xA) return;
	uint8_t mode = potion_mode((uint8_t)a->mod), mirror = 0;
	if (si > 0xD) { si -= 0xE; mirror = 1; }
	int16_t y = di ? -0x11 : -0x0E; piece_table[0x485] = (uint8_t)y; piece_table[0x486] = (uint8_t)(y >> 8);   /* (tile 0x3C's third piece y) */
	add_fore_piece_c(0x3C, (int16_t)(si + 3), a->col, a->row, mode, mirror);
	add_fore_piece_c(0x3C, (int16_t)(si + 0xD), a->col, a->row, (uint8_t)(mode + 1), mirror);
}

/* ---- the tile pass ---- */
static void call_drawer(tile_args *a) {  if (tile_drawers && a->tile < 0x2C && tile_drawers->by_tile[a->tile]) tile_drawers->by_tile[a->tile](a); }
static int wall7(uint8_t t) { return t != 7 && tile_is_wall_kind(t); }
/* 0FB3:0594: which of the four pieces 03DC starts with */
static int first_piece(void)
{
	int dx = redraw_all_flag && ((wall7(cur_tile.tile) && wall7(left_tile.tile)) || (draw_col != 0 && draw_row != 2));
	if (dx) return 3;
	if (redraw_all_flag && draw_col == 0 && draw_row != 2) return 2;
	if (room_bg != 0 && draw_row == 2 && !description_row2_check()) return 2;   /* 0CD6:0666 */
	return 0;
}
/* 0FB3:0624 (the tile's own piece comes next): the room description's objects (render_desc.c) */
static void before_own_piece(uint8_t layer) { render_desc_objects(layer); }
/* 0FB3:03DC: the pieces of layer `layer` from the tiles below-left, below, left and this one */
static void draw_layer(uint8_t layer)
{
	int n = first_piece();
	if (redraw_all_flag) memcpy(draw_clip, screen_rect, sizeof draw_clip);
	for (; n < 4; n++) {
		tile_args a; a.layer = layer; tile_mod t;
		switch (n) {
		case 0:
			a.col = (int8_t)(draw_col - 1); a.row = (int8_t)(draw_row + 1); t = row_below[draw_col];
			if (redraw_all_flag && draw_col != 0 && draw_col != 9 && level_kind != 3 && t.tile != 4 && level_number != 13
			    && !(level_kind == 2 && (t.tile == 0x13 || t.tile == 9))) n += 2;
			break;
		case 1:
			a.row = (int8_t)(draw_row + 1); a.col = draw_col; t = row_below[draw_col + 1];
			if (tile_is_wall_kind(t.tile)) { n++; a.col = (int8_t)(draw_col - 1); a.row = draw_row; t = left_tile; }
			break;
		case 2: a.col = (int8_t)(draw_col - 1); a.row = draw_row; t = left_tile; break;
		default: before_own_piece(layer); a.col = draw_col; a.row = draw_row; t = cur_tile; break;
		}
		a.tile = t.tile; a.mod = t.mod;
		int v = (a.mod >> 14) & 3;   /* (bits 6..7 of the modifier's byte 1) */
		if (room_bg == 0 || v == 0 || layer == v || (v == 2 && layer == 5)) call_drawer(&a);
	}
}
/* 0FB3:1BF2: the frame's sprite of a character (the sprite list, not reconstructed yet): its record's rect */
__attribute__((weak)) const int16_t *sprite_rect_of(int kind, uint8_t charid) { (void)kind; (void)charid; return NULL; }
extern void caverns_teeth(tile_args *a);   /* 34C1:0774 */
/* a character that the teeth of the chomper at the left tile (4) close on: on this row, in the chomper's column or
 * the one after this one, not in action 2 or 6 */
static int bitten(const char_type *c)
{
	return left_tile.tile == 4 && c->curr_row == draw_row && (c->curr_col == draw_col - 1 || draw_col - c->curr_col == 2) && c->action != 2 && c->action != 6;
}
/* 0FB3:0984: the chomper (4) in front of the character it bites: the kind's special drawer (DS:618A) in layer 2 on
 * the left tile, clipped to the character's sprite (whole-room builds: to below the row's top); the caverns' teeth
 * (34C1:0774) while the prince is in frames 0x108..0x10A. (The original loads the characters into Char to test them:
 * here copies.) */
static void draw_special(void)
{
	tile_args b;
	if (Kid.frame >= 0x108 && Kid.frame <= 0x10A) {
		if (level_kind != 3) return;
		b.layer = 0; b.row = draw_row;
		if (cur_tile.tile == 4) { b.col = draw_col; b.tile = 0; b.mod = cur_tile.mod; caverns_teeth(&b); }
		else if (left_tile.tile == 4) { b.col = (int8_t)(draw_col - 1); b.tile = 0; b.mod = left_tile.mod; caverns_teeth(&b); }
		return;
	}
	if (left_tile.tile != 4) return;
	int16_t save[4]; memcpy(save, draw_clip, sizeof save);
	const int16_t *sr = NULL; int hit = bitten(&Kid);
	if (hit) sr = sprite_rect_of(2, 0);
	else {
		int n = room_nchars(drawn_room);
		for (int i = 0; i < n && i < 5 && !hit; i++) if (bitten(&chars[i])) { hit = 1; sr = sprite_rect_of(3, chars[i].charid); }
	}
	if (hit) {
		b.layer = 2; b.col = (int8_t)(draw_col - 1); b.row = draw_row; b.tile = 0; b.mod = left_tile.mod;
		if (tile_drawers && tile_drawers->special) {
			if (redraw_all_flag) draw_clip[0] = (int16_t)(0x3F * draw_row + 3);
			else if (sr) { int16_t r[4]; memcpy(r, draw_clip, sizeof r);
				draw_clip[0] = r[0] > sr[0] ? r[0] : sr[0]; draw_clip[1] = r[1] > sr[1] ? r[1] : sr[1]; draw_clip[2] = r[2] < sr[2] ? r[2] : sr[2]; draw_clip[3] = r[3] < sr[3] ? r[3] : sr[3];
				if (draw_clip[0] >= draw_clip[2] || draw_clip[1] >= draw_clip[3]) draw_clip[0] = draw_clip[1] = draw_clip[2] = draw_clip[3] = 0; }
			if (!(draw_clip[0] >= draw_clip[2] || draw_clip[1] >= draw_clip[3])) tile_drawers->special(&b);
		}
	}
	memcpy(draw_clip, save, sizeof save);
}
/* 0FB3:0858: the fore layer (1) of the left tile, this tile and the tile above */
static void draw_fore(void)
{
	draw_special();
	int si = redraw_all_flag && draw_col != 0, n = si;
	for (;;) {
		tile_args a; a.layer = 1; tile_mod t;
		if (n == 0) { a.col = (int8_t)(draw_col - 1); a.row = draw_row; t = left_tile; }
		else if (n == 1) { a.col = draw_col; a.row = draw_row; t = cur_tile; if (si && level_kind != 3) n++; }
		else { a.row = (int8_t)(draw_row - 1); a.col = draw_col; t = row_above[draw_col + 1]; }
		a.tile = t.tile; a.mod = t.mod;
		int v = (a.mod >> 14) & 3;
		if (room_bg == 0 || v != 3) call_drawer(&a);
		n++;
		if (n >= 3) break;
		if (n >= 2 && draw_row == -1) break;
	}
	before_own_piece(1);
}
void draw_one_tile(void) { draw_layer(0); draw_layer(5); draw_layer(0xB); draw_fore(); }
void render_draw_layer(uint8_t layer) { draw_layer(layer); }   /* 0FB3:03DC */
void render_draw_fore(void) { draw_fore(); }                    /* 0FB3:0858 */
/* 0FB3:0122: the whole room, rows 2..0 then the row above's bottom row */
void draw_room_tiles(void)
{
	load_side_columns();
	for (draw_row = 2; draw_row >= 0; draw_row--) {
		load_row_above(); load_row_below();
		for (draw_col = 0; draw_col < 10; draw_col++) { load_cur_tiles(); draw_one_tile(); }
	}
	draw_row = -1;
	if (byte_5ce7 == 0) {
		uint8_t si = drawn_room;
		drawn_room = room_A; set_neighbour_rooms();
		room_B = si;
		load_side_columns();
		draw_row = 2; load_row_above(); load_row_below();
		draw_row = -1;
		for (draw_col = 0; draw_col < 10; draw_col++) { load_cur_tiles(); draw_one_tile(); }
		drawn_room = si; set_neighbour_rooms();
	} else for (int i = 0; i < 11; i++) { row_above[i].tile = 0; row_above[i].mod = 0xC000; }   /* 0CD6:0142 (a description room: nothing above) */
}

/* DS:[0x6188] per level kind (0FB3 loads the kind's overlays): 1 desert, 2 temple, 3 caverns, 4 ruins, 5 rooftops,
 * 6 the final level */
const kind_drawers *kind_drawers_for(int kind)
{
	switch (kind) { case 1: return &kind_desert; case 2: return &kind_temple; case 3: return &kind_caverns; case 4: return &kind_ruins; case 5: return &kind_rooftops; case 6: return &kind_final; default: return NULL; }
}
