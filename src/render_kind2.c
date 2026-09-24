/* Level kind 2 (temple: levels 10 to 13): the tile types' drawers (the overlay at 3579 as loaded in level 10, reached
 * through the far pointers at DS:[0x6188]; tiles 0x0C / 0x0D are the shared 3443 ones, render_kind_common.c). Each
 * gets the layer (0, 1, 2, 5 or 0xB), the tile's column and row, its type and modifier, and adds pieces through the
 * 0993 adders. Transcribed from the disassembly; see render_kind3.c for the caverns ones. */
#include <string.h>
#include "types.h"
#include "globals.h"
#include "render.h"
#include "render_tiles.h"

typedef int (*adder)(uint8_t tile, int16_t id, int8_t col, int8_t row, uint8_t mode, uint8_t mirror);
#define A_BACK_A add_back_piece_a   /* 0993:0124 */
#define A_BACK_B add_back_piece_b   /* 0993:019C */
#define A_FORE_B add_fore_piece_b   /* 0993:0220 */
#define A_FORE_C add_fore_piece_c   /* 0993:0330 */
#define NOID ((int16_t)-2)
static uint16_t mod_lo(const tile_args *a) { return (uint16_t)a->mod; }
static uint8_t mod_b1(const tile_args *a) { return (uint8_t)(a->mod >> 8); }
static void piece_word(int off, int16_t v) { piece_table[off] = (uint8_t)v; piece_table[off + 1] = (uint8_t)(v >> 8); }
static int16_t piece_get(int off) { return (int16_t)(piece_table[off] | piece_table[off + 1] << 8); }
/* 194C:5266: d = a cut to b; 0 (and d zeroed) when empty */
static int sect(int16_t *d, const int16_t *a, const int16_t *b)
{
	int16_t t = a[0] > b[0] ? a[0] : b[0], l = a[1] > b[1] ? a[1] : b[1], bo = a[2] < b[2] ? a[2] : b[2], r = a[3] < b[3] ? a[3] : b[3];
	if (t >= bo || l >= r) { d[0] = d[1] = d[2] = d[3] = 0; return 0; }
	d[0] = t; d[1] = l; d[2] = bo; d[3] = r; return 1;
}
/* 194C:647E: d = the box around a and b */
static void union_rect(int16_t *d, const int16_t *a, const int16_t *b)
{
	int16_t t = a[0] < b[0] ? a[0] : b[0], l = a[1] < b[1] ? a[1] : b[1], bo = a[2] > b[2] ? a[2] : b[2], r = a[3] > b[3] ? a[3] : b[3];
	d[0] = t; d[1] = l; d[2] = bo; d[3] = r;
}
static int rect_empty(const int16_t *r) { return r[0] >= r[2] || r[1] >= r[3]; }   /* 194C:4D50 */
/* 2699:011C: a lies inside b (the box around both is b) */
static int rect_inside(const int16_t *a, const int16_t *b) { int16_t u[4]; union_rect(u, b, a); return !memcmp(u, b, sizeof u); }
/* 17C1:016E: the rect at DS:src moved to tile (col, row) and cut to the screen; 0 when empty (or column/row 0x1E) */
static int tile_offset_rect(int8_t row, int8_t col, uint16_t src, int16_t *r)
{
	if (col == 0x1E || row == 0x1E) return 0;
	for (int i = 0; i < 4; i++) r[i] = (int16_t)ds_word((uint16_t)(src + 2 * i));
	r[0] += 63 * row; r[2] += 63 * row; r[1] += 32 * col; r[3] += 32 * col;
	return sect(r, r, screen_rect);
}
/* 17C1:0618: the tile below (col, row) (row 0 of the room below for row 2; columns out of the room in the room left or
 * right), 0x14 outside the level */
static uint8_t tile_below(int8_t row, int8_t col)
{
	uint8_t room;
	if (row < 2) { room = (row == 0 && draw_row == -1) ? room_B : drawn_room; row++; }
	else { row = 0; room = room_B; }
	if (col < 0) { room = level_links(room)[0]; col += 10; } else if (col >= 10) { room = level_links(room)[1]; col -= 10; }
	if (!room) return 0x14;
	return level.tiles[room - 1][(int8_t)(row_tilepos(row) + col)];
}
/* 0FB3:2CBC: the tile above (col, row) or above and right of it is open (empty, 5, 6, or 3 on level 13) */
static int open_above(int8_t col, int8_t row)
{
	uint8_t a = tile_above(row, col);
	if (tile_is_empty_kind(a) || a == 5 || a == 6 || (a == 3 && level_number == 0xD)) return 1;
	uint8_t b = tile_above_right(row, col);
	if (tile_is_empty_kind(b) || b == 5 || b == 6) return 1;
	return a == 3 && level_number == 0xD;   /* (tests the tile above again, as the original) */
}
/* 0993:078E: the back table's first entry of image id at (col, row), or NULL */
static draw_entry *find_back_entry(int16_t id, int8_t col, int8_t row)
{
	for (int i = 0; i < (int16_t)table_counts[0]; i++) {
		draw_entry *e = &back_table[i];
		if ((int16_t)e->id == id && e->row == (uint8_t)row && e->col == (uint8_t)col) return e;
	}
	return NULL;
}

/* the entry's clip grown to take in the drawing area when they overlap (194C:5266, 194C:647E) */
static int grow_entry(draw_entry *e)
{
	int16_t r[4], tmp[4]; memcpy(r, (uint8_t *)e + 0xA, sizeof r);
	if (!sect(tmp, r, draw_clip)) return 0;
	union_rect(r, r, draw_clip); memcpy((uint8_t *)e + 0xA, r, sizeof r);
	return 1;
}

static void d_floor(tile_args *a);
static void d_wall(tile_args *a);
static void d_13(tile_args *a);
/* 3579:000A: a wall piece of the tile left (modifier byte 2 bits 0..4: its image id, 0x14 = the pair 0x13 / 0x14;
 * byte 3 bits 0..3: the piece record 0x24 + n). Outside whole-room redraws an entry already in the back table
 * for it is grown to the drawing area instead. */
static void left_part(tile_args *a)
{
	int b3 = (uint8_t)(a->mod >> 24) & 0xF, bx = (uint8_t)(a->mod >> 16) & 0x1F, id, two, found;
	if (!bx) return;
	if (bx == 0x14) { id = 0x13; two = 1; } else { id = bx; two = 0; }
	draw_entry *e = NULL;
	if (!redraw_all_flag) {
		e = find_back_entry((int16_t)id, (int8_t)(a->col - 1), a->row);
		if (e && grow_entry(e)) {
			found = 0;
			if (two) { e = find_back_entry((int16_t)(id + 1), (int8_t)(a->col - 1), a->row); if (e) grow_entry(e); }
		} else { e = NULL; found = 1; }
	} else found = 0;
	if (e) return;
	if (!found && b3 && !(a->col == -1 && b3 <= 3) && a->row != 3) return;
	found = 1;
	int16_t top = draw_clip[0];
	if (a->row >= 0 && !open_above(a->col, a->row)) {
		int16_t y = (int16_t)ds_word((uint16_t)(0x0D3E + 2 * a->row));
		if (y > draw_clip[1]) {   /* (compares with the left edge, DS:60E0, as the original) */
			draw_clip[0] = y < top ? top : y;
			if (rect_empty(draw_clip)) found = 0;
		}
	}
	if (found) {
		b3 += 0x24;
		A_BACK_A((uint8_t)b3, (int16_t)id, a->col, a->row, 0, 0);
		if (two) A_BACK_B((uint8_t)b3, NOID, a->col, a->row, 0, 0);
	}
	draw_clip[0] = top;
}
/* 3579:01DC: wall (0x14, 0x19); the modifier's low 3 bits: 0 / 1 a face, 2 and more a face 2 lower (0x1A..) */
static void d_wall(tile_args *a)
{
	switch (a->layer) {
	case 0: if (redraw_all_flag) left_part(a); break;
	case 1: {
		int cx = (mod_lo(a) & 7) + 0x18;
		if (cx > 0x19) { if (!redraw_all_flag) { piece_word(0x18B, -0x20); cx -= 2; } else cx = -1; }
		else piece_word(0x18B, 0);
		if (cx != -1) A_FORE_C(0x14, (int16_t)cx, a->col, a->row, 0xA, 0);
		int di = (mod_lo(a) & 7) + 0x18;
		uint8_t left = tile_at_left(a->row, a->col);
		if (di <= 0x19 && tile_is_wall_kind(left)) A_FORE_C(0x14, 0x3D, a->col, a->row, 0, 0);
		if (tile_is_wall_kind(tile_below(a->row, a->col)))
			A_FORE_C(0x14, (int16_t)(tile_is_wall_kind(left) ? 0x3F : 0x3E), a->col, a->row, 0, 0);
		break; }
	case 2: case 5: {
		int dx = (mod_lo(a) & 7) + 0x18;
		if ((dx == 0x18 || dx > 0x19) && !tile_is_wall_kind(tile_at_right(a->row, a->col)))
			(a->layer == 5 ? A_BACK_B : A_FORE_B)(0x14, NOID, a->col, a->row, 0xA, 0);
		break; }
	}
}
/* 3579:0350: tile 0x10 */
static void d_10(tile_args *a)
{
	switch (a->layer) {
	case 0: if (redraw_all_flag) left_part(a); break;
	case 1: A_FORE_C(0x10, NOID, a->col, a->row, 0, 0); break;
	case 2: A_FORE_B(0x10, NOID, a->col, a->row, 0xA, 0); break;
	case 5: A_BACK_B(0x10, NOID, a->col, a->row, 0xA, 0); break;
	}
}
/* 3579:047E: the layer 5 part of tile 0x11: its piece when its object's state is 4 or more, and image 0x39 twice,
 * y from the modifier (at most 0x2A), cut to 5 above the row's top */
static void d_047e(tile_args *a)
{
	if (a->layer != 5) return;
	int16_t save[4], r[4]; memcpy(save, draw_clip, sizeof save); memcpy(r, screen_rect, sizeof r);
	r[0] = (int16_t)(63 * a->row - 5);
	if (sect(draw_clip, draw_clip, r)) {
		trob_type *t = get_trob((int8_t)(row_tilepos(a->row) + a->col), drawn_room);
		if (t && (int8_t)t->state >= 4) A_BACK_A(0x11, NOID, a->col, a->row, 0, 0);
		int v = (uint8_t)a->mod; if (v > 0x2A) v = 0x2A;
		piece_word(0x440, (int16_t)(-(v + 0x13)));
		A_BACK_A(0x39, NOID, a->col, a->row, 0xA, 0);
		uint16_t f = Kid.frame; adder add = (f >= 0xD9 && f <= 0xE2) ? A_FORE_B : A_BACK_B;
		add(0x39, NOID, a->col, a->row, 0xA, 0);
	}
	memcpy(draw_clip, save, sizeof save);
}
/* 3579:03E0: tile 0x11 */
static void d_11(tile_args *a)
{
	switch (a->layer) {
	case 0: if (redraw_all_flag) { left_part(a); d_047e(a); } break;
	case 1: A_FORE_C(0x11, NOID, a->col, a->row, 0, 0); break;
	case 2: A_FORE_B(0x11, NOID, a->col, a->row, 0xA, 0); break;
	case 5: A_BACK_B(0x11, NOID, a->col, a->row, 0xA, 0); d_047e(a); break;
	}
}
/* 3579:0656: tile 1's first piece drawn in the place of its second (layers 2 and 5) */
static void floor_first_piece(tile_args *a)
{
	adder add = a->layer == 5 ? A_BACK_B : a->layer == 2 ? A_FORE_B : NULL;
	if (!add) return;
	uint8_t save[6]; memcpy(save, piece_table + 0x1A, 6);
	memcpy(piece_table + 0x1A, piece_table + 0x14, 6);
	add(1, NOID, a->col, a->row, 0, 0);
	memcpy(piece_table + 0x1A, save, 6);
}
/* 3579:05B2: floor (0x01); the modifier's low 2 bits: the second piece replaced by the first */
static void d_floor(tile_args *a)
{
	int v = 0; adder add = NULL; uint8_t mode = 0;
	switch (a->layer) {
	case 0: left_part(a); break;
	case 1: add = A_FORE_C; mode = 0; break;
	case 2: add = A_FORE_B; mode = 0xA; v = mod_lo(a) & 3; break;
	case 5: add = A_BACK_B; mode = 0xA; v = mod_lo(a) & 3; break;
	}
	if (!add) return;
	add(1, NOID, a->col, a->row, mode, 0);
	if (v) floor_first_piece(a);
}
/* 3579:075E: the chomper's teeth (image 0x37, layers 2 and 5), y from the modifier bits 2..7 (at most 0x32), cut to
 * 10 above the row's top; also the kind's special drawer (DS:618A, called by 0FB3:0984 with layer 2) */
static void d_075e(tile_args *a)
{
	int16_t save[4], r[4]; memcpy(save, draw_clip, sizeof save); memcpy(r, screen_rect, sizeof r);
	r[0] = (int16_t)(63 * a->row - 10);
	if (sect(draw_clip, draw_clip, r)) {
		int v = ((uint8_t)a->mod & 0xFC) >> 2; if (v > 0x32) v = 0x32;
		piece_word(0x420, (int16_t)(-(v + 4)));
		adder add = a->layer == 2 ? A_FORE_B : a->layer == 5 ? A_BACK_B : NULL;
		if (add) add(0x37, NOID, a->col, a->row, 0xA, 0);
	}
	memcpy(draw_clip, save, sizeof save);
}
/* 3579:06DE: chomper (0x04) */
static void d_04(tile_args *a)
{
	adder add = NULL;
	switch (a->layer) {
	case 0: left_part(a); break;
	case 1: add = A_FORE_C; break;
	case 2: add = A_FORE_B; break;
	case 5: add = A_BACK_B; break;
	}
	if (!add) return;
	add(4, NOID, a->col, a->row, 0xA, 0);
	d_075e(a);
}
/* 3579:082A: tile 0x0B (a ledge; the modifier's low nibble through the tables DS:194E / 1966 by column parity: 0 a
 * plain floor, 0x0D nothing) */
static void d_0b(tile_args *a)
{
	int v = mod_lo(a) & 0xF;
	if (v != 0xD) v = (a->col % 2) ? (int16_t)ds_word((uint16_t)(0x194E + 2 * v)) : (int16_t)ds_word((uint16_t)(0x1966 + 2 * v));
	tile_args b = *a;
	if (!v) b.mod &= ~0x03u;   /* (the copy's low byte: and 0xFC) */
	switch (a->layer) {
	case 0: left_part(a); break;
	case 1: if (v == 0xD) break; if (!v) d_floor(&b); else A_FORE_C(0xB, (int16_t)(v + 0x23), a->col, a->row, 0xA, 0); break;
	case 2: if (v == 0xD) break; if (!v) d_floor(&b); else A_FORE_B(0xB, (int16_t)(v + 0x21), a->col, a->row, 0xA, 0); break;
	case 5: if (v == 0xD) break; if (!v) d_floor(&b); else A_BACK_B(0xB, (int16_t)(v + 0x21), a->col, a->row, 0xA, 0); break;
	}
}
/* 3579:0932 / 09A4: tiles 0x08 and 0x09 (layer 1 in the fore table, layer 5 in the back one's first pieces) */
static void d_08_09(tile_args *a, uint8_t t)
{
	adder add = NULL;
	if (a->layer == 0) left_part(a);
	else if (a->layer == 1) add = A_FORE_C;
	else if (a->layer == 5) add = A_BACK_A;
	if (add) add(t, NOID, a->col, a->row, 0xA, 0);
}
static void d_08(tile_args *a) { d_08_09(a, 8); }
static void d_09(tile_args *a) { d_08_09(a, 9); }
/* 3579:0A16: tile 0x06 (modifier bit 11: the piece of tile 6 instead of 5 / 1) */
static void d_06(tile_args *a)
{
	int bit = mod_b1(a) & 8; adder add = NULL; uint8_t t = 0; int16_t id = 0;
	switch (a->layer) {
	case 0: left_part(a); break;
	case 1: t = bit ? 6 : 5; id = 0x21; add = A_FORE_C; break;
	case 2: case 5:
		if (bit) { t = 6; id = 0x1F; } else { t = 1; id = NOID; }
		add = a->layer == 5 ? A_BACK_B : A_FORE_B; break;
	}
	if (add) add(t, id, a->col, a->row, 0xA, 0);
}
/* 3579:0ACE: tile 0x05 (modifier bit 11: the pieces of tile 6) */
static void d_05(tile_args *a)
{
	uint8_t t = (mod_b1(a) & 8) ? 6 : 5; adder add = NULL; int16_t id = 0;
	switch (a->layer) {
	case 0: left_part(a); break;
	case 1: add = A_FORE_C; id = 0x20; break;
	case 2: case 5: add = a->layer == 5 ? A_BACK_B : A_FORE_B; id = 0x1E; break;
	}
	if (add) add(t, id, a->col, a->row, 0xA, 0);
}
/* 3579:0B6E: tile 0x03 (the modifier's low 2 bits: 0 whole; 1 / 2 the fore piece 0x1A, 2 one tile left (and only
 * outside whole-room redraws), with a wall face 0x26 over it unless the tile left is a 3 too) */
static void d_03(tile_args *a)
{
	int si = mod_lo(a) & 3; adder add = NULL;
	switch (a->layer) {
	case 0: left_part(a); break;
	case 1:
		if (!si) { add = A_FORE_C; si = 0x2C; break; }
		if (redraw_all_flag && si != 1) break;
		add = A_FORE_C; piece_word(0x48, 0);
		if (si == 2) piece_word(0x48, (int16_t)(piece_get(0x48) - 0x20));
		si = 0x1A; break;
	case 2: if (!si) { add = A_FORE_B; si = 0x2B; } break;
	case 5: if (!si) { add = A_BACK_B; si = 0x2B; } break;
	}
	if (!add) return;
	add(3, (int16_t)si, a->col, a->row, 0xA, 0);
	if (si != 0x1A) return;
	if (piece_get(0x48) == 0 && tile_at_left(a->row, a->col) != 3) {
		int16_t y = piece_get(0x18D); piece_word(0x18D, (int16_t)(y - 0x36));
		A_FORE_C(0x14, 0x26, a->col, a->row, 0xA, 0);
		piece_word(0x18D, y);
	}
	piece_word(0x48, 0);
}
/* 3579:0CC0: the piece 0x0E of the layers 1, 2 and 5 (also called by the shared 3443 drawers for modifier bit 5) */
void temple_0cc0(tile_args *a);
void temple_0cc0(tile_args *a)
{
	adder add = a->layer == 1 ? A_FORE_C : a->layer == 2 ? A_FORE_B : a->layer == 5 ? A_BACK_B : NULL;
	if (add) add(0xE, NOID, a->col, a->row, 0xA, 0);
}
/* 3579:0C8A: tile 0x0E */
static void d_0e(tile_args *a)
{
	if (a->layer == 0) left_part(a);
	else if ((a->layer == 5 || a->layer == 2) && (mod_lo(a) & 3)) floor_first_piece(a);
	temple_0cc0(a);
}
/* 3579:0D24: empty (0x00): the left part when the tile is on the row drawn, just below it under an open tile, or
 * the row above's with an empty tile drawn */
static void d_00(tile_args *a)
{
	if (a->layer != 0) return;
	int di = 0;
	if (a->row == draw_row) di = 1;
	else if ((int8_t)(a->row - draw_row) == 1) di = open_above(a->col, a->row);
	if (!di && draw_row == -1 && tile_is_empty_kind(cur_tile.tile)) di = 1;
	if (di) left_part(a);
}
/* 3579:0D8A: tile 0x02 (a wall with the modifier's low 3 bits cleared; layer 5 not in column 9, with its own piece) */
static void d_02(tile_args *a)
{
	tile_args b = *a; b.mod &= ~0x07u;
	if (a->layer == 1) d_wall(&b);
	else if (a->layer == 5 && a->col != 9) { d_wall(&b); A_BACK_A(2, NOID, a->col, a->row, 0, 0); }
}
/* 3579:0E1A: tile 0x13: the floor (modifier 0) and in layer 0 its piece unless the drawing area lies inside the box
 * DS:07CA at the tile; in layer 5 the image 0x33 + the modifier's low byte */
static void d_13(tile_args *a)
{
	int v = (uint8_t)a->mod;
	tile_args b = *a; b.mod = 0;
	switch (a->layer) {
	case 0: {
		int16_t r[4] = {0, 0, 0, 0};
		tile_offset_rect(a->row, a->col, 0x07CA, r);
		if (rect_inside(draw_clip, r)) break;
		d_floor(&b); A_BACK_A(0x13, NOID, a->col, a->row, 0, 0);
		break; }
	case 1: case 2: d_floor(&b); break;
	case 5: d_floor(&b); A_BACK_B(0x13, (int16_t)(v + 0x33), a->col, a->row, 0, 0); break;
	}
}
/* 3579:0E02: tile 0x20 */
static void d_20(tile_args *a) { d_13(a); temple_0cc0(a); }
/* 3579:0EE6: tile 0x1A (the modifier's low 5 bits: 0 a floor, else only the left part) */
static void d_1a(tile_args *a)
{
	int dx = mod_lo(a) & 0x1F;
	if (a->layer == 0) { if (dx) left_part(a); else d_floor(a); }
	else if ((a->layer == 1 || a->layer == 2 || a->layer == 5) && !dx) d_floor(a);
}

/* 37F0:0486 (OVL13, level 13's room 4 with description 0x20; tile 0x2B, the flames): layer 0xB with the whole screen
 * as the clip: modifier bit 7 set, the description object DS:1CC2[m & 0x7F] in the foreground (layer 1 for the
 * call); else object 0xB + m */
static void d_2b(tile_args *a)
{
	if (a->layer != 0xB) return;
	int16_t save[4]; memcpy(save, draw_clip, sizeof save); memcpy(draw_clip, screen_rect, sizeof save);
	uint8_t m = (uint8_t)a->mod;
	if (m & 0x80) {
		int i = ds_byte((uint16_t)(0x1CC2 + (m & 0x7F)));
		if (i < desc_count()) { uint8_t *o = desc_obj(i); o[5] = 1; draw_object(i, 1); o[5] = 0xB; }
	} else draw_object((uint8_t)(m + 0xB), 0xB);
	memcpy(draw_clip, save, sizeof save);
}
const kind_drawers kind_temple = {{
	[0x00] = d_00, [0x01] = d_floor, [0x02] = d_02, [0x03] = d_03, [0x04] = d_04, [0x05] = d_05, [0x06] = d_06,
	[0x08] = d_08, [0x09] = d_09, [0x0A] = draw_tile_0a, [0x0B] = d_0b, [0x0C] = draw_3443_0050, [0x0D] = draw_3443_01d6,
	[0x0E] = d_0e, [0x10] = d_10, [0x11] = d_11, [0x13] = d_13, [0x14] = d_wall, [0x19] = d_wall, [0x1A] = d_1a,
	[0x20] = d_20, [0x2B] = d_2b,   /* 0x2B: 37F0:0486 (OVL13) */
}, d_075e};   /* DS:618A: 3579:075E */
