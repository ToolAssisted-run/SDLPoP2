/* Tile drawers several level kinds share: the 3443 overlay (tiles 0x0C and 0x0D of ruins and temple). */
#include <string.h>
#include "types.h"
#include "globals.h"
#include "render.h"
#include "render_tiles.h"

#define NOID ((int16_t)-2)
static void piece_word(int off, int16_t v) { piece_table[off] = (uint8_t)v; piece_table[off + 1] = (uint8_t)(v >> 8); }
typedef int (*adder)(uint8_t, int16_t, int8_t, int8_t, uint8_t, uint8_t);

/* the kind's own extra part of these tiles, for modifier bit 5 (34A3:0EC0 of ruins, 3579:0CC0 of temple: in
 * render_kind4.c / render_kind2.c) */

/* 3443:000C */
static void extra_part(tile_args *a)
{
	if (!(a->mod & 0x20)) return;
	if (level_kind == 4) { tile_args b = *a; b.mod &= 0xFFFF0000u; ruins_0ec0(&b); }
	else temple_0cc0(a);
}
/* the piece of a frame (DS:1784[modifier bits 0..4]) from the table at `tab` into the piece table at `off`
 * (9 images further for modifier bit 6) */
static void frame_piece(const tile_args *a, int tab, int off, int16_t f)
{
	for (int i = 0; i < 3; i++) piece_word(off + 2 * i, (int16_t)ds_word(tab + f * 6 + 2 * i));
	if (a->mod & 0x40) piece_word(off, (int16_t)(piece_table[off] + (piece_table[off + 1] << 8) + 9));
}
/* the floor under both (the kind's tile 1 drawer), with the modifier's low word the second byte << 8 (+1 in ruins
 * for 0x0D) */
static void floor_under(const tile_args *a, int plus)
{
	tile_args b = *a; b.mod = (a->mod & 0xFFFF0000u) | (uint16_t)(((a->mod >> 8) & 0xFF) << 8);
	if (plus && level_kind == 4) b.mod++;
	if (tile_drawers && tile_drawers->by_tile[1]) tile_drawers->by_tile[1](&b);
}
/* 3443:0050: tile 0x0C (a gate of frames 0x61..0x63) */
void draw_3443_0050(tile_args *a)
{
	adder add = NULL; int16_t f;
	switch (a->layer) {
	case 0: floor_under(a, 0); piece_word(0xE5, (a->mod & 0x80) ? 0x60 : 0x5F); add = add_back_piece_a; break;
	case 1: floor_under(a, 0); f = (int16_t)ds_word(0x1784 + 2 * (a->mod & 0x1F));
		if (f >= 0x61 && f <= 0x63) { frame_piece(a, 0x1584, 0xEB, f); add = add_fore_piece_b; } break;
	case 2: floor_under(a, 0); break;
	case 5: floor_under(a, 0); f = (int16_t)ds_word(0x1784 + 2 * (a->mod & 0x1F));
		if (f >= 0x61 && f <= 0x63) { frame_piece(a, 0x1566, 0xEB, f); add = add_back_piece_b; } break;
	}
	if (add) add(0x0C, NOID, a->col, a->row, 0xA, 0);
	extra_part(a);
}
/* 3443:01D6: tile 0x0D (the same for frames 0x64 / 0x65) */
void draw_3443_01d6(tile_args *a)
{
	adder add = NULL; int16_t f;
	switch (a->layer) {
	case 0: floor_under(a, 1);
		if (!redraw_all_flag && (int8_t)(a->col - draw_col) == -1) { piece_word(0xF8, (a->mod & 0x80) ? 0x60 : 0x5F); add = add_back_piece_a; }
		break;
	case 1: floor_under(a, 1); f = (int16_t)ds_word(0x1784 + 2 * (a->mod & 0x1F));
		if (f == 0x64) { frame_piece(a, 0x1584, 0xFE, f); add = add_fore_piece_b; } break;
	case 2: floor_under(a, 1); break;
	case 5: floor_under(a, 1); f = (int16_t)ds_word(0x1784 + 2 * (a->mod & 0x1F));
		if (f == 0x64 || f == 0x65) { frame_piece(a, 0x1566, 0xFE, f); add = add_back_piece_b; } break;
	}
	if (add) add(0x0D, NOID, a->col, a->row, 0xA, 0);
	extra_part(a);
}
