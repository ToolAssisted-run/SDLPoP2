/* Level kind 4 (ruins: levels 6..9): the tile types' drawers (overlay 34A3, reached through the far pointers at
 * DS:[0x6188]; 0x0C / 0x0D are the shared 3443 ones, 0x0A and 0x16 resident in 0FB3). Each gets the layer (0, 1, 2
 * or 5), the tile's column and row, its type and modifier, and adds pieces through the 0993 adders. Transcribed from
 * the disassembly; see render_kind3.c for the caverns ones. */
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
static uint8_t mod_b0(const tile_args *a) { return (uint8_t)a->mod; }
static uint8_t mod_b1(const tile_args *a) { return (uint8_t)(a->mod >> 8); }
static void piece_word(int off, int16_t v) { piece_table[off] = (uint8_t)v; piece_table[off + 1] = (uint8_t)(v >> 8); }
static void piece_copy(int off, uint16_t ds_off) { memcpy(piece_table + off, ds_ptr(ds_off), 6); }   /* (movsw x3 from DS) */
static int is_cur(const tile_args *a) { return a->col == draw_col && a->row == draw_row; }
/* 194C:5266: d = a cut to b; 0 (and d cleared) when empty */
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
/* 0993:078E (register arguments ax, dl, bl): the first back-table entry of image id at tile (col, row) */
static draw_entry *find_back(int16_t id, int8_t col, int8_t row)
{
	for (int i = 0; i < table_counts[0]; i++)
		if ((int16_t)back_table[i].id == id && back_table[i].col == (uint8_t)col && back_table[i].row == (uint8_t)row) return &back_table[i];
	return NULL;
}
/* an entry's box (+0x0A) overlaps the drawing area: then the drawing area becomes that box (194C:5266, 194C:647E) */
static int take_entry_box(const draw_entry *e)
{
	int16_t box[4], tmp[4]; memcpy(box, (const uint8_t *)e + 0xA, sizeof box);
	if (!sect(tmp, box, draw_clip)) return 0;
	union_rect(draw_clip, box, box);
	return 1;
}

static void d_floor(tile_args *a);
static void d_05(tile_args *a);
static void d_06(tile_args *a);
/* 34A3:0002: the layer-0 back piece of the modifier's byte 2 (image, bits 0..4) and byte 3 (tile type 0x24 + bits
 * 0..3; images 1..5 over image 7: for image 5 part 2, 3, 6 or 7 as part - 2, and in a whole-room redraw once more
 * 2 columns right): not drawn when the tile left has it already (then the drawing area becomes that entry's box) */
static void back_0002(tile_args *a)
{
	uint8_t b3 = (uint8_t)(a->mod >> 24) & 0xF; int16_t di = (int16_t)((a->mod >> 16) & 0x1F);
	if (!di) return;
	draw_entry *e = NULL; int none = 0;
	if (!redraw_all_flag) {
		e = find_back(di, (int8_t)(a->col - 1), a->row);
		if (e && take_entry_box(e)) {
			if (di >= 1 && di <= 5) { e = find_back(7, (int8_t)(a->col - 1), a->row); if (e) take_entry_box(e); }
		} else { e = NULL; none = 1; }
	}
	if (e) return;
	if (!none && b3 && !(a->col == -1 && b3 <= 3) && a->row != 3) return;
	uint8_t mode = 0;
	if (di >= 1 && di <= 5) {
		uint8_t v = (di == 5 && (b3 == 2 || b3 == 3 || b3 == 6 || b3 == 7)) ? (uint8_t)(b3 - 2) : b3;
		A_BACK_A((uint8_t)(v + 0x24), 7, a->col, a->row, 0, 0);
		if (di == 5 && redraw_all_flag) A_BACK_A((uint8_t)(v + 0x24), 7, (int8_t)(a->col + 2), a->row, 0, 0);
		mode = 0xA;
	}
	A_BACK_A((uint8_t)(b3 + 0x24), di, a->col, a->row, mode, 0);
}
/* 34A3:06E4: the modifier's byte 1 parts (tile type 0x12's pieces): layer 5, low nibble 0x0B / 0x0C a back piece
 * (DS:17D8), else bits 4..5 (DS:185C); layer 1, low nibble 1..0x0A a front piece (DS:1820; 4 unclipped) */
static void part_06e4(tile_args *a)
{
	if (a->layer == 5) {
		int v = mod_b1(a) & 0xF;
		if (v == 0xB || v == 0xC) {
			piece_copy(0x157, (uint16_t)(0x17D8 + 6 * v));
			piece_table[0x156] = v == 0xC;
			A_BACK_A(0x12, NOID, a->col, a->row, 0xA, 0);
		} else if ((v = (mod_b1(a) & 0x30) >> 4)) {
			piece_copy(0x15D, (uint16_t)(0x185C + 6 * v));
			A_BACK_B(0x12, NOID, a->col, a->row, 0xA, 0);
		}
	} else if (a->layer == 1) {
		int v = mod_b1(a) & 0xF;
		if (!v || v >= 0xB) return;
		if (v == 2 && redraw_all_flag) return;
		piece_copy(0x15D, (uint16_t)(0x1820 + 6 * v));
		int added = 0;
		if (!(v == 3 && !redraw_all_flag && is_cur(a))) added = A_FORE_B(0x12, NOID, a->col, a->row, 0xA, 0);
		if (added && v == 4) memcpy((uint8_t *)&fore_table[table_counts[1] - 1] + 0xA, screen_rect, 8);   /* ([26D4]:0A1E + 0x14 * DS:60F2) */
	}
}
/* 34A3:01CC: wall (0x14); layer 5 adds the front piece too */
static void d_wall(tile_args *a)
{
	if (a->layer == 0) { if (redraw_all_flag) back_0002(a); return; }
	if (a->layer != 1 && a->layer != 5) return;
	if (a->layer == 5) {
		int di = (mod_lo(a) & 0xC) >> 2;
		if (di != 2 && tile_at_right(a->row, a->col) != 0x14) A_BACK_A(0x14, (int16_t)(di == 3 ? 0x35 : di + 0xC), a->col, a->row, 0xA, 0);
	}
	A_FORE_C(0x14, (int16_t)((mod_b0(a) & 3) + 0xE), a->col, a->row, 0, 0);
	part_06e4(a);
}
/* 34A3:0266: tile 0x15 */
static void d_15(tile_args *a)
{
	adder add = NULL;
	switch (a->layer) {
	case 0: back_0002(a); break;
	case 1: add = A_FORE_C; break;
	case 2: add = A_FORE_B; break;
	case 5: add = A_BACK_B; break;
	}
	if (add) { add(0x15, NOID, a->col, a->row, 0xA, 0); part_06e4(a); }
}
/* 34A3:02E6: tile 0x07 (bit 7: its own image by the low 2 bits; else a wall with the face of DS:17E2 / bits 2..3) */
static void d_07(tile_args *a)
{
	uint16_t m = mod_lo(a); int lo = m & 3, dx = m & 0x80; tile_args b = *a;
	switch (a->layer) {
	case 0: if (redraw_all_flag) back_0002(a); break;
	case 1:
		if (dx) A_FORE_C(7, (int16_t)(lo + 0x3A), a->col, a->row, 0xA, 0);
		else { b.mod = (b.mod & ~0xFFFFu) | (uint16_t)((m & 0xFFFC) | ds_word((uint16_t)(0x17E2 + 2 * lo))); d_wall(&b); }
		part_06e4(a);
		break;
	case 2: if (dx) A_FORE_B(7, (int16_t)(lo + 0x36), a->col, a->row, 0xA, 0); break;
	case 5:
		if (dx) A_BACK_B(7, (int16_t)(lo + 0x36), a->col, a->row, 0xA, 0);
		else { b.mod |= lo == 0 ? 0xCu : 0x8u; d_wall(&b); }
		break;
	}
}
/* 34A3:03F4: tile 0x10 (layer 0 only in a whole-room redraw) */
static void d_10(tile_args *a)
{
	adder add = NULL; uint8_t mode = 0xA; int part = 1;
	switch (a->layer) {
	case 0: if (redraw_all_flag) { back_0002(a); add = A_BACK_A; } break;
	case 1: add = A_FORE_C; mode = 0; break;
	case 2: add = A_FORE_B; part = 0; break;
	case 5: add = A_BACK_B; break;
	default: part = 0; break;
	}
	if (add) add(0x10, NOID, a->col, a->row, mode, 0);
	if (part) part_06e4(a);
}
/* 34A3:05A0: tile 0x11's layer 0 below the row's line: the tile again once its object's state is 4 or more, then
 * image 0x39 twice (y from the modifier, at most 0x2A; the second in front while the prince climbs) */
static void d_05a0(tile_args *a)
{
	if (a->layer != 0) return;
	int16_t save[4], r[4]; memcpy(save, draw_clip, sizeof save); memcpy(r, screen_rect, sizeof r);
	r[0] = (int16_t)(63 * a->row - 5);
	if (sect(draw_clip, draw_clip, r)) {
		trob_type *t = get_trob((int8_t)(a->col + row_tilepos(a->row)), drawn_room);   /* 1375:2598 */
		if (t && (int8_t)t->state >= 4) A_BACK_A(0x11, NOID, a->col, a->row, 0xA, 0);
		int v = mod_b0(a); if (v > 0x2A) v = 0x2A;
		piece_word(0x440, (int16_t)(-(v + 0x13)));
		A_BACK_A(0x39, NOID, a->col, a->row, 0xA, 0);
		uint16_t f = Kid.frame; adder add = (f >= 0xD9 && f <= 0xE2) ? A_FORE_B : A_BACK_B;
		piece_copy(0x442, 0x187A);
		add(0x39, NOID, a->col, a->row, 0xA, 0);
	}
	memcpy(draw_clip, save, sizeof save);
}
/* 34A3:04A6: tile 0x11 */
static void d_11(tile_args *a)
{
	adder add = NULL; uint8_t mode = 0xA; int part = 0;
	switch (a->layer) {
	case 0:
		if (redraw_all_flag) back_0002(a);
		else { piece_copy(0x442, 0x1874); A_BACK_B(0x39, NOID, a->col, a->row, 0xA, 0); }
		break;
	case 1: add = A_FORE_C; mode = 0; part = 1; break;
	case 2: add = A_FORE_B; break;
	case 5: if (!redraw_all_flag) add = A_BACK_B; part = 1; break;
	}
	if (add) add(0x11, NOID, a->col, a->row, mode, 0);
	if (part) part_06e4(a);
	if (a->layer == 0) d_05a0(a);
}
/* 34A3:086E: floor (0x01, also 0x0F) */
static void d_floor(tile_args *a)
{
	int di = mod_b0(a) & 3;
	switch (a->layer) {
	case 0: back_0002(a); break;
	case 1: A_FORE_C(1, (int16_t)(di + 0x1A), a->col, a->row, 0, 0); part_06e4(a); break;
	case 2: A_FORE_B(1, (int16_t)(di + 0x16), a->col, a->row, 0xA, 0); break;
	case 5: A_BACK_B(1, (int16_t)(di + 0x16), a->col, a->row, 0xA, 0); part_06e4(a); break;
	}
}
/* 34A3:0986: the chomper's teeth (image 0x37), y from the modifier bits 2..7 (at most 0x32); also the kind's special
 * drawer (DS:618A); level 8's room 9 has its own (the 37F0 overlay through 2A31:0E43) */
static void d_0986(tile_args *a)
{
	int16_t save[4], r[4]; memcpy(save, draw_clip, sizeof save); memcpy(r, screen_rect, sizeof r);
	r[0] = (int16_t)(63 * a->row - 10);
	if (sect(draw_clip, draw_clip, r)) {
		int v = (mod_b0(a) & 0xFC) >> 2; if (v > 0x32) v = 0x32;
		piece_word(0x420, (int16_t)(-(v + 4)));
		adder add = a->layer == 2 ? A_FORE_B : a->layer == 5 ? A_BACK_B : NULL;
		if (add) {
			if (drawn_room == 9 && level_number == 8) { if (a->layer == 2) note_missing("DRAW_2A31_0E43"); }
			else add(0x37, NOID, a->col, a->row, 0xA, 0);
		}
	}
	memcpy(draw_clip, save, sizeof save);
}
/* 34A3:0900: chomper (0x04) */
static void d_04(tile_args *a)
{
	adder add = NULL;
	switch (a->layer) {
	case 0: back_0002(a); break;
	case 1: add = A_FORE_C; break;
	case 2: add = A_FORE_B; break;
	case 5: add = A_BACK_B; break;
	}
	if (add) { add(4, NOID, a->col, a->row, 0xA, 0); d_0986(a); part_06e4(a); }
}
/* 34A3:0A72: tile 0x0B (a ledge by the low nibble through DS:17EA / 1802 for odd / even columns; 0x0D none, 0 the
 * floor) */
static void d_0b(tile_args *a)
{
	int v = mod_b0(a) & 0xF; tile_args b = *a;
	if (v != 0xD) v = (a->col % 2) ? ds_word((uint16_t)(0x17EA + 2 * v)) : ds_word((uint16_t)(0x1802 + 2 * v));
	if (!v) b.mod &= ~0x03u;
	switch (a->layer) {
	case 0: if (v == 0xD || v != 0) back_0002(a); else d_floor(&b); break;
	case 1:
		if (v == 0xD) part_06e4(a);
		else if (!v) d_floor(&b);
		else { A_FORE_C(0xB, (int16_t)(v + 0x2B), a->col, a->row, 0xA, 0); part_06e4(a); }
		break;
	case 2: if (v == 0xD) break; if (!v) d_floor(&b); else A_FORE_B(0xB, (int16_t)(v + 0x29), a->col, a->row, 0xA, 0); break;
	case 5:
		if (v == 0xD) part_06e4(a);
		else if (!v) d_floor(&b);
		else { A_BACK_B(0xB, (int16_t)(v + 0x29), a->col, a->row, 0xA, 0); part_06e4(a); }
		break;
	}
}
/* 34A3:0B8C / 0BF4: tiles 0x08 and 0x09 */
static void d_08_09(tile_args *a, uint8_t t)
{
	adder add = NULL;
	if (a->layer == 0) { back_0002(a); add = A_BACK_A; } else if (a->layer == 1) add = A_FORE_C;
	if (add) { add(t, NOID, a->col, a->row, 0xA, 0); part_06e4(a); }
}
static void d_08(tile_args *a) { d_08_09(a, 8); }
static void d_09(tile_args *a) { d_08_09(a, 9); }
/* 34A3:0C5A / 0D06: tiles 0x06 and 0x05 (drawn as tile type 5, or 6 for modifier bit 11; images 0x34/0x33 and
 * 0x32/0x31) */
static void d_05_06(tile_args *a, int16_t fore_id)
{
	uint8_t t = (uint8_t)(5 + ((mod_b1(a) & 8) ? 1 : 0)); adder add = NULL; int16_t id = 0; uint8_t mode = 0;
	switch (a->layer) {
	case 0: back_0002(a); break;
	case 1: add = A_FORE_C; id = fore_id; mode = 0; break;
	case 2: case 5: add = a->layer == 5 ? A_BACK_B : A_FORE_B; id = (int16_t)(fore_id - 1); mode = 0xA; break;
	}
	if (add) add(t, id, a->col, a->row, mode, 0);
}
static void d_06(tile_args *a) { d_05_06(a, 0x34); }
static void d_05(tile_args *a) { d_05_06(a, 0x32); }
/* 34A3:0DB2: tile 0x03 */
static void d_03(tile_args *a)
{
	int si = mod_lo(a) & 3; adder add = NULL;
	switch (a->layer) {
	case 0: back_0002(a); break;
	case 1: add = A_FORE_C; si += 0x27; break;
	case 2: add = A_FORE_B; si += 0x24; break;
	case 5: add = A_BACK_B; si += 0x24; break;
	}
	if (add) { add(3, (int16_t)si, a->col, a->row, 0xA, 0); part_06e4(a); }
}
/* 34A3:0EC0: tile 0x0E's own piece (bits 4..5); also the 3443 drawers' extra part (render_kind_common.c) */
void ruins_0ec0(tile_args *a);
void ruins_0ec0(tile_args *a)
{
	int si = (mod_b0(a) & 0x30) >> 4; adder add = NULL;
	switch (a->layer) {
	case 1: si += 0x21; add = A_FORE_C; break;
	case 2: si += 0x1E; add = A_FORE_B; break;
	case 5: si += 0x1E; add = A_BACK_B; break;
	}
	if (add) { add(0xE, (int16_t)si, a->col, a->row, 0xA, 0); part_06e4(a); }
}
/* 34A3:0E44: tile 0x0E (bit 6: over tile 5 (bit 2), 6 (bit 3; both clear the caller's bit 11) or the floor) */
static void d_0e(tile_args *a)
{
	if (mod_b0(a) & 0x40) {
		tile_args b = *a; b.mod &= ~0x40u;
		if (b.mod & 4) { d_05(&b); a->mod &= ~0x800u; }
		else if (b.mod & 8) { d_06(&b); a->mod &= ~0x800u; }
		else d_floor(&b);
	} else if (a->layer == 0) back_0002(a);
	ruins_0ec0(a);
	part_06e4(a);
}
/* 0FB3:2788 (resident): tile 0x16, the kind's floor (low 2 bits cleared; not while the prince is in level 8's room
 * 9), then in front image 0x0B when the low 2 bits are the sword type (DS:5CBA; 1 when that is 0xFF), else 0x2E */
static void d_16(tile_args *a)
{
	if (tile_drawers && tile_drawers->by_tile[1] && !(Kid.room == 9 && level_number == 8)) {
		tile_args b = *a; b.mod &= ~0x03u; tile_drawers->by_tile[1](&b);
	}
	if (a->layer != 1) return;
	uint8_t v = mod_b0(a) & 3;
	int16_t id = (v == byte_5cba || (byte_5cba == 0xFF && v == 1)) ? 0xB : 0x2E;
	A_FORE_C(0x16, id, a->col, a->row, 0xA, 0);
}
/* 0FB3:2CBC: does the tile above (col, row), or above and right, leave the floor's edge open (empty, 5 or 6; 3 in
 * level 13) */
static int above_open(int8_t row, int8_t col)
{
	uint8_t t = tile_above(row, col);
	if (tile_is_empty_kind(t) || t == 5 || t == 6 || (t == 3 && level_number == 13)) return 1;
	uint8_t t2 = tile_above_right(row, col);
	if (tile_is_empty_kind(t2) || t2 == 5 || t2 == 6) return 1;
	return t == 3 && level_number == 13;   /* (the first tile again, as the original) */
}
/* 34A3:0F42: empty (0x00) */
static void d_00(tile_args *a)
{
	if (a->layer == 0) {
		int di = 0;
		if (a->row == draw_row) di = 1;
		else if ((int8_t)(a->row - draw_row) == 1) di = above_open(a->row, a->col);
		if (!di && draw_row == -1 && tile_is_empty_kind(cur_tile.tile)) di = 1;   /* 0FB3:28D4 */
		if (di) back_0002(a);
	}
	part_06e4(a);
}

const kind_drawers kind_ruins = {{
	[0x00] = d_00, [0x01] = d_floor, [0x03] = d_03, [0x04] = d_04, [0x05] = d_05, [0x06] = d_06, [0x07] = d_07,
	[0x08] = d_08, [0x09] = d_09, [0x0A] = draw_tile_0a, [0x0B] = d_0b, [0x0C] = draw_3443_0050, [0x0D] = draw_3443_01d6,
	[0x0E] = d_0e, [0x0F] = d_floor, [0x10] = d_10, [0x11] = d_11, [0x14] = d_wall, [0x15] = d_15, [0x16] = d_16,
	/* 0x02, 0x12, 0x13: none (the table ends at 0x16) */
}, d_0986};   /* DS:618A: 34A3:0986 */
