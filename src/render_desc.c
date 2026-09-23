/* Drawing a room's description (0CD6 loads it, 0FB3:0624 / 0712 put its objects into the draw tables): the "CUST"
 * resource of the room (see roomhooks.c) is a header of 0x1C bytes ([0] the object count, [1] the background id,
 * [2] the first image resource) and 0x19-byte objects: [0] image (resource first + image; 0xFF none), [1] y,
 * [3] x (the image's top left), [5] the layer it belongs to, [6] draw mode, [7] its id in image set 4 (set at load:
 * first + object number; -1 none), [9] the image loaded, [0xB] the rect shown (relative to the image; set at load
 * to the screen rect), [0x14] the piece byte of its table entry. */
#include <string.h>
#include "types.h"
#include "globals.h"
#include "render.h"
#include "render_tiles.h"
#include "render_frame.h"

static uint8_t desc[0x1000];   /* the loaded description (DS:[0x01AC]) */
static int desc_ok;
uint16_t word_2ba6;             /* DS:2BA6 (0AAC): the level 14 / level 10 extra pieces are off */

static uint8_t *obj_at(int i) { return desc + 0x1C + 0x19 * i; }
static int16_t rd(const uint8_t *p) { return (int16_t)(p[0] | p[1] << 8); }
static void wr(uint8_t *p, int16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static int rect_empty(const int16_t *r) { return r[0] >= r[2] || r[1] >= r[3]; }   /* 194C:4D50 */
static int sect(int16_t *r, const int16_t *a, const int16_t *b)   /* 194C:5266 */
{
	int16_t t = a[0] > b[0] ? a[0] : b[0], l = a[1] > b[1] ? a[1] : b[1], bo = a[2] < b[2] ? a[2] : b[2], ri = a[3] < b[3] ? a[3] : b[3];
	if (t >= bo || l >= ri) { r[0] = r[1] = r[2] = r[3] = 0; return 0; }
	r[0] = t; r[1] = l; r[2] = bo; r[3] = ri; return 1;
}
static void get_rect(const uint8_t *o, int16_t *r) { for (int k = 0; k < 4; k++) r[k] = rd(o + 0xB + 2 * k); }
static void put_rect(uint8_t *o, const int16_t *r) { for (int k = 0; k < 4; k++) wr(o + 0xB + 2 * k, r[k]); }

/* 0CD6:0398 (the drawing part): the room's description and its images */
void render_desc_load(uint8_t room)
{
	desc_ok = 0;
	if (room_bg == 0) return;
	uint16_t n; const uint8_t *r = room_description_res(room, &n);
	if (!r || n < 0x1C) return;
	if (n > sizeof desc) n = sizeof desc;
	memcpy(desc, r, n); desc_ok = 1;
	int16_t first = rd(desc + 2);
	for (int i = 0; i < (int8_t)desc[0]; i++) {
		uint8_t *o = obj_at(i);
		if (o[0] == 0xFF) {
			wr(o + 9, 0);
			int16_t e[4]; for (int k = 0; k < 4; k++) e[k] = (int16_t)ds_word(0x1F12 + 2 * k);
			put_rect(o, e); wr(o + 7, -1); continue;
		}
		render_register_image(4, first + i, level_kind_dat(), first + (int8_t)o[0]);
		const image_t *im = render_image(4, first + i);
		wr(o + 9, im ? 1 : 0);
		int16_t y = rd(o + 1), x = rd(o + 3), box[4] = {y, x, (int16_t)(y + (im ? im->height : 0)), (int16_t)(x + (im ? im->width : 0))}, rc[4];   /* 194C:13B3 */
		get_rect(o, rc);
		if (rect_empty(rc)) put_rect(o, box);
		else { rc[0] += y; rc[2] += y; rc[1] += x; rc[3] += x; sect(rc, rc, box); put_rect(o, rc); }   /* 194C:50EC, 5266 */
		wr(o + 7, (int16_t)(first + i));
	}
}
int render_desc_loaded(void) { return desc_ok; }
/* the drawn room's description resource as loaded (0CD6:02BE: DS:[0x01AC] before the drawing's changes) */
const uint8_t *render_desc_raw(void) { uint16_t n; const uint8_t *r = room_description_res(drawn_room, &n); return r && n >= 0x1C ? r : NULL; }
/* the description's runtime state (the game's copy at DS:[0x01AC], for the tests) */
void render_desc_set(const uint8_t *raw, int len) { if (len > (int)sizeof desc) len = sizeof desc; memcpy(desc, raw, len); desc_ok = 1; }
int render_desc_bg(void) { return desc_ok ? desc[1] : -1; }
int16_t render_desc_word(int off) { return desc_ok ? rd(desc + off) : 0; }
int render_desc_count_raw(void) { return desc_ok ? (int8_t)desc[0] : 0; }
int render_desc_image_id(int i) { return rd(obj_at(i) + 7); }

/* 33FD:0000 (rooftops): the foreground objects 0x16, 0x17, 0x23, 0x48 go behind the prince while his box
 * (Kid +1B) meets theirs and he is in action 3 or 4 (not image 0x44 in Kid +19) */
extern uint8_t byte_6937, byte_693a; extern int16_t word_6938;
static int rooftops_obj_behind(const uint8_t *o)
{
	if (o[0] != 0x23 && o[0] != 0x16 && o[0] != 0x48 && o[0] != 0x17) return 0;
	int16_t rc[4], kb[4] = {Kid.bbox_top, Kid.bbox_left, Kid.bbox_bottom, Kid.bbox_right}, r[4]; get_rect(o, rc);
	if (!sect(r, rc, kb) || Kid.f19 == 0x44) return 0;
	return Kid.action == 3 || Kid.action == 4;
}
/* 33FD:030E: the box of the grab in progress (DS:6937 steps 1..7): y 0xAC (0xBA hanging), x DS:6938 +-0x10 */
static void grab_rect(int16_t *r)
{
	if (byte_6937 == 0 || (int8_t)byte_6937 > 7) { for (int k = 0; k < 4; k++) r[k] = (int16_t)ds_word(0x1F12 + 2 * k); return; }
	r[0] = byte_693a == 1 ? 0xBA : 0xAC; r[1] = (int16_t)(word_6938 - 0x10); r[2] = (int16_t)(r[0] + 1); r[3] = (int16_t)(word_6938 + 0x10);
}
/* 33FD:0826 (rooms 0x10 and 0x13): objects 6..0xB go behind while a hanging character is being grabbed there */
static int rooftops_obj_behind2(const uint8_t *o)
{
	if (byte_6937 == 0 || byte_693a != 1 || o[0] < 6 || o[0] > 0xB) return 0;
	int16_t g[4], rc[4]; grab_rect(g); get_rect(o, rc);
	return sect(g, g, rc);
}

/* 0FB3:0712: object i into the tables when it belongs to `layer` */
void draw_object(int i, uint8_t layer)
{
	if (!desc_ok || i < 0 || i >= (int8_t)desc[0]) return;
	uint8_t *o = obj_at(i);
	if (rd(o + 7) == -1 || o[5] != layer) return;
	int16_t rc[4], r[4]; get_rect(o, rc);
	if (!sect(r, rc, draw_clip)) return;
	draw_entry *e;
	if (layer == 0 || layer == 5 || layer == 0xB) {
		if (rd(o + 9) == 0 || table_counts[0] >= BACK_MAX) return;
		e = &back_table[table_counts[0]++];
	} else {
		if (table_counts[1] >= FORE_MAX) return;
		e = &fore_table[table_counts[1]++];
		if (level_kind == 5) {
			int back = 0;
			if (drawn_room != 0x13 && drawn_room != 0x10 && drawn_room != 0x0F) back = rooftops_obj_behind(o);
			if (!back && (drawn_room == 0x13 || drawn_room == 0x10)) back = rooftops_obj_behind2(o);
			if (back && table_counts[0] < BACK_MAX) { table_counts[1]--; e = &back_table[table_counts[0]++]; }
		}
	}
	e->y = rd(o + 1); e->x = rd(o + 3); e->id = (uint16_t)rd(o + 7); e->chtab = 4;
	e->col = (uint8_t)draw_col; e->row = (uint8_t)draw_row;
	e->top = r[0]; e->left = r[1]; e->bottom = r[2]; e->right = r[3];
	e->mode = o[6]; e->piece = o[0x14];
	e->mirror = 0;   /* (not set by the original: what the entry held before) */
}
/* 0FB3:228E / 22F4: the extra pieces of three rooms (level 9 room 2 behind, level 14 room 1 and level 10 room 0x16
 * in front, while DS:2BA6 is 0): image ids 0x6370.. at the DS positions */
static void draw_extra(uint8_t layer)
{
	int16_t id = 0; uint16_t pos = 0;
	if (layer == 0) { if (drawn_room == 2 && level_number == 9) { id = 0x6371; pos = 0x452; } }
	else if (layer == 1 && word_2ba6 == 0) {
		if (drawn_room == 1 && level_number == 14) { id = 0x6372; pos = 0x45A; }
		else if (drawn_room == 0x16 && level_number == 10) { id = 0x6370; pos = 0x456; }
	}
	if (!pos) return;
	draw_entry *e;
	if (layer == 0) { if (table_counts[0] >= BACK_MAX) return; e = &back_table[table_counts[0]++]; }
	else { if (table_counts[1] >= FORE_MAX) return; e = &fore_table[table_counts[1]++]; }
	e->mirror = 0; e->y = (int16_t)ds_word(pos); e->x = (int16_t)ds_word(pos + 2); e->id = (uint16_t)id; e->chtab = 4;
	e->col = (uint8_t)draw_col; e->row = (uint8_t)draw_row;
	e->top = draw_clip[0]; e->left = draw_clip[1]; e->bottom = draw_clip[2]; e->right = draw_clip[3];
	e->mode = 0xA; e->piece = cur_tile.tile;   /* (DS:6B72) */
}
__attribute__((weak)) void level14_room1_palette(void) { }   /* 33FD:145E */
/* 0FB3:0624: the description's objects of `layer` (whole-room builds: once, the background ones at the first tile
 * drawn and the foreground ones at the last) */
void render_desc_objects(uint8_t layer)
{
	int si = 0;
	if ((layer == 0 || layer == 5) && redraw_all_flag && draw_col == 0 && draw_row == 2) {
		si = 1;
		if (drawn_room == 1 && level_number == 14) level14_room1_palette();
	} else if (layer == 0 && !redraw_all_flag && desc_ok && rd(desc + 0xE) != 0) si = 1;
	else if ((layer == 1 || layer == 2) && ((draw_col == 9 && draw_row == 0) || !redraw_all_flag)) si = 1;
	if (!si) return;
	if (desc_ok && room_bg != 0) for (int i = 0; i < (int8_t)desc[0]; i++) draw_object(i, layer);
	if (layer == 1 || (redraw_all_flag && layer == 0)) draw_extra(layer);
}

/* 0CD6:06B4 (0FB3:0B78, a whole redraw: before the first back entry with a piece byte): the screen under the objects
 * of piece byte 2 is saved (0CD6:0002 -> 0993:04F0: id 0x64 + image, kind 2, kept); piece byte 3: the kind's own
 * (33FD:0770) */
/* 33FD:0770 (desert, objects of piece byte 3): the screen under six copies of the object's rect, 0x7E lower and 0x40,
 * 0x60, .. 0xE0 right, saved (id 0x64 + image, kind 3: kept) */
static void desc_save_kind3(uint8_t *o)
{
	if (level_kind != 1) { note_missing("33FD:0770"); return; }
	int16_t rc[4]; get_rect(o, rc);
	for (int k = 0; k < 6; k++) {
		int16_t dx = (int16_t)(0x40 + 0x20 * k), r[4] = {(int16_t)(rc[0] + 0x7E), (int16_t)(rc[1] + dx), (int16_t)(rc[2] + 0x7E), (int16_t)(rc[3] + dx)};   /* 194C:50EC */
		render_save_under(r[1], r[3], r[0], (int16_t)(r[2] - r[0]), (uint8_t)(o[0] + 0x64), 3);
	}
}
void render_desc_save_under(void)
{
	if (!desc_ok) return;
	for (int i = 0; i < (int8_t)desc[0]; i++) {
		uint8_t *o = obj_at(i); int16_t r[4]; get_rect(o, r);
		if (o[0x14] == 2) render_save_under(r[1], r[3], r[0], (int16_t)(r[2] - r[0]), (uint8_t)(o[0] + 0x64), o[0x14]);
		else if (o[0x14] == 3) desc_save_kind3(o);
	}
}
/* 0FB3:0CBA (drawing an entry of a description image): an object of piece byte 1 has the screen under the entry's
 * rect saved first (0CD6:0002: id 0x64 + image, kind 1: put back the next frame) */
void render_desc_entry_saved(uint16_t id, const int16_t *rect)
{
	if (!desc_ok) return;
	int i = (int16_t)id - rd(desc + 2);
	if (i < 0 || i >= (int8_t)desc[0]) return;
	uint8_t *o = obj_at(i);
	if (o[0x14] == 1) render_save_under(rect[1], rect[3], rect[0], (int16_t)(rect[2] - rect[0]), (uint8_t)(o[0] + 0x64), o[0x14]);
}
/* 0CD6:0684: the saved screen under object i's image is put back next time (its slot's flag cleared) */
void render_desc_restore_obj(uint8_t image)
{
	int n = 0;
	for (int k = 0; k < saved_count; k++) if (saved_bgs[k].id == (uint8_t)(image + 0x64) && ++n == 1) { saved_bgs[k].flag = 0; return; }   /* 0993:0646 (the first) */
}

/* 0CD6:0792(0) (after a whole redraw): the objects of layer 0xB have the back layers under them redrawn next frame
 * (1375:0F5A with 1375:0DC6; the last object of background 0x21 over DS:1C80) */
void render_desc_after_redraw(void)
{
	if (!desc_ok) return;
	for (int i = 0; i < (int8_t)desc[0]; i++) {
		uint8_t *o = obj_at(i);
		if (o[5] != 0xB) continue;
		int16_t r[4];
		if (desc[1] == 0x21) { if ((int8_t)desc[0] - i - 1 != 0) continue; for (int k = 0; k < 4; k++) r[k] = (int16_t)ds_word((uint16_t)(0x1C80 + 2 * k)); }   /* (background 0x21: only the last, over DS:1C80) */
		else get_rect(o, r);
		mark_tiles_under(mark_back, r, 0xFF);
	}
}

/* ---- for the drawers of the description kinds (render_kind_desc.c) ---- */
uint8_t *desc_obj(int i) { return obj_at(i); }
int desc_count(void) { return desc_ok ? (int8_t)desc[0] : 0; }
/* 194C:13B3 on the object's own image: its rect is the image at its position */
void desc_obj_image_rect(uint8_t *o)
{
	const image_t *im = render_image(4, rd(o + 7)); int16_t y = rd(o + 1), x = rd(o + 3);
	int16_t r[4] = {y, x, (int16_t)(y + (im ? im->height : 0)), (int16_t)(x + (im ? im->width : 0))}; put_rect(o, r);
}
void desc_obj_offset(uint8_t *o, int16_t dx, int16_t dy)   /* 194C:50EC on the object's rect */
{
	int16_t r[4]; get_rect(o, r); r[0] += dy; r[2] += dy; r[1] += dx; r[3] += dx; put_rect(o, r);
}
/* 0CD6:0108: the object moved to tile (col, row) (x + col * 32, y + (row + 1) * 63, its rect too) */
static void obj_at_tile(uint8_t *o, int8_t col, int8_t row)
{
	int16_t dy = (int16_t)((row + 1) * 0x3F), dx = (int16_t)(col << 5);
	wr(o + 1, (int16_t)(rd(o + 1) + dy)); wr(o + 3, (int16_t)(rd(o + 3) + dx)); desc_obj_offset(o, dx, dy);
}
/* 0CD6:007A: object i drawn at the tile of `a` in its layer (its position, rect and layer restored after) */
void desc_draw_obj_at(tile_args *a, int i)
{
	uint8_t *o = obj_at(i), save[0x19]; memcpy(save, o, sizeof save);
	obj_at_tile(o, a->col, a->row);
	o[5] = a->layer;
	draw_object(i, a->layer);
	memcpy(o + 1, save + 1, 4); o[5] = save[5]; memcpy(o + 0xB, save + 0xB, 8);
}
/* 33FD:0000 .. 0826 (rooftops, tile 0x25): object i at the tile in the background, then (unless the grab of
 * 33FD:0826 is on it) again 12 lower and 30 to the left in the foreground */
void desc_draw_obj_pair(tile_args *a, int i)
{
	uint8_t layer = a->layer; a->layer = 0;
	desc_draw_obj_at(a, i);
	uint8_t *o = obj_at(i), save[0x19]; memcpy(save, o, sizeof save);
	obj_at_tile(o, a->col, a->row);
	if (!rooftops_obj_behind2(o)) a->layer = 1;
	memcpy(o + 1, save + 1, 4); memcpy(o + 0xB, save + 0xB, 8);
	wr(o + 1, (int16_t)(rd(o + 1) + 0xC)); wr(o + 3, (int16_t)(rd(o + 3) - 0x1E)); desc_obj_offset(o, -0x1E, 0xC);   /* 33FD:0876 */
	desc_draw_obj_at(a, i);
	memcpy(o + 1, save + 1, 4); memcpy(o + 0xB, save + 0xB, 8);
	a->layer = layer;
}
