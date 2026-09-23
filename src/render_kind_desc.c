/* The tile drawers of the level kinds drawn mostly by their room descriptions (desert = 1, rooftops = 5, the final
 * level = 6): they only place description objects (render_desc.c), animated through the tile modifiers. Transcribed
 * from the 33FD overlay of each kind. */
#include <string.h>
#include "types.h"
#include "globals.h"
#include "render.h"
#include "render_tiles.h"

static int16_t rd(const uint8_t *p) { return (int16_t)(p[0] | p[1] << 8); }
static void wr(uint8_t *p, int16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static uint16_t mod_lo(const tile_args *a) { return (uint16_t)a->mod; }
/* object i of layer 0xB with the whole screen as the clip */
static void draw_unclipped(int i)
{
	int16_t save[4]; memcpy(save, draw_clip, sizeof save); memcpy(draw_clip, screen_rect, sizeof save);
	draw_object(i, 0xB);
	memcpy(draw_clip, save, sizeof save);
}

/* ---- desert (33FD in level 2) ---- */
/* 33FD:05C2: tile 4, two objects slid by the modifier (object 2 at x (4 - m) * 2, object 15 at 0x1B - 2m) */
static void desert_04(tile_args *a)
{
	if (a->layer != 0xB) return;
	int m = a->mod & 0x1F;
	wr(desc_obj(2) + 3, (int16_t)((4 - m) * 2));
	wr(desc_obj(15) + 3, (int16_t)(0x1B - 2 * m)); wr(desc_obj(15) + 0xD, 0);   /* (and its rect's left 0) */
	draw_object(2, 0xB); draw_object(0xF, 0xB);
}
/* 33FD:071A / 068E: tiles 0x1C / 0x1D, object ((modifier >> 1) & 7) + 1 */
static void desert_1c(tile_args *a) { if (a->layer == 0xB) draw_unclipped(((mod_lo(a) >> 1) & 7) + 1); }
/* 33FD:0872: tile 0x1E, object (modifier & 0xF) + 4 at the tile, and object 0x10 two columns right of the puzzle's
 * right tile (DS:2B6A) for 8 and up */
extern int8_t puzzle_answer;
static void desert_1e(tile_args *a)
{
	if (a->layer != 0xB && a->layer != 2) return;
	int d = a->mod & 0xF; if (!d) return;
	desc_draw_obj_at(a, d + 4);
	if (d > 7 && a->col - puzzle_answer == 2) desc_draw_obj_at(a, 0x10);
}
const kind_drawers kind_desert = {{
	[0x04] = desert_04, [0x1C] = desert_1c, [0x1D] = desert_1c, [0x1E] = desert_1e,
}, NULL};

/* ---- rooftops (33FD in level 1) ---- */
/* 33FD:073C: tile 0x25, object ((modifier >> 1) & 7) + 0x19 at the tile, behind and in front */
static void roof_25(tile_args *a)
{
	if (a->layer != 0xB && !(a->layer == 0 && redraw_all_flag)) return;
	desc_draw_obj_pair(a, ((mod_lo(a) >> 1) & 7) + 0x19);
}
/* 33FD:0976: tile 0x26 (modifier 0..0xB0: a scroll), objects 19 and 20 (20 in front) and 0x15 + (m & 3) at x -m */
static void roof_26(tile_args *a)
{
	if (a->layer != 0xB || mod_lo(a) > 0xB0) return;
	int16_t save[4]; memcpy(save, draw_clip, sizeof save); memcpy(draw_clip, screen_rect, sizeof save);
	int16_t c = (int16_t)mod_lo(a);
	uint8_t *o = desc_obj(19); wr(o + 3, (int16_t)-c); desc_obj_image_rect(o); draw_object(0x13, 0xB);
	o = desc_obj(20); wr(o + 3, (int16_t)(0x2D - c)); desc_obj_image_rect(o); o[5] = 1; draw_object(0x14, 1); o[5] = 0xB;
	int k = (c & 3) + 0x15; o = desc_obj(k); wr(o + 3, (int16_t)-c); desc_obj_image_rect(o); draw_object(k, 0xB);
	memcpy(draw_clip, save, sizeof save);
}
/* 33FD:05CE: tile 0x27 (modifier below 0xB), object 0x1F + m (1375:0F5A marks the next object's rect for redrawing:
 * no table change) */
static void roof_27(tile_args *a)
{
	if (a->layer != 0xB || mod_lo(a) >= 0xB) return;
	draw_unclipped(mod_lo(a) + 0x1F);
}
const kind_drawers kind_rooftops = {{
	[0x25] = roof_25, [0x26] = roof_26, [0x27] = roof_27,
}, NULL};

/* ---- the final level (33FD in level 14) ---- */
/* 33FD:1898 / 1ACA: the object DS:1A6A[n] + 4 (room 6) / DS:1AA0[n] + 2 (rooms 7, 8) for modifier bits 4..10
 * (n + 1), placed at DS:1A72 / 1AAA[m & 0xF] in layer DS:1A96 / 1ACE[m & 0xF] (modifier bit 11 set) */
static void final_moving(tile_args *a, uint16_t objs, int plus, uint16_t pos, uint16_t layers)
{
	if (a->layer != 1 || !((a->mod >> 8) & 8)) return;
	int si = (int)((mod_lo(a) & 0x7F0) >> 4) - 1; if (si < 0) return;
	int i = ds_byte((uint16_t)(objs + si)) + plus, k = mod_lo(a) & 0xF;
	uint8_t *o = desc_obj(i), rect[8]; memcpy(rect, o + 0xB, 8);
	wr(o + 1, (int16_t)ds_word((uint16_t)(pos + 4 * k))); wr(o + 3, (int16_t)ds_word((uint16_t)(pos + 4 * k + 2)));
	desc_obj_offset(o, rd(o + 3), rd(o + 1));
	o[5] = ds_byte((uint16_t)(layers + k));
	draw_object(i, o[5]);
	o[5] = 0xB; memcpy(o + 0xB, rect, 8); wr(o + 1, 0); wr(o + 3, 0);
}
static void final_29(tile_args *a) { final_moving(a, 0x1A6A, 4, 0x1A72, 0x1A96); }
static void final_2a(tile_args *a) { final_moving(a, 0x1AA0, 2, 0x1AAA, 0x1ACE); }
/* 33FD:0000: tiles 0, 1 and 0x14 with modifier bit 12: the moving objects of rooms 6 / 7, 8 */
static void final_00(tile_args *a)
{
	if (!((a->mod >> 8) & 0x10) || level_kind != 6) return;
	if (drawn_room == 6) final_29(a);
	if (drawn_room == 7 || drawn_room == 8) final_2a(a);
}
/* 33FD:15E8: tile 0x1F, object DS:1A64[m] + 2 (whole-room builds: m = modifier & 0xF below 6; per frame m - 5) */
static void final_1f(tile_args *a)
{
	unsigned k = mod_lo(a) & 0xF;
	if (!redraw_all_flag && k < 5) return;
	if (a->layer != 0xB) return;
	if (!redraw_all_flag) k -= 5;
	if (k < 6) draw_object(ds_byte((uint16_t)(0x1A64 + k)) + 2, 0xB);
}
/* 33FD:16D8: tile 0x28, object (modifier & 7) + 6 with the clip 6 further left */
static void final_28(tile_args *a)
{
	if (a->layer != 0xB) return;
	int16_t l = draw_clip[1]; draw_clip[1] -= 6;
	draw_object((a->mod & 7) + 6, 0xB);
	draw_clip[1] = l;
}
const kind_drawers kind_final = {{
	[0x00] = final_00, [0x01] = final_00, [0x0A] = draw_tile_0a, [0x14] = final_00, [0x1F] = final_1f,
	[0x28] = final_28, [0x29] = final_29, [0x2A] = final_2a,
}, NULL};
