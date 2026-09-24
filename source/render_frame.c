/* The frame's drawing (169B:0A98: 0FB3:12F4 fills the draw tables, 0FB3:13C2 draws them): the redraw requests per
 * tile (1375:0F16 / 0DC6 / 0E12 / 0E56 / 0E8C, kept at DS:61E4..6662), the tile pass over them (0FB3:1308 / 01E4 /
 * 0358 / 14D4 / 16AE) and the sprites of the frame's objects (DS:5D3A, put into table 3 by 0FB3:18DE / 1A16 / 1C32
 * and 0993:03CC). Transcribed from the disassembly. */
#include <stdio.h>
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "globals.h"
#include "render.h"
#include "render_tiles.h"
#include "render_frame.h"

redraw_t redraw;                    /* DS:61E4..6662 */
frame_obj objs[OBJ_MAX];            /* DS:5D3A */
uint16_t obj_count;                 /* DS:60F8 */
uint8_t obj_list[OBJ_MAX]; uint16_t obj_list_n;   /* DS:27DE / DS:293C: the objects drawn together */
spr_vars sv;                        /* DS:60FA..610D */

static int sect(int16_t *r, const int16_t *a, const int16_t *b) { return render_sect_rect(r, a, b); }
static void unite(int16_t *r, const int16_t *a, const int16_t *b)   /* 194C:647E UnionRect */
{
	int16_t t = a[0] < b[0] ? a[0] : b[0], l = a[1] < b[1] ? a[1] : b[1], bo = a[2] > b[2] ? a[2] : b[2], ri = a[3] > b[3] ? a[3] : b[3];
	r[0] = t; r[1] = l; r[2] = bo; r[3] = ri;
}
static int rect_empty(const int16_t *r) { return r[0] >= r[2] || r[1] >= r[3]; }   /* 194C:4D50 */
static void set_rect_ds(int16_t *r, uint16_t a) { for (int k = 0; k < 4; k++) r[k] = (int16_t)ds_word((uint16_t)(a + 2 * k)); }
static int8_t tile_index(int8_t row, int8_t col) { return (int8_t)(row * 10 + col); }   /* 0AFF:07D4 + col */

/* ---- the redraw requests ---- */
/* 0AFF:0220: the tile index of (row, col): row -1 gives -1 - col; 30 outside */
int8_t tile_index_of(int8_t row, int8_t col)
{
	if (row >= 3 || row < -1 || col >= 10 || col < 0) return 0x1E;
	if (row == -1) return (int8_t)(0xFF - col);
	return tile_index(row, col);
}
/* 1375:0D2E: a request for the row above (index -1 - col), the tile's box cut to r (r NULL: the whole box) */
static void mark_above(int8_t t, const int16_t *r)
{
	if (t > -1 || t < -10) return;
	int16_t b[4]; int8_t col = (int8_t)-((t + 1) % 10);
	tile_rect(-1, col, b);
	if (r && !sect(b, b, r)) return;
	int c = -(t + 1);
	if (redraw.above[c].set) unite(redraw.above[c].rect, redraw.above[c].rect, b);
	else { redraw.above[c].set = 1; memcpy(redraw.above[c].rect, b, sizeof b); }
}
/* 1375:1072: request `arr[t]` for the tile's box cut to r */
static void mark_rect(redraw_rect *arr, int8_t t, const int16_t *r)
{
	int16_t b[4]; tile_rect((int8_t)(t / 10), (int8_t)(t % 10), b);
	if (!sect(b, b, r)) return;
	if (arr[t].set) unite(arr[t].rect, arr[t].rect, b);
	else { arr[t].set = 1; memcpy(arr[t].rect, b, sizeof b); }
}
/* 1375:1020: the characters in the drawn room other than `id` whose box meets r are redrawn (+0x36) */
static void mark_chars(const int16_t *r, uint8_t id)
{
	int n = room_nchars(drawn_room);
	for (int i = 0; i < n; i++) {
		if (chars[i].index == id) continue;
		int16_t b[4] = {chars[i].bbox_top, chars[i].bbox_left, chars[i].bbox_bottom, chars[i].bbox_right}, t[4];
		((uint8_t *)&chars[i])[0x36] |= (uint8_t)sect(t, b, r);
	}
}
/* 1375:0F16 (callback of 0F5A): the fore layer of tile t over r (table 6248) */
void mark_fore(int8_t t, const int16_t *r) { if (t >= 0x1E) return; if (t >= 0) mark_rect(redraw.fore2, t, r); else mark_above(t, r); }
/* 1375:0DC6: the back layers of tile t over r (table 64FA), and the characters there */
void mark_back(int8_t t, const int16_t *r)
{
	if (t >= 0x1E) return;
	if (t >= 0) mark_rect(redraw.back, t, r); else mark_above(t, r);
	mark_chars(r, 0xFF);
}
/* 1375:0E12: the fore layer (2) of tile t over r (table 63CE) */
void mark_fore_part(int8_t t, const int16_t *r) { if (t >= 0x1E) return; if (t >= 0) mark_rect(redraw.fore_part, t, r); else mark_above(t, r); }
/* 1375:0E56: the whole fore layer (2) of tile t (table 6392) */
void mark_fore_full(int8_t t) { if (t >= 0x1E) return; if (t >= 0) redraw.fore_full[t] = 1; else mark_above(t, NULL); }
/* 1375:0E8C: the whole tile t (table 6626) */
void mark_tile(int8_t t) { if (t >= 0x1E) return; if (t >= 0) redraw.full[t] = 1; else mark_above(t, NULL); }
/* 1375:0F5A: every tile under r gets `mark` (rows by (y - 3) / 63, columns by x / 32, both toward 0), then the
 * characters under r other than `id` are redrawn */
void mark_tiles_under(void (*mark)(int8_t, const int16_t *), const int16_t *r, uint8_t id)
{
	if (rect_empty(r)) return;
	int row0 = r[0] >= 3 ? (r[0] - 3) / 0x3F : -1, row1 = r[2] >= 3 ? (r[2] - 3) / 0x3F : -1;
	int col0 = r[1] / 32, col1 = r[3] / 32;   /* (sar of the magnitude: toward 0) */
	for (int8_t row = (int8_t)row0; row <= row1; row++)
		for (int8_t col = (int8_t)col0; col <= col1; col++) mark(tile_index_of(row, col), r);
	mark_chars(r, id);
}

/* ---- the objects' sprites ---- */
/* 0993:03CC (al chtab, dx image + 1, bx x; stack mode, y): a sprite from the drawing variables into table 3 (bottom
 * at y, the rect DS:6103); mirrored when facing left (DS:60FB 0), the scenery's images only with DS:610D >= 0 */
int add_sprite(uint8_t chtab, uint16_t id1, int16_t x, uint8_t mode, int16_t y)
{
	if (id1 == 0 || table_counts[3] >= SPRITE_MAX) return 0;
	int16_t id = (int16_t)(id1 - 1);
	sprite_entry *s = &sprite_table[table_counts[3]];
	s->x = x;
	const image_t *im = NULL;
	if (level_number == 5 && drawn_room == 10 && chtab == 4 && render_desc_count_raw() > id) im = render_image(4, render_desc_image_id(id));   /* 0CD6:0224 */
	else im = render_image(chtab, id + 1);   /* 0993:0FE2 */
	if (!im || !im->height) { if (getenv("FT_DBG")) fprintf(stderr, "add_sprite fail chtab %u id %d\n", chtab, id); return 0; }
	s->y = (int16_t)(y - im->height + 1);
	s->chtab = chtab; s->id = (uint16_t)id;
	memcpy(s->rect, sv.rect, sizeof s->rect);
	s->mode = mode; s->mask = 0;
	s->mirror = sv.dir == 0 && !(chtab == 4 && (int8_t)sv.charidx < 0);
	s->layer = 0xFF;
	sprite_guard[table_counts[3]] = render_guard_type;
	table_counts[3]++;
	return 1;
}
/* 0FB3:1E4E: an object's fields into the drawing variables; its type */
static uint8_t load_obj(int i)
{
	frame_obj *o = &objs[i];
	sv.key = o->key; obj_x = o->x; sv.x0 = o->x; obj_y = o->y; obj_id = (int16_t)o->id; obj_chtab = o->chtab; sv.dir = o->dir;
	memcpy(sv.rect, o->rect, sizeof sv.rect); sv.mask = o->mask; sv.charidx = o->charidx;
	return o->type;
}
/* 0FB3:1BCA: the drawing order of a type with bit 0x80 (DS:0854) or not (DS:0844) */
static uint8_t type_order(uint8_t t) { return t & 0x80 ? ds_byte(0x0854 + (t & 0x7F)) : ds_byte(0x0844 + t); }
/* 0FB3:1A68: swap list entries i and i + 1 (a, b)? Same type: by y, then x; the collapsing skeleton frames (types
 * 7 / 8 at frames 0xB5 / 0xA6) first; then by the types' order (DS:0844 / 0854), y, x */
static int obj_after(int i)
{
	const frame_obj *oa = &objs[obj_list[i]], *ob = &objs[obj_list[i + 1]];
	uint16_t fa = oa->frame, fb = ob->frame; uint8_t ta = oa->type, tb = ob->type;
	if (ta == tb) return ob->y != oa->y ? ob->y < oa->y : ob->x < oa->x;
	if (ta != 7 && ta != 8 && (tb == 7 || tb == 8) && (fb == 0xB5 || fb == 0xA6)) return 1;
	if ((ta == 7 || ta == 8) && tb != 7 && tb != 8 && (fa == 0xB5 || fa == 0xA6)) return 0;
	uint8_t pa = type_order(ta), pb = type_order(tb);
	if (pa != pb) return pb > pa;
	if (ob->y == oa->y) return ob->x < oa->x;
	if ((ta & 0x80) || (tb & 0x80)) return ob->y > oa->y;
	return ob->y < oa->y;
}
/* 0FB3:1A16: bubble sort of the list */
static void sort_list(void)
{
	int swapped;
	do {
		swapped = 0;
		for (int i = 0; i < (int)obj_list_n - 1; i++)
			if (obj_after(i)) { uint8_t t = obj_list[i]; obj_list[i] = obj_list[i + 1]; obj_list[i + 1] = t; swapped = 1; }
	} while (swapped);
}

void obj_hook_186a_038c(uint8_t type);   /* render_hooks.c */
void obj_hook_347c_02d6(uint8_t type);   /* render_hooks.c */
void obj_hook_347c_0f5e(uint8_t type);   /* render_hooks.c */

void obj_hook_33fd_1e72(uint8_t type);   /* render_hooks.c */
/* 0FB3:1DBA (types 0x80, 0x81, 0x83): two scenery sprites by level kind: temple 0x15 / 0x16, caverns 0x42 + n /
 * 0x46 + n (n = type & 0x7F; x + 1 and the rect 5 lower), ruins 0x16 / 0x1A */
static void obj_scenery(uint8_t type)
{
	int16_t a, b; int16_t dx = 0, dy = 0;
	switch (level_kind) {
	case 2: a = 0x15; b = 0x16; break;
	case 3: a = (int16_t)((type & 0x7F) + 0x42); b = (int16_t)((type & 0x7F) + 0x46); dx = 1; dy = 4; sv.rect[2] += 5; break;
	case 4: a = 0x16; b = 0x1A; break;
	default: return;
	}
	sv.dir = 0xFF;
	add_sprite(4, (uint16_t)a, (int16_t)(sv.x0 + dx), 10, obj_y);
	add_sprite(4, (uint16_t)b, sv.x0, 10, (int16_t)(obj_y + dy));
}
/* 0FB3:1C32: object i into table 3 by its type */
static void draw_obj(int i)
{
	uint8_t t = load_obj(i);
	render_guard_type = obj_chtab == 3 && (t == 10 || t == 12) ? charid_to_type[t] : 0xFF;   /* (0993:0F36: the guard file of the character's type) */
	switch (t) {
	case 0:
		if (obj_id == -1) return;
		add_sprite(obj_chtab, (uint16_t)(obj_id + 1), sv.x0, 10, obj_y);
		if (sv.mask) sprite_table[table_counts[3] - 1].mask = sv.mask;   /* (after a failed add: the entry past the last) */
		return;
	case 1: case 2: case 3: case 5: case 6: case 7: case 8: case 9: case 10: case 0xE:
		if (add_sprite(obj_chtab, (uint16_t)(obj_id + 1), sv.x0, 10, obj_y)) { sprite_table[table_counts[3] - 1].mask = sv.mask; sprite_table[table_counts[3] - 1].layer = sv.charidx; }
		return;
	case 0xD: obj_hook_37f0_023a(t); return;
	case 0x80: case 0x81: case 0x83: obj_scenery(t); return;
	case 0x82: add_sprite(obj_chtab, 0x67, sv.x0, 10, obj_y); return;
	case 0x84: case 0x85: obj_hook_186a_038c(t); return;
	case 0x86: case 0x87: case 0x88: case 0x89: obj_hook_347c_02d6(t); return;
	case 0x8A: obj_hook_347c_0f5e(t); return;
	case 0x8B: obj_hook_37f0_08d2(t); return;
	case 0x8C: obj_hook_33fd_1e72(t); return;
	default: return;
	}
}
static void draw_obj_wrap(int i) { draw_obj(i); render_guard_type = 0xFF; }
/* 0FB3:18DE: the objects keyed to tile `key` and, repeatedly, every object meeting the box of those found (from the
 * clip on), last first; drawn sorted in that box, then marked drawn (key 0xFE) */
void draw_objs_at(uint8_t key)
{
	if (!obj_count) return;
	int16_t save[4], acc[4], u[4]; memcpy(save, draw_clip, sizeof save); memcpy(acc, draw_clip, sizeof acc);
	obj_list_n = 0;
	for (int i = obj_count - 1; i >= 0; i--) {
		frame_obj *o = &objs[i];
		if (o->key != key) {
			if ((int8_t)o->key < 0) continue;
			if (!sect(u, acc, o->rect)) continue;
		}
		obj_list[obj_list_n++] = (uint8_t)i;
		unite(u, acc, o->rect);
		if (memcmp(u, acc, sizeof u)) { i = obj_count; obj_list_n = 0; memcpy(acc, u, sizeof acc); }   /* (2699:015E EqualRect: start over) */
	}
	if (obj_list_n) {
		memcpy(draw_clip, acc, sizeof acc);
		sort_list();
		for (int k = 0; k < obj_list_n; k++) { draw_obj_wrap(obj_list[k]); objs[obj_list[k]].key = 0xFE; }
	}
	memcpy(draw_clip, save, sizeof save);
}

/* ---- the tile pass over the requests ---- */
static void call_drawer2(tile_args *a) { if (tile_drawers && a->tile < 0x2D && tile_drawers->by_tile[a->tile]) tile_drawers->by_tile[a->tile](a); }
static int drawer_of(uint8_t tile) { return tile_drawers && tile < 0x2D && tile_drawers->by_tile[tile]; }
static int empty_type(uint8_t t) { return t == 0 || t == 9 || t == 0x21 || t == 0x23 || t == 0x1B || t == 0x25; }   /* 0FB3:28D4 */
static int wall_type_290a(uint8_t t) { return t == 0x14 || t == 2 || t == 7 || t == 0x19 || t == 0x2B; }             /* 0FB3:290A */
int temple_wall_rect(int8_t row, uint8_t room, int16_t *r);   /* 347C:0238 (render_hooks.c) */
/* 17C1:016E: r moved to tile (row, col) and cut to the screen; 0 outside */
int render_rect_at_tile(int row, int col, const int16_t *src, int16_t *dst)
{
	if (col == 0x1E || row == 0x1E) return 0;
	int16_t t[4] = {(int16_t)(src[0] + 0x3F * row), (int16_t)(src[1] + 32 * col), (int16_t)(src[2] + 0x3F * row), (int16_t)(src[3] + 32 * col)};   /* 194C:50EC */
	return sect(dst, t, screen_rect);
}
/* 0FB3:15E2: while the prince (or a shadow in his room) climbs (frames 0x87..0x90) his frame's box (DS:03F2 + 8 n)
 * moved to this tile: the part of the fore layer to draw again */
static int climb_rect(int16_t *out)
{
	uint16_t f = 0;
	if (Kid.frame >= 0x87 && Kid.frame < 0x91) f = Kid.frame;
	else {
		int n = room_nchars(Char.room);
		for (int i = 0; i < n && !f; i++) if (chars[i].charid == 1 && chars[i].frame >= 0x87 && chars[i].frame < 0x91) f = chars[i].frame;
	}
	if (!f) return 0;
	int16_t r[4]; for (int k = 0; k < 4; k++) r[k] = (int16_t)ds_word((uint16_t)(f * 8 - 0x36 + 2 * k));
	if (cur_tile.tile == 6) r[2]++;
	if ((Char.room == 7 || Char.room == 8) && level_kind == 6) r[0] += 3;
	return render_rect_at_tile(draw_row, draw_col, r, out);
}
static void fore_args(tile_args *a, int8_t col, const tile_mod *t) { a->layer = 2; a->col = col; a->row = draw_row; a->tile = t->tile; a->mod = t->mod; }
static int layer_bits(const tile_mod *t) { return (int)((t->mod >> 14) & 3); }
/* 0FB3:14D4: the whole fore layer (2) of this tile again, cut to the climbing box (and on the temple to below a moving
 * wall), then the fore layer (1) over that box */
static void redraw_fore_full(void)
{
	int16_t r[4]; if (!climb_rect(r)) return;
	if (level_kind == 2) {
		int16_t w[4], t[4];
		if (temple_wall_rect(draw_row, drawn_room, w) && sect(t, w, r)) r[0] = (int16_t)(w[2] - 1);
	}
	int16_t save[4]; memcpy(save, draw_clip, sizeof save); memcpy(draw_clip, r, sizeof r);
	tile_args a; fore_args(&a, draw_col, &cur_tile);
	int v = layer_bits(&cur_tile);
	if (drawer_of(cur_tile.tile) && (!byte_5ce7 || v == 2 || v == 0)) call_drawer2(&a);
	mark_fore(tile_index(draw_row, draw_col), r);
	if (render_desc_loaded() && render_desc_word(0xC)) render_desc_objects(2);   /* 0FB3:0624 */
	memcpy(draw_clip, save, sizeof save);
}
/* 0FB3:16AE: the fore layer (2) of the left tile and this one below the row's top (y 63 row + 0x35), in the clip */
static void redraw_fore_part(void)
{
	int16_t save[4]; memcpy(save, draw_clip, sizeof save);
	int16_t y = (int16_t)(0x3F * draw_row + 0x35);
	if (y > draw_clip[0]) draw_clip[0] = y;
	int si = 0;
	if (level_kind == 2) {
		int16_t w[4], t[4];
		if (temple_wall_rect(draw_row, drawn_room, w) && sect(t, w, draw_clip)) { draw_clip[0] = (int16_t)(w[2] - 1); si = 1; }
		else if (temple_wall_rect((int8_t)(draw_row + 1), drawn_room, w) && sect(t, w, draw_clip)) si = 1;
	}
	tile_args a;
	if (empty_type(left_tile.tile)) {
		int v = layer_bits(&left_tile);
		if ((!byte_5ce7 || v == 2 || v == 0) && drawer_of(left_tile.tile)) { fore_args(&a, (int8_t)(draw_col - 1), &left_tile); call_drawer2(&a); }
		v = layer_bits(&cur_tile);
		if (!byte_5ce7 || v == 2 || v == 0) {
			if (drawer_of(cur_tile.tile)) { fore_args(&a, draw_col, &cur_tile); call_drawer2(&a); mark_fore(tile_index(draw_row, draw_col), draw_clip); }
			if (render_desc_loaded() && render_desc_word(0xC)) render_desc_objects(2);
		}
	} else if (si && empty_type(cur_tile.tile) && wall_type_290a(left_tile.tile)) {
		int v = layer_bits(&left_tile);
		if ((!byte_5ce7 || v == 2) && drawer_of(left_tile.tile)) { fore_args(&a, (int8_t)(draw_col - 1), &left_tile); call_drawer2(&a); }
	}
	memcpy(draw_clip, save, sizeof save);
}
static void back_layers_and_fore(void) { draw_one_tile(); }   /* 0FB3:03DC x3 + 0858 (render_tiles.c) */
/* 0FB3:01E4: the requests for tile t (row, col of the drawing globals) */
static void redraw_tile(int8_t t)
{
	if (redraw.full[t]) { redraw.full[t] = 0; draw_one_tile(); }   /* 0FB3:01CA */
	else if (redraw.back[t].set) {
		redraw.back[t].set = 0;
		int16_t save[4]; memcpy(save, draw_clip, sizeof save);
		if (sect(draw_clip, draw_clip, redraw.back[t].rect)) back_layers_and_fore();
		memcpy(draw_clip, save, sizeof save);
	}
	if (redraw.fore_part[t].set) {
		redraw.fore_part[t].set = 0;
		int16_t r[4];
		if (redraw.fore_full[t] && climb_rect(r)) unite(redraw.fore_part[t].rect, redraw.fore_part[t].rect, r);
		int16_t save[4]; memcpy(save, draw_clip, sizeof save);
		if (sect(draw_clip, draw_clip, redraw.fore_part[t].rect)) redraw_fore_part();
		memcpy(draw_clip, save, sizeof save);
	} else if (redraw.fore_full[t]) { redraw.fore_full[t] = 0; redraw_fore_full(); }
	if (redraw.objs_at[t]) { draw_objs_at((uint8_t)t); redraw.objs_at[t] = 0; }
	if (redraw.fore2[t].set) {
		redraw.fore2[t].set = 0;
		if (sect(draw_clip, draw_clip, redraw.fore2[t].rect)) render_draw_fore();   /* 0FB3:0858 */
	}
}
/* 0FB3:0358: the requests for column `col` of the row above */
static void redraw_above(int8_t col)
{
	if (!redraw.above[col].set) return;
	redraw.above[col].set = 0;
	int8_t row = draw_row; int16_t save[4]; memcpy(save, draw_clip, sizeof save);
	memcpy(draw_clip, redraw.above[col].rect, sizeof draw_clip);
	draw_row = -1;
	render_draw_layer(0); render_draw_layer(5); render_draw_layer(0xB); render_draw_fore();
	draw_row = row; memcpy(draw_clip, save, sizeof save);
}
/* 0FB3:1308: the tile pass: objects keyed 0x1E first, rows 2..0 with every column's requests, the row above (from
 * the room above), then the objects not drawn yet */
void redraw_requested(void)
{
	load_side_columns();
	draw_objs_at(0x1E);
	for (draw_row = 2; draw_row >= 0; draw_row--) {
		load_row_above(); load_row_below();
		for (draw_col = 0; draw_col < 10; draw_col++) { load_cur_tiles(); redraw_tile(tile_index(draw_row, draw_col)); }
	}
	if (byte_5ce7 == 0) {
		uint8_t si = drawn_room;
		drawn_room = room_A; set_neighbour_rooms();   /* 0FB3:0026 */
		load_side_columns();
		draw_row = 2; load_row_above(); load_row_below();
		for (draw_col = 0; draw_col < 10; draw_col++) { load_cur_tiles(); redraw_above(draw_col); }
		drawn_room = si; set_neighbour_rooms();
	} else for (int i = 0; i < 11; i++) { row_above[i].tile = 0; row_above[i].mod = 0xC000; }   /* 0CD6:0142 */
	draw_objs_at(0xFF);
}
/* 0FB3:1BF2: the frame's object of image set `chtab` and type `type` (a character's sprite): its rect; NULL none */
const int16_t *sprite_rect_of(int chtab, uint8_t type)
{
	for (int i = 0; i < obj_count; i++) if (objs[i].chtab == chtab && objs[i].type == type) return objs[i].rect;
	return NULL;
}

/* ---- the frame ---- */
/* 194C:4D72 (EraseRect): a rectangle of the port in its background color (the offscreen port's: 0) */
void render_erase_rect(const int16_t *r)
{
	for (int y = r[0] < 0 ? 0 : r[0]; y < r[2] && y < 192; y++)
		for (int x = r[1] < 0 ? 0 : r[1]; x < r[3] && x < 320; x++) offscreen[y * SCREEN_W + x] = 0;
}
/* 33FD:0186 / 0128 (rooftops): the level's colors 0x40..0xEF (PALS 3500, 0xB0 colors) sub-palette 1 in the sea rooms
 * 0x13 / 0x10 / 0xF, 0 elsewhere, loaded when it changes (DS:2B68, which the game logic keeps: here its own copy) */
static int kind5_pal = -1;
static void room_palette_33fd_0186(void)
{
	int want = drawn_room == 0x13 || drawn_room == 0x10 || drawn_room == 0xF;
	if (want != kind5_pal) { kind5_pal = want; render_pal_load(want, 0xB0, 0x40, 3500); }
}
void render_pal_level_reset(void) { kind5_pal = -1; }
/* 0CD6:003A: ruins without a room above: the top 3 rows (DS:0986) erased; rooftops: the rooms' palette */
static void room_prepare(void)
{
	if (level_kind == 4) { if (room_A == 0) { int16_t r[4]; for (int k = 0; k < 4; k++) r[k] = (int16_t)ds_word((uint16_t)(0x0986 + 2 * k)); render_erase_rect(r); } }
	else if (level_kind == 5) room_palette_33fd_0186();
}
/* ---- the tick-time redraw requests (frontends) ----
 * The game logic asks for redraws when it changes the drawn room's tiles (the tile animations 1375:01F4..0416, the
 * overlays' 1375:0F5A / 0E8C calls, items, gates); the core leaves those requests out. With render_track_tiles on,
 * a frame compares the drawn room's tiles and modifiers (and the row above's) with those of the last frame and asks
 * for every changed tile to be redrawn whole (1375:0E8C, with the characters over it: 1375:0F5A) - more than the
 * game's own requests, drawing the same pixels. Off for the tests (they take the game's requests from its state). */
int render_track_tiles;
static uint8_t seen_room, seen_above, seen_tiles[40]; static uint32_t seen_mods[40]; static int seen_ok;
static void tiles_now(uint8_t *t, uint32_t *m)
{
	for (int i = 0; i < 30; i++) { t[i] = drawn_room ? ROOM_TILES(drawn_room)[i] : 0; m[i] = drawn_room ? ROOM_ATTRS(drawn_room)[i] : 0; }
	for (int c = 0; c < 10; c++) { t[30 + c] = room_A ? ROOM_TILES(room_A)[20 + c] : 0; m[30 + c] = room_A ? ROOM_ATTRS(room_A)[20 + c] : 0; }
}
static void tiles_seen(void) { tiles_now(seen_tiles, seen_mods); seen_room = drawn_room; seen_above = room_A; seen_ok = 1; }
/* a tile the tick changed with its own request (1375:1C7E's landing: 2296(0), the falling floor's box): the change
 * accepted as seen, so it is not redrawn whole */
void render_tile_modelled(uint8_t room, int8_t tp)
{
	if (!seen_ok || seen_room != drawn_room || seen_above != room_A || tp < 0 || tp >= 30 || !room) return;
	int i = room == drawn_room ? tp : room == room_A && tp >= 20 ? 30 + tp - 20 : -1;
	if (i < 0) return;
	uint8_t t[40]; uint32_t m[40]; tiles_now(t, m);
	seen_tiles[i] = t[i]; seen_mods[i] = m[i];
}
/* the tiles whose tick-time requests the core reports through its hooks (shell.c): not tracked */
static int tick_requests_modelled(uint8_t t)
{
	return (level_kind == 1 && (t == 4 || t == 0x1C || t == 0x1D || t == 0x1E))   /* 33FD:0538 / 0658 / 06E4 / 07CE */
	    || (level_number == 5 && (t == 0x1B || t == 0x2C))                           /* 37F0:0756 / 06CE */
	    || (level_kind == 5 && (t == 0x25 || t == 0x26 || t == 0x27))                /* 33FD:0680 / 08C0 / 05AC */
	    || t == 0x13 || t == 0x20 || t == 0xA || t == 0x11 || t == 4 || t == 5 || t == 6 || t == 0x22   /* 1375:01F4 / 02D0 / 0274 / 0388 / 02F8 */
	    || t == 0xB                                                                   /* 1375:0334 */
	    || (level_kind == 3 && t == 0x24);                                            /* 1375:0416 (33FD:04B6) */
}
static void mark_changed_tiles(void)
{
	uint8_t t[40]; uint32_t m[40]; tiles_now(t, m);
	if (seen_ok && seen_room == drawn_room && seen_above == room_A)
		for (int i = 0; i < 40; i++) {
			if (t[i] == seen_tiles[i] && m[i] == seen_mods[i]) continue;
			if (t[i] == seen_tiles[i] && tick_requests_modelled(t[i])) continue;
			int8_t row = i < 30 ? (int8_t)(i / 10) : -1, col = (int8_t)(i < 30 ? i % 10 : i - 30);
			int16_t r[4]; tile_rect(row, col, r);
			mark_tile(tile_index_of(row, col)); mark_chars(r, 0xFF);
		}
	memcpy(seen_tiles, t, sizeof t); memcpy(seen_mods, m, sizeof m); seen_room = drawn_room; seen_above = room_A; seen_ok = 1;
}
/* 169B:0A8A .. 0A9D: a frame: the tables filled (0FB3:12F4) and drawn (0FB3:13C2) */
void render_frame(void)
{
	tile_drawers = kind_drawers_for(level_kind);   /* (the level kind's overlay, loaded with the level) */
	render_check_pieces();
	if (render_track_tiles) mark_changed_tiles();
	render_frame_tables();
	render_draw_tables();
	render_present();   /* 0FB3:218A */
}
/* 169B:0430 (the drawing): the whole room: its tables built (0FB3:0002: saved screens dropped, 0FB3:0122, the
 * requests cleared by 1375:1476) and drawn, the description's after-redraw requests (0CD6:0792), then a frame's
 * characters and objects over it */
void render_redraw_room(void)
{
	tile_drawers = kind_drawers_for(level_kind);
	render_check_pieces();
	redraw_all_flag = 1;
	render_free_saved();   /* 0993:075E */
	memset(table_counts, 0, sizeof table_counts); obj_count = 0;
	set_rect_ds(sv.rect, 0x097E);   /* 0AFF:08BE */
	render_desc_load(drawn_room);
	draw_room_tiles();     /* 0FB3:0122 */
	memset(&redraw, 0, sizeof redraw);   /* 1375:1476 */
	room_prepare();        /* 0CD6:003A */
	render_draw_tables();
	render_desc_after_redraw();   /* 0CD6:0792(0) */
	redraw_all_flag = 0;
	if (render_track_tiles) tiles_seen();
}
static int loaded_level = -1;   /* the level render_level_loaded() last ran for */
void render_check_level(void) { if (loaded_level != level_number) render_level_loaded(); }   /* (a level whose load was not reported) */
void render_redraw_all(void)
{
	render_check_level();
	render_redraw_room(); render_frame_tables(); render_draw_tables();
	render_pal_guards();    /* 2D3E:0F50 */
	render_present_all();
	render_pal_restore();   /* 0FB3:294C */
}
/* a level loaded in full (1286:01F2: its image sets made, 1286:066A, and its palettes, 1286:00EA / 01DE / 096D /
 * 07CE): the KID.DAT images' colors forgotten, the level's palette set */
void render_level_loaded(void)
{
	loaded_level = level_number;
	render_reset_images();
	render_load_pieces();
	render_pal_level_start();
}
