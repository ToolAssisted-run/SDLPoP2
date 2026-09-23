/* Level kind 3 (caverns: levels 3, 4 and 5): the tile types' drawers (overlay OVL04 at 34C1, reached through the
 * far pointers at DS:[0x6188]). Each gets the layer (0, 1, 2 or 5 here), the tile's column and row, its type and
 * modifier, and adds pieces through the 0993 adders. Transcribed from the disassembly. */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
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
static int is_cur(const tile_args *a) { return a->col == draw_col && a->row == draw_row; }
/* 17C1:016E: rect (DS:07C2) moved to tile (col, row) and cut to the screen; 0 when empty (or column/row 0x1E) */
static int tile_offset_rect(int8_t row, int8_t col, int16_t *r)
{
	if (col == 0x1E || row == 0x1E) return 0;
	for (int i = 0; i < 4; i++) r[i] = (int16_t)ds_word(0x07C2 + 2 * i);
	r[0] += 63 * row; r[2] += 63 * row; r[1] += 32 * col; r[3] += 32 * col;
	int16_t t = r[0] > screen_rect[0] ? r[0] : screen_rect[0], l = r[1] > screen_rect[1] ? r[1] : screen_rect[1];
	int16_t b = r[2] < screen_rect[2] ? r[2] : screen_rect[2], rr = r[3] < screen_rect[3] ? r[3] : screen_rect[3];
	if (t >= b || l >= rr) { r[0] = r[1] = r[2] = r[3] = 0; return 0; }
	r[0] = t; r[1] = l; r[2] = b; r[3] = rr; return 1;
}
static int rect_empty(const int16_t *r) { return r[0] >= r[2] || r[1] >= r[3]; }   /* 194C:4D50 */
static int sect(int16_t *d, const int16_t *a, const int16_t *b)
{
	int16_t t = a[0] > b[0] ? a[0] : b[0], l = a[1] > b[1] ? a[1] : b[1], bo = a[2] < b[2] ? a[2] : b[2], r = a[3] < b[3] ? a[3] : b[3];
	if (t >= bo || l >= r) { d[0] = d[1] = d[2] = d[3] = 0; return 0; }
	d[0] = t; d[1] = l; d[2] = bo; d[3] = r; return 1;
}
/* 2699:011C: a lies inside b */
static int rect_inside(const int16_t *a, const int16_t *b) { return a[0] >= b[0] && a[1] >= b[1] && a[2] <= b[2] && a[3] <= b[3]; }
/* 33FD:0844 (OVL04): the collapsing-floor object of the tile at (col, row) of room (0x18 is the right half of 0x17) */
static uint8_t *floor_obj_at(uint8_t room, int8_t col, int8_t row)
{
	uint8_t t = get_tile(row, col, room); int8_t tp = (int8_t)curr_tilepos;
	if (t == 0x18) tp--;
	trob_type *tr = get_trob(tp, room);
	get_room_address_draw(drawn_room);
	return (tr && floor_ptrs[tr->state & 3]) ? floor_objs[tr->state & 3] : NULL;
}

static void d_floor(tile_args *a);
static void d_wall(tile_args *a);
static void d_c10(tile_args *a);
static void d_12a8(tile_args *a);
/* 34C1:0136: the tile's own drawing of a debris piece (modifier bits 8..10) on the tile being drawn */
static void debris(tile_args *a)
{
	if (a->row == 2 || (int8_t)(a->row - draw_row) != -1 || a->col != draw_col) return;
	int di = mod_b1(a) & 7; if (di) A_FORE_C(0x34, (int16_t)(di + 7), a->col, a->row, 0xA, 0);
}
/* 34C1:1216: the torch/object of modifier bits 11..13 on the tile being drawn */
static void obj_1216(tile_args *a)
{
	if (!is_cur(a)) return;
	int k = (mod_b1(a) & 0x38) >> 3; if (!k) return;
	piece_word(0x496, (int16_t)ds_word(0x1628 + 2 * k));
	int id = k + 0x5B;
	const image_t *im = render_image(4, id);   /* (0993:0DC8 loads it; its height) */
	piece_word(0x498, (int16_t)((im ? im->height : 0) - 0x3F));
	A_FORE_C(0x3D, (int16_t)id, a->col, a->row, 0xA, 0);
}
/* 34C1:0002: wall (0x14) */
static void d_wall(tile_args *a)
{
	uint16_t m = mod_lo(a); int di = (m & 0xF) + 0x4A, q = (m & 0xF) / 5;
	if (a->layer == 1) {
		if (!((di == 0x4E || di == 0x53) && (int8_t)(a->col - draw_col) == -1)) A_FORE_C(0x14, (int16_t)di, a->col, a->row, 0xA, 0);
		debris(a);
		if (m & 0x80) A_FORE_C(0x33, (int16_t)(q + 0x58), a->col, a->row, 0, 0);
	} else if (a->layer == 5) {
		if (di != 0x4E && di != 0x53) { if (!tile_is_wall_kind(tile_at_right(a->row, a->col))) A_BACK_A(0x14, (int16_t)(q + 0x55), a->col, a->row, 0xA, 0); }
		else A_BACK_B(0x14, (int16_t)(di == 0x4E ? 0x20 : 0x21), a->col, a->row, 0xA, 0);
		if (m & 0x80) A_BACK_A(0x33, (int16_t)(q + 0x5A), a->col, a->row, 0xA, 0);
	}
}
/* 34C1:018A: tile 0x24 (a floor with a hanging piece) */
static void d_24(tile_args *a)
{
	tile_args b = *a; b.mod = (b.mod & 0xFFFF0000u) | (uint16_t)(b.mod & 0x3F02);   /* (the modifier's low word and 0x3F02) */
	d_floor(&b);
	if (a->layer == 1) { int r = A_FORE_C(0x24, (int16_t)ds_word(0x167A + 2 * (mod_lo(a) & 0xF)), a->col, a->row, 0xA, 0);  (void)r; }
}
/* 34C1:01E2 / 023C: tiles 0x10 and 0x11 (0x11 with its left part too) */
static void d_10_11(tile_args *a, uint8_t t, int left)
{
	if (a->layer == 0) { A_BACK_A(t, NOID, a->col, a->row, 0xA, 0); if (left) d_12a8(a); }
	else if (a->layer == 1) { A_FORE_C(t, NOID, a->col, a->row, 0, 0); debris(a); obj_1216(a); }
}
static void d_10(tile_args *a) { d_10_11(a, 0x10, 0); }
static void d_03e0(tile_args *a);
static void d_11(tile_args *a) { if (a->layer == 0) { A_BACK_A(0x11, NOID, a->col, a->row, 0xA, 0); d_03e0(a); } else if (a->layer == 1) { A_FORE_C(0x11, NOID, a->col, a->row, 0, 0); debris(a); obj_1216(a); } }
/* 34C1:029C / 033E: tiles 0x21 and 0x23 (a piece of image 1 / 2 below, then the tile unless its box is empty) */
static void d_21_23(tile_args *a, uint8_t t, int16_t below)
{
	if (a->layer == 0) {
		if ((int8_t)(a->row - draw_row) != -1) A_BACK_A(0, below, a->col, a->row, 0, 0);
		int16_t save[4]; memcpy(save, draw_clip, sizeof save);
		draw_clip[2] = (int16_t)(63 * a->row + 0x42);
		if (!rect_empty(draw_clip)) A_BACK_A(t, NOID, a->col, a->row, 0xA, 0);
		memcpy(draw_clip, save, sizeof save);
	} else if (a->layer == 1) obj_1216(a);
}
static void d_21(tile_args *a) { d_21_23(a, 0x21, 1); }
static void d_23(tile_args *a) { d_21_23(a, 0x23, 2); }
/* 34C1:03E0: the part left of tile 0x11: image 0x39 twice, its y from the modifier (at most 0x2A), cut to the rows below */
static void d_03e0(tile_args *a)
{
	if (a->layer != 0) return;
	int16_t save[4], r[4]; memcpy(save, draw_clip, sizeof save); memcpy(r, screen_rect, sizeof r);
	r[0] = (int16_t)(63 * a->row + 1);
	if (sect(draw_clip, draw_clip, r)) {
		int v = (uint8_t)a->mod; if (v > 0x2A) v = 0x2A;
		piece_word(0x440, (int16_t)(-(v - (-15))));   /* (sub ax,0xFFF1; neg) */
		A_BACK_A(0x39, NOID, a->col, a->row, 0xA, 0);
		uint16_t f = Kid.frame; adder add = (f >= 0xD9 && f <= 0xE2) ? A_FORE_B : A_BACK_B;
		add(0x39, NOID, a->col, a->row, 0xA, 0);
	}
	memcpy(draw_clip, save, sizeof save);
}
/* 34C1:04C6: floor (0x01) */
static void d_floor(tile_args *a)
{
	int di = mod_lo(a) & 3; adder add = NULL; int16_t id = 0; uint8_t mode = 0;
	switch (a->layer) {
	case 0: id = (int16_t)(di + 0x10); mode = 0; add = A_BACK_A; break;
	case 1: id = (int16_t)(di + 0x19); mode = 0; add = A_FORE_C; break;
	case 2: id = (int16_t)(di + 0x16); mode = 0xA; add = A_FORE_B; break;
	case 5:
		if (tile_is_wall_kind(tile_at_left(a->row, a->col))) A_BACK_A(0x36, (int16_t)(di + 0x13), a->col, a->row, 0xA, 0);
		id = (int16_t)(di + 0x16); mode = 0xA; add = A_BACK_B; break;
	}
	if (add) {
		add(1, id, a->col, a->row, mode, 0);
		if (a->layer == 1) { debris(a); obj_1216(a); }
	}
}
/* 34C1:0686: the chomper's teeth (image 0x37), y from the modifier bits 2..7 (at most 0x32) */
static void d_0686(tile_args *a)
{
	int16_t save[4], r[4]; memcpy(save, draw_clip, sizeof save); memcpy(r, screen_rect, sizeof r);
	r[0] = (int16_t)(63 * a->row); if (a->layer != 1) r[0] -= 0x10;
	if (sect(draw_clip, draw_clip, r)) {
		int v = ((uint8_t)a->mod & 0xFC) >> 2; if (v > 0x32) v = 0x32;
		int16_t si = (int16_t)(-3 - v); adder add = NULL;
		if (a->layer == 1) { piece_word(0x426, si); add = A_FORE_C; }
		else if (a->layer == 2) { piece_word(0x420, si); add = A_FORE_B; }
		else if (a->layer == 5) { piece_word(0x420, si); add = A_BACK_B; }
		if (add) add(0x37, NOID, a->col, a->row, 0xA, 0);
	}
	memcpy(draw_clip, save, sizeof save);
}
/* 34C1:05B4: chomper (0x04) */
static void d_04(tile_args *a)
{
	switch (a->layer) {
	case 0:
		A_BACK_A(4, NOID, a->col, a->row, 0, 0);
		if (tile_is_wall_kind(tile_at_left(a->row, a->col))) A_BACK_A(0x36, 0x23, a->col, a->row, 0xA, 0);
		break;
	case 1: A_FORE_C(4, NOID, a->col, a->row, 0, 0); debris(a); obj_1216(a); break;
	case 2: A_FORE_B(4, NOID, a->col, a->row, 0xA, 0); break;
	case 5: A_BACK_B(4, NOID, a->col, a->row, 0xA, 0); break;
	}
	d_0686(a);
}
/* 34C1:0A04: a collapsing floor's pieces (the object's stage; up to 10 falling bits); right = the right half */
static void floor_pieces(tile_args *a, int right)
{
	if (redraw_all_flag && !right) return;
	adder add = a->layer == 2 ? A_FORE_B : A_BACK_B;
	uint8_t *obj = floor_obj_at(drawn_room, a->col, a->row); if (!obj) return;
	int st = obj[0] & 0x3F;
	if (obj[0] & 0x80) { add((uint8_t)(right + 0x17), (int16_t)(st + 0x92), a->col, a->row, 0xA, 0); if (st < 3) return; }
	if ((obj[0] & 0x40) && st >= 6 && st <= 0xE) {
		piece_word(0x46A, (int16_t)(st + 0x14)); piece_word(0x46C, -8);
		if (st == 6 && st == 0xE) { piece_word(0x468, 0x98); if (st == 6) piece_word(0x46A, (int16_t)(piece_get(0x46A) + 3)); }   /* (never both: as the original) */
		else piece_word(0x468, 0x97);
		if (right) piece_word(0x46A, (int16_t)(piece_get(0x46A) - 0x20));
		add(0x3B, NOID, a->col, a->row, 0xA, 0);
	}
	for (int pass = 0; pass < 2; pass++) {
		uint8_t *s = obj + 1;
		for (int k = 0; k < 10; k++, s += 10) {
			if (!(s[0] | s[1] << 8)) continue;
			if ((int16_t)(s[8] | s[9] << 8) == pass) continue;
			piece_word(0x46A, (int16_t)(-((right << 5) - (int16_t)(s[6] | s[7] << 8))));
			piece_word(0x46C, (int16_t)(s[4] | s[5] << 8));
			const uint8_t *list = ds_ptr(ds_word(0x15F0 + 2 * (int8_t)s[3]));
			piece_word(0x468, (int16_t)((int8_t)list[(int8_t)s[2]] + 0x7F));
			add(0x3B, NOID, a->col, a->row, 0xA, 0);
		}
		if (pass == 0) { add((uint8_t)(right + 0x17), 0x90, a->col, a->row, 0xA, 0); continue; }
		if ((obj[0] & 0x40) && st >= 0xD && st <= 0x17) {
			memcpy(piece_table + 0x468, ds_ptr(0x15EA + 6 * st), 6);
			if (right) piece_word(0x46A, (int16_t)(piece_get(0x46A) - 0x20));
			add(0x3B, NOID, a->col, a->row, 0xA, 0);
		}
	}
}
/* 34C1:080E / 090C: collapsing floor, left (0x17) and right (0x18) halves */
static void d_17_18(tile_args *a, int right)
{
	tile_args b = *a; b.mod = (b.mod & ~0xFFu) | (uint32_t)right;   /* (the modifier's low byte: 0, or 1 for the right half) */
	uint8_t *obj = floor_obj_at(drawn_room, a->col, a->row); adder add = NULL;
	switch (a->layer) {
	case 0: case 1: d_floor(&b); break;
	case 2: d_floor(&b); if (obj && (!(obj[0] & 0x80) || (obj[0] & 0x3F) >= 3)) add = A_FORE_B; break;
	case 5:
		d_floor(&b);
		if (!right && redraw_all_flag) break;
		if (obj && (!(obj[0] & 0x80) || (obj[0] & 0x3F) >= 3)) add = A_BACK_B;
		break;
	case 0xB: floor_pieces(a, right); break;
	}
	if (add) { add((uint8_t)(right ? 0x18 : 0x17), 0x91, a->col, a->row, 0, 0); if (a->layer == 2) floor_pieces(a, 0); }
}
static void d_17(tile_args *a) { d_17_18(a, 0); }
static void d_18(tile_args *a) { d_17_18(a, 1); }
/* 34C1:0C10: tile 0x0B (a ledge; 0x0D in the low nibble: none) */
static void d_c10(tile_args *a)
{
	int flip = (mod_lo(a) & 0x80) >> 7, di = mod_lo(a) & 0xF;
	if (di != 0xD) di = (a->col % 2) ? (int16_t)ds_word(0x1688 + 2 * di) : (int16_t)ds_word(0x16A0 + 2 * di);
	switch (a->layer) {
	case 0: A_BACK_A(0xB, (int16_t)(flip + 0x40), a->col, a->row, 0xA, 0); break;
	case 1: if (di != 0xD) { piece_word(0xE2, (int16_t)(di + 5)); A_FORE_C(0xB, (int16_t)(flip + 0x46), a->col, a->row, 0xA, 0); } break;
	case 2: case 5: {
		adder add = a->layer == 2 ? A_FORE_B : A_BACK_B;
		piece_word(0xDA, 0x10); piece_word(0xDC, -7);
		add(0xB, (int16_t)(flip + 0x44), a->col, a->row, 0xA, 0);
		if (di != 0xD) { piece_word(0xDA, 1); piece_word(0xDC, (int16_t)(di + 1)); add(0xB, (int16_t)(flip + 0x42), a->col, a->row, 0xA, 0); }
		break; }
	}
}
/* 34C1:0DA0: tile 0x08 (floor and a pillar part) */
static void d_08(tile_args *a) { d_floor(a); if (a->layer == 1) A_FORE_C(8, NOID, a->col, a->row, 0xA, 0); }
/* 34C1:0DD2: tile 0x09 */
static void d_00(tile_args *a);
static void d_09(tile_args *a)
{
	if (a->layer == 0) { a->mod &= ~0x07u; d_00(a); }
	else if (a->layer == 1) { A_FORE_C(9, NOID, a->col, a->row, 0xA, 0); obj_1216(a); }
}
/* 34C1:0E18: tile 0x03 */
static void d_03(tile_args *a)
{
	switch (a->layer) {
	case 0: A_BACK_A(3, NOID, a->col, a->row, 0xA, 0); break;
	case 1: A_FORE_C(3, NOID, a->col, a->row, 0xA, 0); debris(a); obj_1216(a); break;
	case 2: A_FORE_B(3, NOID, a->col, a->row, 0xA, 0); break;
	case 5: A_BACK_B(3, NOID, a->col, a->row, 0xA, 0); break;
	}
}
/* 34C1:0EB4: tile 0x22 (modifier bit 11: the extra piece 0x38) */
static void d_22(tile_args *a)
{
	adder extra = NULL;
	switch (a->layer) {
	case 0: A_BACK_A(0x22, NOID, a->col, a->row, 0, 0); if (tile_is_wall_kind(tile_at_left(a->row, a->col))) extra = A_BACK_A; break;
	case 1: A_FORE_C(0x22, NOID, a->col, a->row, 0, 0); debris(a); break;
	case 2: A_FORE_B(0x22, NOID, a->col, a->row, 0xA, 0); if (mod_b1(a) & 8) extra = A_FORE_B; break;
	case 5: A_BACK_B(0x22, NOID, a->col, a->row, 0xA, 0); if (mod_b1(a) & 8) extra = A_BACK_B; break;
	}
	if (extra) extra(0x38, NOID, a->col, a->row, 0xA, 0);
}
/* 34C1:0FE2: the piece 0x0E of the layers 1, 2 and 5 */
static void piece_0e(tile_args *a)
{
	adder add = a->layer == 1 ? A_FORE_C : a->layer == 2 ? A_FORE_B : a->layer == 5 ? A_BACK_B : NULL;
	if (add) add(0xE, NOID, a->col, a->row, 0xA, 0);
}
/* 34C1:0FBC: tile 0x0E */
static void d_0e(tile_args *a) { if (mod_b1(a) & 8) d_22(a); else d_floor(a); piece_0e(a); }
/* 34C1:1046: tile 0x20 */
static void d_20(tile_args *a) { d_12a8(a); piece_0e(a); }
/* 34C1:105E: empty (0x00): the floor's front edge under a tile above (the modifier's low 3 bits: 0..2 an edge
 * image, 3 and more a ledge 0x0B drawn instead) */
static void d_00(tile_args *a)
{
	uint16_t v = mod_lo(a); int di = 0, dx = 0;
	if (a->layer == 0 || a->layer == 5) {
		v &= 7;
		if (a->row == draw_row) dx = 1;
		else if ((int8_t)(a->row - draw_row) == 1 && (tile_is_empty_kind(tile_above(a->row, a->col)) || tile_is_empty_kind(tile_above_right(a->row, a->col)))) dx = 1;
		else dx = draw_row == -1 && tile_is_empty_kind(cur_tile.tile);
	}
	int edge = 0;
	if (a->layer == 0) { if (dx) { if (v >= 3) di = 1; else { A_BACK_A(0, (int16_t)(v + 1), a->col, a->row, 0, 0); edge = 1; } } }
	else if (a->layer == 1) obj_1216(a);
	else if (a->layer == 5) { if (dx) { if (v >= 3) di = 1; else edge = 1; } }
	if (edge && !(tile_is_empty_kind(left_tile.tile) && (uint16_t)left_tile.mod < 3) && a->col == draw_col)
		A_BACK_A(0x35, (int16_t)(v + 4), a->col, a->row, 0xA, 0);
	if (di) {
		tile_args b = *a;
		b.mod = (b.mod & 0xFFFF0000u) | (uint16_t)((v == 4 ? 0x80 : 0) | 0xD);
		d_c10(&b);
	}
}
/* 34C1:11A4: tile 0x02 (a wall face in the modifier's low nibble 0, or 0x80) */
static void d_02(tile_args *a)
{
	tile_args b = *a; b.mod = (b.mod & ~0x0Fu) | 0x80;   /* (low byte: and 0xF0, or 0x80) */
	if (a->layer == 0 || a->layer == 1) d_wall(&b);
	else if (a->layer == 5 && a->col != 9) { d_wall(&b); A_BACK_A(2, NOID, a->col, a->row, 0xA, 0); }
}
/* 34C1:12A8: tile 0x13 (and the left of 0x10/0x20): the floor unless the box covers the drawing area, then 0x3A */
static void d_12a8(tile_args *a)
{
	tile_args b = *a; b.mod &= ~0x03u;
	int16_t r[4]; int inside = 0;
	if (tile_offset_rect(a->row, a->col, r)) inside = rect_inside(draw_clip, r);
	switch (a->layer) {
	case 0:
		if (!inside) { d_floor(&b); A_BACK_A(0x13, NOID, a->col, a->row, 0xA, 0); }
		A_BACK_A(0x3A, (int16_t)((uint8_t)a->mod + 0x35), a->col, a->row, 0xA, 0);
		break;
	case 1: case 2: d_floor(&b); break;
	case 5: if (!inside) d_floor(&b); break;
	}
}
static void d_13(tile_args *a) { d_12a8(a); }

static const kind_drawers caverns = {{
	[0x00] = d_00, [0x01] = d_floor, [0x02] = d_02, [0x03] = d_03, [0x04] = d_04, [0x08] = d_08, [0x09] = d_09, [0x0A] = draw_tile_0a,
	[0x0B] = d_c10, [0x0E] = d_0e, [0x10] = d_10, [0x11] = d_11, [0x13] = d_13, [0x14] = d_wall, [0x17] = d_17,
	[0x18] = d_18, [0x20] = d_20, [0x21] = d_21, [0x22] = d_22, [0x23] = d_23, [0x24] = d_24,
	/* 0x0A: 0FB3:2394 (resident), 0x12 / 0x1B / 0x2C: the 37F0 overlays (level 5's rooms) */
}, NULL};
const kind_drawers *kind_drawers_for(int kind) { return kind == 3 ? &caverns : NULL; }
