/* The tile drawers of the level kinds drawn mostly by their room descriptions (desert = 1, rooftops = 5, the final
 * level = 6): they only place description objects (render_desc.c), animated through the tile modifiers. Transcribed
 * from the 33FD overlay of each kind. */
#include <string.h>
#include "types.h"
#include "globals.h"
#include "render.h"
#include "render_tiles.h"
#include "render_frame.h"

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
/* the tick-time parts (called from kind1.c's tile animations through shell.c): they act on the drawn room's
 * description (DS:[0x01AC]) whichever room the tile is in */
/* 33FD:0576 (tile 4's animation step): object 2's rect grows 2 to the left; the back layers under it are redrawn at
 * the tile, the tile right of it and the two above (1375:0DC6), and the screen saved under it put back (0CD6:0684) */
void render_desert_gate_tick(int8_t tp)
{
	if (desc_count() <= 2) return;
	uint8_t *o = desc_obj(2); wr(o + 0xD, (int16_t)(rd(o + 0xD) - 2));
	int16_t r[4]; for (int k = 0; k < 4; k++) r[k] = rd(o + 0xB + 2 * k);
	mark_back(tp, r); mark_back((int8_t)(tp + 1), r); mark_back((int8_t)(tp - 10), r); mark_back((int8_t)(tp - 9), r);
	render_desc_restore_obj(o[0]);
}
/* 33FD:067C / 0708 (tiles 0x1C / 0x1D, a step of the waves): the back layers under object 1's rect at the tile */
void render_desert_wave_tick(int8_t tp)
{
	if (desc_count() <= 1) return;
	uint8_t *o = desc_obj(1); int16_t r[4]; for (int k = 0; k < 4; k++) r[k] = rd(o + 0xB + 2 * k);
	mark_back(tp, r);
}
/* 17C1:0034 / 00B4: the column / row of tile tp of `room` in the drawn room's grid (-1 / 10 / 3: the neighbours'
 * edge next to it; 0x1E: not in it) */
static int8_t grid_col(int8_t tp, uint8_t room)
{
	int8_t c = (int8_t)(tp % 10);
	if (room == drawn_room || room == room_A || room == room_B) return c;
	if (c == 9 && (room == room_L || room == room_BL || room == room_AL)) return -1;
	if (c == 0 && (room == room_R || room == room_BR || room == room_AR)) return 0xB;
	return 0x1E;
}
static int8_t grid_row(int8_t tp, uint8_t room)
{
	int8_t r = (int8_t)(tp / 10);
	if (room == drawn_room || room == room_L || room == room_R) return r;
	if (room == room_A && r == 2) return -1;
	if (room == room_B && r == 0) return 3;
	return 0x1E;
}
/* 1375:0454: the rect DS:tmpl at the animated tile (DS:6672) on the screen; 0 when it does not show */
int render_trob_rect(uint16_t tmpl, int16_t *r)
{
	int16_t t[4]; for (int k = 0; k < 4; k++) t[k] = (int16_t)ds_word((uint16_t)(tmpl + 2 * k));
	int8_t col = grid_col((int8_t)cur_trob.tilepos, cur_trob.room), row = grid_row((int8_t)cur_trob.tilepos, cur_trob.room);
	return render_rect_at_tile(row, col, t, r);   /* 17C1:016E */
}
/* 1375:0536 / 06A8: the tile right of / below the animated one as the drawn room's request index (0x1E none; -1..-10
 * the row above) */
static int8_t trob_right(void)
{
	int8_t t = (int8_t)cur_trob.tilepos; uint8_t room = cur_trob.room;
	if (room == drawn_room) return t % 10 == 9 ? 0x1E : (int8_t)(t + 1);
	if (room == room_L) return t % 10 == 9 ? (int8_t)(t - 9) : 0x1E;
	if (room == room_A) return t >= 0x14 && t < 0x1D ? (int8_t)(0x12 - t) : 0x1E;
	if (room == room_AL) return t == 0x1D ? -1 : 0x1E;
	return 0x1E;
}
static int8_t trob_below(void)
{
	int8_t t = (int8_t)cur_trob.tilepos; uint8_t room = cur_trob.room;
	if (room == drawn_room) return t < 0x14 ? (int8_t)(t + 10) : 0x1E;
	if (room == room_A) return t >= 0x14 ? (int8_t)(t % 10) : 0x1E;
	return 0x1E;
}
/* 1375:048E / 05BC / 060A: the animated tile itself / the one above it / the one above and right of it as the drawn
 * room's request index (0x1E none; -1..-10 the row above) */
static int8_t trob_self(void)
{
	int8_t t = (int8_t)cur_trob.tilepos; uint8_t room = cur_trob.room;
	if (room == room_A) return t >= 0x14 && t < 0x1E ? (int8_t)(0x13 - t) : 0x1E;
	return room == drawn_room ? t : 0x1E;
}
static int8_t trob_above(void)
{
	int8_t t = (int8_t)cur_trob.tilepos; uint8_t room = cur_trob.room;
	if (room == drawn_room) return t < 10 ? (int8_t)(-1 - t) : (int8_t)(t - 10);
	if (room == room_B) return t < 10 ? (int8_t)(t + 0x14) : 0x1E;
	return 0x1E;
}
static int8_t trob_above_right(void)
{
	int8_t t = (int8_t)cur_trob.tilepos; uint8_t room = cur_trob.room;
	if (room == drawn_room) return t % 10 == 9 ? 0x1E : t < 10 ? (int8_t)(-2 - t) : (int8_t)(t - 9);
	if (room == room_L) return t == 9 ? -1 : t % 10 == 9 ? (int8_t)(t - 0x13) : 0x1E;
	if (room == room_B) return t < 9 ? (int8_t)(t + 0x15) : 0x1E;
	if (room == room_BL) return t == 9 ? 0x14 : 0x1E;
	return 0x1E;
}
/* 1375:0454 with the template's words given (01F4 and 0388 copy theirs to the stack first) */
static int trob_rect_of(const int16_t *t, int16_t *r)
{
	return render_rect_at_tile(grid_row((int8_t)cur_trob.tilepos, cur_trob.room), grid_col((int8_t)cur_trob.tilepos, cur_trob.room), t, r);
}
static void tmpl_words(uint16_t a, int16_t *t) { for (int k = 0; k < 4; k++) t[k] = (int16_t)ds_word((uint16_t)(a + 2 * k)); }
/* the tile animations' redraw requests (1375:01F4..0416, from the tick through shell.c's hook): the rect of a template
 * at the animated tile (DS:6672), its back layers asked again at that tile and its neighbours (1375:0DC6) */
void render_trob_request(int which, uint16_t arg)
{
	int16_t t[4], r[4];
	switch (which) {
	case 0x1F4: {   /* 08D0: a torch (its flame reaches into the tile on the right; on the temple levels above too) */
		int k2 = level_kind == 2;
		tmpl_words(k2 ? 0x07CA : 0x07C2, t);
		if (!trob_rect_of(t, r)) return;
		if (k2) { mark_back((int8_t)cur_trob.tilepos, r); mark_back(trob_above(), r); mark_back(trob_above_right(), r); }
		mark_back(trob_right(), r);
		break; }
	case 0x274:     /* 0C66: the level door */
		if (!render_trob_rect(ds_word((uint16_t)(0x07FC + 2 * level_kind)), r)) return;
		mark_back((int8_t)cur_trob.tilepos, r); mark_back(trob_right(), r); mark_back(trob_above(), r); mark_back(trob_above_right(), r);
		break;
	case 0x2D0:     /* 088C / 1C7E: tile 0x0A's bubbles */
		if (!render_trob_rect(0x07A2, r)) return;
		mark_back(trob_self(), r);
		break;
	case 0x2F8:     /* 15D4 / 16BC: a button goes down or up: the back layers of every tile under the rect */
		if (!render_trob_rect(level_kind == 3 ? 0x07D2 : 0x07BA, r)) return;
		mark_tiles_under(mark_back, r, 0xFF);
		break;
	case 0x334:     /* 170A: a loose floor falls away */
		if (!render_trob_rect(ds_word((uint16_t)(0x07F2 + 2 * level_kind)), r)) return;
		mark_back(trob_self(), r); mark_back(trob_right(), r);
		if (level_kind == 3) mark_back(trob_below(), r);
		break;
	case 0x388: {   /* 0910: a gate (from its top when the tile above and right of it is not empty) */
		tmpl_words(level_kind == 3 ? 0x077A : 0x0782, t);
		int8_t ar = trob_above_right();
		if (ar != 0x1E && ar >= 0 && !tile_is_empty_kind(ROOM_TILES(drawn_room)[ar])) t[0] = 0;
		if (!trob_rect_of(t, r)) return;
		mark_back((int8_t)cur_trob.tilepos, r); mark_back(trob_right(), r); mark_back(trob_above_right(), r);
		break; }
	case 0x416:     /* (arg: the template) 33FD:04B6, a caverns rock */
		if (!arg || !render_trob_rect(arg, r)) return;
		mark_back(trob_self(), r); mark_back(trob_right(), r);
		break;
	}
}
/* 33FD:08BE (tile 0x1E's animation, still moving): the back layers under DS:1498 at the tile (the animated tile's
 * own position as the index), the one right of it and the one below */
void render_desert_tile1e_tick(void)
{
	int16_t r[4];
	if (!render_trob_rect(0x1498, r)) return;
	mark_back((int8_t)cur_trob.tilepos, r); mark_back(trob_right(), r); mark_back(trob_below(), r);
}
/* 33FD:0904 (a puzzle tile pressed, column col): the screens saved under the clue's copies (id 0x72: object 14's six
 * copies, 33FD:0770) that touch x = 32 (col + 1) are put back next frame: the first two with an edge there, or the one
 * spanning it at y 0x72..0x7F */
void render_desert_press(int col)
{
	int16_t x = (int16_t)((col + 1) << 5); int found = 0;
	for (int n = 1; ; n++) {   /* 0993:0646: the n-th slot of id 0x72 */
		int k, m = 0; for (k = 0; k < saved_count; k++) if (saved_bgs[k].id == 0x72 && ++m == n) break;
		if (k >= saved_count) return;
		saved_bg *b = &saved_bgs[k];
		if (b->rect[1] == x || b->rect[3] == x) { b->flag = 0; if (++found == 2) return; }
		else if (b->rect[1] < x && b->rect[3] > x && b->rect[0] == 0x72 && b->rect[2] == 0x7F) { b->flag = 0; return; }
	}
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
/* the tick-time parts of the rooftops' tile animations (anim.c through shell.c) */
static int rect_meets(int16_t *r, const int16_t *a, const int16_t *b) { return render_sect_rect(r, a, b); }   /* 194C:5266 */
/* 33FD:0680 (tile 0x25, a step while visible): object 0x19 at the tile, the tiles under it and under it 12 lower and
 * 30 to the left (33FD:0876) requested, unless a grab in progress (33FD:030E) meets it; the object put back */
void render_roof25_tick(int8_t tp)
{
	if (desc_count() <= 0x19) return;
	int16_t g[4], r[4]; desc_grab_rect(g);
	uint8_t *o = desc_obj(0x19), save[0x19]; memcpy(save, o, sizeof save);
	desc_obj_to_tile(o, (int8_t)(tp % 10), (int8_t)(tp / 10));   /* 0CD6:0108 */
	for (int k = 0; k < 4; k++) r[k] = rd(o + 0xB + 2 * k);
	extern uint8_t byte_6937;
	if (!(byte_6937 && rect_meets(g, g, r))) {
		mark_tiles_under(mark_back, r, 0xFF);
		wr(o + 1, (int16_t)(rd(o + 1) + 0xC)); wr(o + 3, (int16_t)(rd(o + 3) - 0x1E)); desc_obj_offset(o, -0x1E, 0xC);   /* 33FD:0876 */
		for (int k = 0; k < 4; k++) r[k] = rd(o + 0xB + 2 * k);
		mark_tiles_under(mark_back, r, 0xFF);
	}
	memcpy(o + 1, save + 1, 4); memcpy(o + 0xB, save + 0xB, 8);
}
/* the box of object i's image at its position with x = x (194C:13B3), one wider */
static void obj_box(int i, int16_t x, int16_t *r)
{
	uint8_t *o = desc_obj(i); const image_t *im = render_image(4, rd(o + 7)); int16_t y = rd(o + 1);
	r[0] = y; r[1] = x; r[2] = (int16_t)(y + (im ? im->height : 0)); r[3] = (int16_t)(x + (im ? im->width : 0) + 1);
}
/* 33FD:08C0 (tile 0x26, the ship leaving, a step m while the tile is in the drawn room): object 19 at x -m and
 * object 0x15 + ((m - 1) & 3) at x 1 - m: the tiles under their boxes (one wider) requested; object 19's saved
 * screen put back (0CD6:0684), its rect cut at the box's right edge when that falls inside it (33FD:0B12) */
void render_roof26_tick(uint16_t m)
{
	if (desc_count() <= 20 || cur_trob.room != drawn_room) return;
	int16_t r[4];
	uint8_t *o = desc_obj(19); wr(o + 3, (int16_t)-m);
	obj_box(19, (int16_t)-m, r);
	mark_tiles_under(mark_back, r, 0xFF);
	int slot = render_desc_restore_obj(o[0]);
	if ((int8_t)slot >= 0 && slot < saved_count && saved_bgs[slot].rect[1] < r[3] && saved_bgs[slot].rect[3] > r[3]) saved_bgs[slot].rect[3] = r[3];
	int k = (int)(((uint8_t)(m - 1) & 3) + 0x15);
	if (k >= desc_count()) return;
	wr(desc_obj(k) + 3, (int16_t)(1 - m));
	obj_box(k, (int16_t)(1 - m), r);
	mark_tiles_under(mark_back, r, 0xFF);
}
/* 33FD:05AC (tile 0x27, step m): the tiles under object m + 0x1E's rect */
void render_roof27_tick(uint16_t m)
{
	int i = (int)(uint8_t)(m + 0x1E); if (i >= desc_count()) return;
	int16_t r[4]; for (int k = 0; k < 4; k++) r[k] = rd(desc_obj(i) + 0xB + 2 * k);
	mark_tiles_under(mark_back, r, 0xFF);
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
