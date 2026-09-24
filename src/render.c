/* The drawing: image sets, the draw tables' entries and drawing them. Transcribed from 0993:0008 (table entries),
 * 0993:0DC8 / 0E70 / 0F36 / 1034 (the image sets), 0FB3:0B78 / 0BFC / 0EE0 / 11AE (drawing the tables), 0823:1414 /
 * 1447 / 14DB (mirroring an image in place) and the blitters 2583:0006 (row runs), 194C:04BA (4-bit images) and
 * 194C:6D42 (8-bit images, the transfer modes); see render.h. */
#include <stdio.h>
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "globals.h"
#include "dat.h"
#include "render.h"
#include "render_tiles.h"
#include "render_frame.h"
extern int8_t draw_row, draw_col;
extern uint16_t redraw_all_flag;
const char *level_kind_dat(void);
const char *game_path(const char *name);

uint8_t offscreen[SCREEN_W * 192];
uint8_t screen_buf[SCREEN_W * SCREEN_H];
uint8_t render_palette[256 * 3];
draw_entry back_table[BACK_MAX], fore_table[FORE_MAX];
sprite_entry sprite_table[SPRITE_MAX];
uint8_t sprite_guard[SPRITE_MAX] = {[0 ... SPRITE_MAX - 1] = 0xFF};   /* the guard type each sprite of set 3 was loaded for (0xFF: the level's) */
uint16_t table_counts[5];
int16_t draw_clip[4] = {0, 0, 192, 320};
uint16_t dirty_count; int16_t dirty_rects[DIRTY_MAX][4];
int16_t *render_owner; int16_t render_owner_id;   /* (tests) the entry that last wrote each pixel */
static uint8_t *port_bits = offscreen;   /* the current port's pixels (DS:[2450]: the offscreen buffer, or the screen 5D0A) */
#define PUT(p, v) do { port_bits[p] = (v); if (render_owner && port_bits == offscreen) render_owner[p] = render_owner_id; } while (0)

/* ---- the game's files and a cache of decoded images by (file, resource) ---- */
static dat_file dats[16]; static char dat_names[16][32];
static dat_file *dat_of(const char *name)
{
	if (!name) return NULL;
	for (int i = 0; i < 16; i++) if (dat_names[i][0] && !strcmp(dat_names[i], name)) return dats[i].data ? &dats[i] : NULL;
	for (int i = 0; i < 16; i++) if (!dat_names[i][0]) {
		snprintf(dat_names[i], sizeof dat_names[i], "%s", name);
		if (!dat_open(&dats[i], game_path(name))) { dats[i].data = NULL; return NULL; }
		return &dats[i];
	}
	return NULL;
}
/* the level kind's piece table (its file's PIEC 3500, 19 bytes a tile type, loaded with the level to DS:[0x1090];
 * the tile drawers rewrite parts of it while the level runs) */
static uint8_t piece_copy[0x1000]; static int piece_kind = -1;
void render_load_pieces(void)
{
	extern uint8_t *piece_table;
	uint16_t n; dat_file *d = dat_of(level_kind_dat()); const uint8_t *p = d ? dat_find(d, "CEIP", 3500, &n) : NULL;
	memset(piece_copy, 0, sizeof piece_copy);
	if (p) memcpy(piece_copy, p, n < sizeof piece_copy ? n : sizeof piece_copy);
	piece_table = p ? piece_copy : NULL; piece_kind = level_kind;
}
void render_check_pieces(void) { extern uint8_t *piece_table; if (!piece_table || (piece_table == piece_copy && piece_kind != level_kind)) render_load_pieces(); }   /* (a table set from elsewhere, the tests', is kept) */
#define CACHE_N 4096
static struct { char dat[16]; int res; image_t *im; } cache[CACHE_N]; static int n_cache;
static const image_t *decode_res(const char *datname, int res)
{
	if (!datname) return NULL;
	for (int i = 0; i < n_cache; i++) if (cache[i].res == res && !strcmp(cache[i].dat, datname)) return cache[i].im;
	dat_file *d = dat_of(datname); if (!d) return NULL;
	uint16_t sz; const uint8_t *r = dat_find(d, "PAHS", (uint16_t)res, &sz);
	image_t *im = calloc(1, sizeof *im);
	if (r && r + sz < d->data + d->size) sz++;   /* (the game loads a resource with one byte more, the next one's checksum byte: a
	                                              * last LZG reference can read it, CAVERNS.DAT 3525) */
	if (!r || !image_decode(r, sz, im)) { free(im); im = NULL; }
	if (n_cache < CACHE_N) { snprintf(cache[n_cache].dat, sizeof cache[n_cache].dat, "%s", datname); cache[n_cache].res = res; cache[n_cache].im = im; n_cache++; }
	return im;
}

/* images registered into an image set outside its own ids (a room description's images: 26BC:073C) */
#define EXTRA_MAX 128
static struct { int chtab, id, res; char dat[32]; uint16_t mask; } extra[EXTRA_MAX]; static int n_extra;   /* mask: the conversion mask of the image's load (0: the file's) */
void render_register_image(int n, int id, const char *dat, int res)
{
	if (!dat) return;
	for (int i = 0; i < n_extra; i++) if (extra[i].chtab == n && extra[i].id == id) { extra[i].res = res; extra[i].mask = 0; snprintf(extra[i].dat, sizeof extra[i].dat, "%s", dat); return; }
	if (n_extra == EXTRA_MAX) return;
	extra[n_extra].chtab = n; extra[n_extra].id = id; extra[n_extra].res = res; extra[n_extra].mask = 0;
	snprintf(extra[n_extra].dat, sizeof extra[n_extra].dat, "%s", dat); n_extra++;
}
/* a registered image loaded with another mask than its file's (26BC:040A with a shape list's copy: 37F0:0510) */
void render_image_set_mask(int n, int id, uint16_t mask) { for (int i = 0; i < n_extra; i++) if (extra[i].chtab == n && extra[i].id == id) extra[i].mask = mask; }
static uint16_t image_mask(int n, int id) { for (int i = 0; i < n_extra; i++) if (extra[i].chtab == n && extra[i].id == id) return extra[i].mask; return 0; }
void render_set_chtab(int n, const char *dat, uint16_t first, uint8_t pal_base) { (void)n; (void)dat; (void)first; (void)pal_base; }   /* (the sets follow the game state: render_image_res) */

/* DS:0672: the guard file of each level type (type 4 has none) */
static const char *const guard_dat[10] = {"GUARD.DAT", "FLAME.DAT", "SKELETON.DAT", "GUARD.DAT", NULL, "HEAD.DAT", "HEAD.DAT", "BIRD.DAT", "HEAD.DAT", "JINNEE.DAT"};
uint8_t render_guard_type = 0xFF;   /* the type the guard set was loaded for (0xFF: the level's) */
static uint16_t level_word(uint16_t a) { const uint8_t *l = (const uint8_t *)&level + (a - 0x2BB8); return (uint16_t)(l[0] | l[1] << 8); }   /* (the level at DS:2BB8) */
/* 1286:07EE: the guard type's image range k (DS:06BC[type] lows, DS:06D2[type] highs) comes from the second bank:
 * ranges 0..2 by the level's words DS:4410/4412/4414, the others for type 7 or on level kind 6 */
static int guard_range_on(uint8_t t, int k)
{
	if (t == 0xFF) return 0;
	uint16_t p = ds_word(0x06BC + 2 * t); if (!p) return 0;
	int n = 0; while (ds_word((uint16_t)(p + 2 * n)) != 0xFFFF) n++;
	if (n < k) return 0;
	if (k < 3) return level_word((uint16_t)(0x4410 + 2 * k)) != 0;
	return t == 7 || level_kind == 6;
}
/* the file and resource of image n (1-based, as the tables hold it) of set `chtab`; 0 if none */
int render_image_res(int chtab, int n, const char **dat, int *res)
{
	for (int i = 0; i < n_extra; i++) if (extra[i].chtab == chtab && extra[i].id == n) { *dat = extra[i].dat; *res = extra[i].res; return 1; }
	int i = n - 1; if (i < 0) return 0;
	switch (chtab) {
	case 0:   /* DS:60E6 (1286:0454): 1000 + n, 1200 + n with sword type 2; with type 1 and DS:4410 the images 47..57 (the
	           * guards' swords, DS:06E8 / 06EA) are 1101 + i (26BC:0288) */
		*dat = "PRINCE.DAT"; *res = (byte_5cba == 2 ? 1200 : 1000) + n;
		if (byte_5cba == 1 && level_word(0x4410) && i >= (int16_t)ds_word(0x06E8) && i <= (int16_t)ds_word(0x06EA)) *res = 1101 + i;
		return 1;
	case 1: *dat = "PRINCE.DAT"; *res = 3000 + n; return 1;                           /* DS:60E8 */
	case 2: *dat = "KID.DAT"; *res = 25001 + n - (i >= 0xDE ? 0x190 : 0); return 1;   /* 0993:0E70 (the ranges of 1286:06F0 agree) */
	case 3: {   /* 0993:0F36: the guard type's file (750 + n); the ranges on in 1286:09E4 are 851 + i */
		uint8_t t = render_guard_type != 0xFF ? render_guard_type : level.type;
		if (t > 9 || !guard_dat[t]) return 0;
		*dat = guard_dat[t]; *res = 750 + n;
		if (t != 5 && t != 6) {
			uint16_t lo = ds_word(0x06BC + 2 * t), hi = ds_word(0x06D2 + 2 * t);
			for (int k = 0; lo && ds_word((uint16_t)(lo + 2 * k)) != 0xFFFF; k++)
				if (i >= (int16_t)ds_word((uint16_t)(lo + 2 * k)) && i <= (int16_t)ds_word((uint16_t)(hi + 2 * k)) && guard_range_on(t, k)) { *res = 851 + i; break; }
		}
		return 1; }
	case 4:   /* 0993:0DC8: the level kind's file, the second bank (+200) at DS:05AC[kind] on */
		*dat = level_kind_dat(); if (!*dat) return 0;
		*res = 3500 + n + ((int16_t)ds_word(0x05AC + 2 * level_kind) <= i ? 200 : 0); return 1;
	}
	return 0;
}
int render_image_is_desc(int n) { for (int i = 0; i < n_extra; i++) if (extra[i].chtab == 4 && extra[i].id == n) return 1; return 0; }   /* (an id past the set's own: a description's image) */
const image_t *render_image(int chtab, int n)
{
	const char *dat; int res;
	if (!render_image_res(chtab, n, &dat, &res)) return NULL;
	const image_t *im = decode_res(dat, res);
	if (!im && chtab == 2) im = decode_res(level_kind_dat(), res);   /* (the resource search goes on through the scenery file: level 14's fireballs) */
	return im;
}

/* ---- table entries ---- */
static int sect_rect(int16_t *r, const int16_t *a, const int16_t *b)   /* 194C:5266 SectRect: r = a cut to b; nonzero if not empty */
{
	int16_t t = a[0] > b[0] ? a[0] : b[0], l = a[1] > b[1] ? a[1] : b[1], bo = a[2] < b[2] ? a[2] : b[2], ri = a[3] < b[3] ? a[3] : b[3];
	if (t >= bo || l >= ri) { r[0] = r[1] = r[2] = r[3] = 0; return 0; }
	r[0] = t; r[1] = l; r[2] = bo; r[3] = ri; return 1;
}
int render_sect_rect(int16_t *r, const int16_t *a, const int16_t *b) { return sect_rect(r, a, b); }
int render_set_entry(draw_entry *e, uint8_t chtab, int16_t id_override, const int16_t piece[3], uint8_t col, uint8_t row, uint8_t mode, uint8_t mirror)
{
	e->id = (uint16_t)piece[0]; e->x = piece[1]; e->y = piece[2];
	if (id_override != -2) e->id = (uint16_t)id_override;
	const image_t *im = render_image(chtab, e->id);   /* (chtab 4: 0993:0DC8 loads it first) */
	if (!im || !im->height) return 0;
	e->y = (int16_t)(e->y + (int16_t)ds_word(0x0D40 + 2 * (int8_t)row) - im->height);
	e->x = (int16_t)(e->x + (int16_t)ds_word(0x0D26 + 2 * (int8_t)col));
	int16_t box[4] = {e->y, e->x, (int16_t)(e->y + im->height), (int16_t)(e->x + im->width)}, r[4];   /* 194C:13B3 */
	if (!sect_rect(r, box, draw_clip)) return 0;
	e->chtab = chtab; e->col = (uint8_t)draw_col; e->row = (uint8_t)draw_row;   /* (DS:6B6F / 6B6E, not the arguments) */
	e->top = r[0]; e->left = r[1]; e->bottom = r[2]; e->right = r[3];
	e->mode = mode; e->mirror = mirror;
	return 1;
}

/* ---- the blitters ----
 * The port (DS:[2450] = DS:[5CC2] while the tables are drawn: the offscreen buffer, 320 x 192) clips to its clip
 * rectangle, set to the entry's rectangle by 194C:4C34 and put back (0, 0, 192, 320) by 4C9A. */
static int16_t clip[4] = {0, 0, 192, 320};   /* port +0x18: top, left, bottom, right */
/* the part of an image at (x, y) inside the clip: rows [*y0, *y1), columns [*x0, *x1) of the screen */
static int clip_box(const image_t *im, int x, int y, int *x0, int *x1, int *y0, int *y1)
{
	*x0 = x < clip[1] ? clip[1] : x; *x1 = x + im->width > clip[3] ? clip[3] : x + im->width;
	*y0 = y < clip[0] ? clip[0] : y; *y1 = y + im->height > clip[2] ? clip[2] : y + im->height;
	return *x1 > *x0 && *y1 > *y0;
}
static inline uint8_t px(const image_t *im, int xx, int yy, int mirror) { return im->pixels[yy * im->width + (mirror ? im->width - 1 - xx : xx)]; }
/* 2583:0006: an image of row runs (0..0x7F: n+1 bytes follow; 0x80..: the next byte n-0x7F times); mode 0 copies
 * everything (0x14B), the others leave the screen where a run repeats 0 (0x193: a literal 0 is drawn) */
/* 194C:122C (at load): a row-run image's nonzero pixels get the high nibble of the k-th bit set in the file's mask
 * word, k their own high nibble (the scenery files' 0xFFF0: + 0x40) */
static uint8_t mask_pixel(uint8_t p, uint16_t mask)
{
	if (!p) return 0;
	int k = p >> 4, n = 0;
	for (int b = 0; b < 16; b++) if (mask & (1u << b)) { if (n++ == k) return (uint8_t)((p & 0xF) | b << 4); }
	return p;   /* (past the bits set: the original reads its stack) */
}
static void blit_rows(const image_t *im, int x, int y, int mode, int mirror, uint16_t mask)
{
	int x0, x1, y0, y1; if (!clip_box(im, x, y, &x0, &x1, &y0, &y1)) return;
	for (int sy = y0; sy < y1; sy++) for (int sx = x0; sx < x1; sx++) {
		int xx = sx - x, yy = sy - y, k = yy * im->width + (mirror ? im->width - 1 - xx : xx);
		if (mode != 0 && im->clear && im->clear[k]) continue;
		PUT(sy * SCREEN_W + sx, mask_pixel(im->pixels[k], mask));
	}
}
/* 194C:04BA: a 4-bit image (two pixels a byte); 0 leaves the screen, the others are drawn with the high nibble
 * 16 * log2(mask) (mask 0x8000: 0xF0, 0x4000: 0xE0, the guards' slots 4 << n: 0x20 + 16 n) */
static void blit_nibbles(const image_t *im, int x, int y, uint16_t mask, int mirror)
{
	uint8_t hi = 0; for (uint16_t m = mask >> 1; m; m >>= 1) hi += 0x10;
	int x0, x1, y0, y1; if (!clip_box(im, x, y, &x0, &x1, &y0, &y1)) return;
	for (int sy = y0; sy < y1; sy++) for (int sx = x0; sx < x1; sx++) {
		uint8_t p = px(im, sx - x, sy - y, mirror) & 0xF;
		if (p) PUT(sy * SCREEN_W + sx, p | hi);
	}
}
/* 194C:6D42 (DS:[247C]): an 8-bit image; mode 0 copy, 1 and, 2 or, 3 xor, 4..7 the same with the source inverted,
 * 8 color 0 where the image is not 0, 10 copy leaving the screen where the image is 0 */
static void blit_bytes(const image_t *im, int x, int y, int mode, int mirror, uint8_t add)
{
	int x0, x1, y0, y1; if (!clip_box(im, x, y, &x0, &x1, &y0, &y1)) return;
	for (int sy = y0; sy < y1; sy++) for (int sx = x0; sx < x1; sx++) {
		uint8_t s = px(im, sx - x, sy - y, mirror), *d = &port_bits[sy * SCREEN_W + sx];
		if (render_owner) render_owner[sy * SCREEN_W + sx] = render_owner_id;
		if (add && s) s = (uint8_t)(s + add);
		if (mode == 8) { if (s) *d = 0; continue; }
		if (mode == 10) { if (s) *d = s; continue; }
		if (mode & 4) s = (uint8_t)~s;
		switch (mode & 3) { case 0: *d = s; break; case 1: *d &= s; break; case 2: *d |= s; break; case 3: *d ^= s; break; }
	}
}
uint8_t render_kid_colors = 0x10;   /* the colors KID.DAT's 4-bit images are converted with (the file's mask word, 0FB3:2C9C) */
/* KID.DAT's images are converted to 8-bit when loaded, with the file's mask of that moment (2, colors 0x10.., unless a
 * sprite's mask is set around its load by 0FB3:0EE0), then kept: an image's colors are those of its first load since
 * the level's image sets were made (1286:066A; images 0x83, 0x84, 0xD8..0xDA are loaded then, with mask 2). (The
 * game can also purge images when short of memory: not modelled.) */
static uint8_t kid_base[0x200];
void render_reset_images(void) { memset(kid_base, 0, sizeof kid_base); }
static uint8_t kid_colors(int n)
{
	int i = n - 1; if (i < 0 || i >= 0x200) return render_kid_colors;
	if (i == 0x83 || i == 0x84 || (i >= 0xD8 && i <= 0xDA)) return 0x10;
	if (!kid_base[i]) kid_base[i] = render_kid_colors;
	return kid_base[i];
}
static int kid_image_n;   /* the KID.DAT image being drawn (1-based) */
static uint16_t rows_mask = 0xFFF0;   /* the row-run image's file mask (the scenery files: 0xFFF0; 0FB3:21AC's pieces: 0xFFFE) */
/* 194C:6B06 (DS:[2478], 0FB3:127A for the 1-bit images, loaded as a byte a pixel: entry +2 set): the pixels not 0
 * in the color `color` (the entry's mode) */
static void blit_color(const image_t *im, int x, int y, uint8_t color, int mirror)
{
	int x0, x1, y0, y1; if (!clip_box(im, x, y, &x0, &x1, &y0, &y1)) return;
	for (int sy = y0; sy < y1; sy++) for (int sx = x0; sx < x1; sx++) if (px(im, sx - x, sy - y, mirror)) PUT(sy * SCREEN_W + sx, color);
}
/* 0FB3:11AE: draw an image of `type` (1: 8-bit, 2: row runs, 3: 4-bit with `mask`) at (x, y) in `mode` */
static void draw_image(const image_t *im, int type, int x, int y, int mode, uint16_t mask, int mirror)
{
	if (!im) return;
	uint8_t add = 0;
	if (im->depth == 4 && type == 1) add = kid_image_n ? kid_colors(kid_image_n) : render_kid_colors;
	if (im->depth == 1) blit_color(im, x, y, (uint8_t)mode, mirror);   /* 0FB3:127A */
	else
	switch (mode) {
	case 0: case 10:   /* 0FB3:122A */
		if (type == 2) blit_rows(im, x, y, mode, mirror, rows_mask);
		else if (type == 3) blit_nibbles(im, x, y, mask, mirror);
		else blit_bytes(im, x, y, mode, mirror, add);
		break;
	case 2: case 3: case 8: blit_bytes(im, x, y, mode, mirror, add); break;   /* 0FB3:125E */
	default: break;   /* (1, 4..7, 9, > 10: nothing) */
	}
	if (!redraw_all_flag) {   /* 0FB3:12AA: the image's box cut to the port's clip (the entry's) is a dirty rectangle */
		int16_t r[4] = {(int16_t)y, (int16_t)x, (int16_t)(y + im->height), (int16_t)(x + im->width)};
		if (sect_rect(r, r, clip)) render_add_dirty(r);
	}
}
/* 26BC:0592 (to the screen port 5D0A: the status line): image n of set `chtab` at (x, y) in `mode`, as `type` (1:
 * 8-bit, 2: rows, 3: 4-bit with the mask 0x2000) */
void render_image_to_screen(int chtab, int n, int x, int y, int mode, int type)
{
	const image_t *im = render_image(chtab, n); if (!im) return;
	uint8_t *sp = port_bits; int16_t sc[4]; memcpy(sc, clip, sizeof sc); uint16_t sf = redraw_all_flag;
	port_bits = screen_buf; clip[0] = 0; clip[1] = 0; clip[2] = SCREEN_H; clip[3] = SCREEN_W; redraw_all_flag = 1;   /* (no dirty rectangle: 26BC:0592 draws directly) */
	if (im->depth == 1) blit_color(im, x, y, (uint8_t)mode, 0);
	else if (type == 2) blit_rows(im, x, y, mode, 0, 0xFFF0);
	else if (type == 3) blit_nibbles(im, x, y, 0x2000, 0);
	else blit_bytes(im, x, y, mode, 0, im->depth == 4 ? (chtab == 2 ? kid_colors(n) : render_kid_colors) : 0);
	port_bits = sp; memcpy(clip, sc, sizeof sc); redraw_all_flag = sf;
}
/* 194C:4D72 on the screen port: a rectangle in color 0 */
void render_erase_screen(const int16_t *r)
{
	for (int y = r[0] < 0 ? 0 : r[0]; y < r[2] && y < SCREEN_H; y++)
		for (int x = r[1] < 0 ? 0 : r[1]; x < r[3] && x < SCREEN_W; x++) screen_buf[y * SCREEN_W + x] = 0;
}
/* 0FB3:143E: add a dirty rectangle (merged into the first one it meets, 194C:647E union; at most 40) */
void render_add_dirty(const int16_t *r)
{
	for (int i = 0; i < dirty_count; i++) {
		int16_t g[4] = {(int16_t)(r[0] - 1), (int16_t)(r[1] - 1), (int16_t)(r[2] + 1), (int16_t)(r[3] + 1)}, t[4];   /* 194C:500C InsetRect -1 */
		if (sect_rect(t, g, dirty_rects[i])) {
			int16_t *d = dirty_rects[i];
			if (r[0] < d[0]) d[0] = r[0];
			if (r[1] < d[1]) d[1] = r[1];
			if (r[2] > d[2]) d[2] = r[2];
			if (r[3] > d[3]) d[3] = r[3];
			return;
		}
	}
	if (dirty_count >= DIRTY_MAX) return;   /* (2768:002E: an error message) */
	memcpy(dirty_rects[dirty_count++], r, 8);
}

/* ---- the saved backgrounds (DS:5FEC count, 6-byte slots at DS:5FEE: bitmap, id, kind, restore flag) ----
 * 0993:04F0 saves the screen under a sprite before it is drawn (194C:5194: a bitmap of the rectangle); 0993:0684
 * (0FB3:13C2, before the tables) puts back the saved screens the next frame, last first. */
saved_bg saved_bgs[SAVED_MAX]; uint16_t saved_count;
static void port_copy(uint8_t *dst, const int16_t *r, int to_screen, uint8_t *bits, const int16_t *bounds)
{
	int w = bounds[3] - bounds[1];
	for (int y = r[0]; y < r[2]; y++) for (int x = r[1]; x < r[3]; x++) {
		if (y < 0 || y >= 192 || x < 0 || x >= 320) continue;   /* (CopyBits clips to the port) */
		if (y < bounds[0] || y >= bounds[2] || x < bounds[1] || x >= bounds[3]) continue;   /* (and to the saved port's bounds) */
		uint8_t *b = &bits[(y - bounds[0]) * w + (x - bounds[1])];
		if (to_screen) PUT(y * SCREEN_W + x, *b); else *b = dst[y * SCREEN_W + x];
	}
}
/* 0993:04F0 (ax left, dx right, bx top, stack: height, id, kind) */
void render_save_under(int16_t left, int16_t right, int16_t top, int16_t height, uint8_t id, uint8_t kind)
{
	if (saved_count >= SAVED_MAX) return;
	int16_t r[4] = {top, left, (int16_t)(top + height), right};
	if (r[2] <= r[0] || r[3] <= r[1]) return;   /* 194C:4D50 EmptyRect */
	saved_bg *b = &saved_bgs[saved_count];
	memcpy(b->rect, r, sizeof r); memcpy(b->bounds, r, sizeof r);
	b->bits = realloc(b->bits, (size_t)(r[2] - r[0]) * (r[3] - r[1]));
	port_copy(offscreen, r, 0, b->bits, b->bounds);   /* 194C:5194 */
	b->id = id; b->kind = kind; b->flag = (kind == 2 || kind == 3);
	saved_count++;
}
/* 0993:0684: put back the saved screens not kept (flag 0), last first; kind 2 is put back but kept; then the kept
 * ones below id 0x64 (characters) are put back next time */
void render_restore_saved(void)
{
	uint16_t keep[SAVED_MAX];
	for (int i = saved_count - 1; i >= 0; i--) {
		saved_bg *b = &saved_bgs[i];
		keep[i] = b->flag;
		if (b->flag) continue;
		if (!redraw_all_flag) { int16_t r[4]; memcpy(r, b->rect, sizeof r); render_add_dirty(r); }   /* 0FB3:143E */
		port_copy(offscreen, b->rect, 1, b->bits, b->bounds);   /* 2699:0184 / 194C:5164: CopyBits over its rect +0x10 */
		if (b->kind == 2) { b->flag = 1; keep[i] = 1; }
	}
	int n = 0;
	for (int i = 0; i < saved_count; i++) {
		if (!keep[i]) continue;
		if (n != i) { saved_bg t = saved_bgs[n]; saved_bgs[n] = saved_bgs[i]; saved_bgs[i] = t; }
		if (saved_bgs[n].id < 0x64) saved_bgs[n].flag = 0;
		n++;
	}
	saved_count = (uint16_t)n;
}
/* 0993:075E (0FB3:0002, a whole redraw): the saved screens are dropped */
void render_free_saved(void) { saved_count = 0; }

/* 0FB3:0BFC: draw an entry of table 0 (back) or 1 (fore) */
void render_draw_entry(const draw_entry *e)
{
	int type = 2; uint16_t mask = 0;
	if (e->chtab == 1) { type = 3; mask = 0x4000; } else if (e->chtab == 0) { type = 3; mask = 0x8000; }
	const image_t *im;
	rows_mask = 0xFFF0;
	if (e->chtab == 4 && render_desc_loaded() && render_image_is_desc(e->id)) render_desc_entry_saved(e->id, &e->top);   /* 0FB3:0CBA */
	if (e->chtab == 4 && e->id >= 0x62D7 && e->id <= 0x62E2) render_lever5_entry();   /* 0FB3:0D28 -> 37F0:01E6 */
	if (e->id >= 0x6370 && e->id <= 0x6372) { im = decode_res("PRINCE.DAT", e->id); rows_mask = 0xFFFE; }   /* 0FB3:21AC: SHAP 0x6370.. (PRINCE.DAT) converted with the mask 0xFFFE */
	else { im = render_image(e->chtab, e->id); if (e->chtab == 4 && image_mask(4, e->id)) rows_mask = image_mask(4, e->id); }
	if (!im) { rows_mask = 0xFFF0; return; }
	int16_t r[4]; if (!sect_rect(r, &e->top, (const int16_t[4]){0, 0, 192, 320})) { rows_mask = 0xFFF0; return; }   /* 194C:4C34 */
	memcpy(clip, r, sizeof clip);
	draw_image(im, type, e->x, e->y, e->mode, mask, e->mirror);
	rows_mask = 0xFFF0;
	clip[0] = 0; clip[1] = 0; clip[2] = 192; clip[3] = 320;   /* 194C:4C9A */
}
/* 0FB3:0EE0: draw an entry of table 3 (the characters' sprites: x is the right edge when mirrored) */
void render_draw_sprite(const sprite_entry *s)
{
	int type; uint16_t mask;
	if (s->chtab == 2) { type = 1; mask = 0; }
	else if (s->chtab == 4) { type = 2; mask = 0; }
	else { type = 3; mask = s->chtab == 0 ? 0x8000 : s->chtab == 1 ? 0x4000 : s->mask; }
	uint8_t save = render_kid_colors;
	if (s->chtab == 2 && s->mask) { uint8_t hi = 0; for (uint16_t m = s->mask >> 1; m; m >>= 1) hi += 0x10; render_kid_colors = hi; }   /* 0FB3:2C9C */
	int k = (int)(s - sprite_table); if (k >= 0 && k < SPRITE_MAX && s->chtab == 3) render_guard_type = sprite_guard[k];
	const image_t *im;
	if (s->chtab == 4 && s->id >= 0x62D7 && s->id <= 0x62E2) im = render_image(4, s->id);   /* 0FB3:0F04 -> 37F0:03AC(id - 0x62D7) */
	else if (s->chtab == 4 && level_number == 5 && drawn_room == 0xA && s->id < render_desc_count_raw()) im = render_image(4, render_desc_image_id(s->id));   /* 0FB3:0F2E -> 0CD6:01EC */
	else im = render_image(s->chtab, s->id + 1);
	render_guard_type = 0xFF;
	if (s->chtab == 4 && image_mask(4, s->chtab == 4 && s->id >= 0x62D7 && s->id <= 0x62E2 ? s->id : -1)) rows_mask = image_mask(4, s->id);
	kid_image_n = s->chtab == 2 ? s->id + 1 : 0;
	if (im) {
		int x = s->x; if (s->mirror) x -= im->width;
		int16_t right = (int16_t)(x + im->width), h = (int16_t)(s->rect[2] - s->y);
		if (right > s->rect[3]) right = s->rect[3];
		if (h > im->height) h = (int16_t)im->height;
		render_save_under((int16_t)x, right, s->y, h, s->layer, 0);   /* 0993:04F0 */
		int16_t r[4]; if (sect_rect(r, s->rect, (const int16_t[4]){0, 0, 192, 320})) {
			memcpy(clip, r, sizeof clip);
			draw_image(im, type, x, s->y, s->mode, mask, s->mirror);
			clip[0] = 0; clip[1] = 0; clip[2] = 192; clip[3] = 320;
		}
	}
	render_kid_colors = save; kid_image_n = 0; rows_mask = 0xFFF0;
}
void render_draw_table(int n)
{
	if (n == 3) { for (int i = 0; i < table_counts[3]; i++) render_draw_sprite(&sprite_table[i]); return; }
	draw_entry *t = n == 0 ? back_table : fore_table;
	int saved = !(redraw_all_flag && render_desc_loaded());
	for (int i = 0; i < table_counts[n]; i++) {
		if (!saved && back_table[i].piece != 0) { render_desc_save_under(); saved = 1; }   /* 0CD6:06B4 (the back table's piece byte, for table 1 too) */
		render_draw_entry(&t[i]);
	}
}
/* 0FB3:13C2: the tables drawn into the offscreen buffer: reordered, then back, sprites, fore */
void render_draw_tables(void)
{
	dirty_count = 0;
	render_sort_tables();
	render_restore_saved();   /* 0993:0684 */
	render_draw_table(0); render_draw_table(3); render_draw_table(1);
}

/* 0FB3:13C2's reorderings before the tables are drawn: 1ECE (desert-less kinds 2, 4, 5) puts the back entries with
 * piece byte 0 first, 1F68 (level 14 room 1, level 10 room 0x16 in kind 6) the foreground entries of image 0x6372;
 * both stable */
static void move_to_front(draw_entry *t, int n, int (*pick)(const draw_entry *))
{
	int dst = 0;
	for (int i = 0; i < n; i++) {
		if (!pick(&t[i])) continue;
		if (i != dst) { draw_entry e = t[i]; memmove(&t[dst + 1], &t[dst], (size_t)(i - dst) * sizeof e); t[dst] = e; }   /* (2812:1FE6) */
		dst++;
	}
}
static int piece0(const draw_entry *e) { return e->piece == 0; }
static int image6372(const draw_entry *e) { return e->id == 0x6372; }
void render_sort_tables(void)
{
	if (level_kind == 4 || level_kind == 2 || level_kind == 5) move_to_front(back_table, table_counts[0], piece0);   /* 0FB3:1ECE */
	if ((drawn_room == 1 && level_number == 14) || (drawn_room == 0x16 && level_number == 10)) {   /* 0FB3:1F68 */
		if (level_kind == 6) move_to_front(fore_table, table_counts[1], image6372);
	}
}
