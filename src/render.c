/* The drawing: draw tables, image sets and the blitters. Transcribed from 0993:0008 (table entries), 0FB3:0B78 /
 * 0BFC / 11AE (drawing the tables) and 2583:0006 (the row-run blitter); see render.h. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "globals.h"
#include "dat.h"
#include "render.h"
extern int8_t draw_row, draw_col;

uint8_t screen_buf[SCREEN_W * SCREEN_H];
draw_entry back_table[BACK_MAX], fore_table[FORE_MAX];
uint16_t table_counts[5];
int16_t draw_clip[4] = {0, 0, 192, 320};

/* ---- image sets and the image cache ---- */
static chtab_t chtabs[8];
#define CACHE_IDS 512
static image_t *cache[8][CACHE_IDS];
static dat_file dats[8]; static char dat_names[8][32];
void render_set_chtab(int n, const char *dat, uint16_t first, uint8_t pal_base)
{
	if (n < 0 || n >= 8) return;
	if (chtabs[n].dat != dat || chtabs[n].first != first) for (int i = 0; i < CACHE_IDS; i++) if (cache[n][i]) { image_free(cache[n][i]); free(cache[n][i]); cache[n][i] = NULL; }
	chtabs[n].dat = dat; chtabs[n].first = first; chtabs[n].pal_base = pal_base;
}
static dat_file *dat_of(const char *name)
{
	for (int i = 0; i < 8; i++) if (dat_names[i][0] && !strcmp(dat_names[i], name)) return &dats[i];
	for (int i = 0; i < 8; i++) if (!dat_names[i][0]) {
		extern char glue_dir[400]; const char *dir = glue_dir[0] ? glue_dir : getenv("PRINCE2_DIR");
		char p[512]; snprintf(p, sizeof p, "%s/%s", dir ? dir : ".", name);
		if (!dat_open(&dats[i], p)) return NULL;
		snprintf(dat_names[i], sizeof dat_names[i], "%s", name); return &dats[i];
	}
	return NULL;
}
const image_t *render_image(int n, int id)
{
	if (n < 0 || n >= 8 || !chtabs[n].dat || id < 0 || id >= CACHE_IDS) return NULL;
	if (cache[n][id]) return cache[n][id];
	dat_file *d = dat_of(chtabs[n].dat); if (!d) return NULL;
	int rid = chtabs[n].first + id;
	if (n == 4 && id - 1 >= (int16_t)ds_word(0x05AC + 2 * level_kind)) rid += 200;   /* (0993:0DC8: the level kind's later images, DS:05AC[kind] on, are 200 further) */
	uint16_t sz; const uint8_t *r = dat_find(d, "PAHS", (uint16_t)rid, &sz);
	image_t *im = calloc(1, sizeof *im);
	if (!r || !image_decode(r, sz, im)) { free(im); return NULL; }
	if (chtabs[n].pal_base) for (int i = 0; i < im->width * im->height; i++) if (im->pixels[i]) im->pixels[i] = (uint8_t)(im->pixels[i] + chtabs[n].pal_base);   /* (0 stays 0: transparent) */
	return cache[n][id] = im;
}

/* ---- table entries ---- */
static int sect_rect(int16_t *r, const int16_t *a, const int16_t *b)   /* 194C:5266 SectRect: r = a cut to b; nonzero if not empty */
{
	int16_t t = a[0] > b[0] ? a[0] : b[0], l = a[1] > b[1] ? a[1] : b[1], bo = a[2] < b[2] ? a[2] : b[2], ri = a[3] < b[3] ? a[3] : b[3];
	if (t >= bo || l >= ri) { r[0] = r[1] = r[2] = r[3] = 0; return 0; }
	r[0] = t; r[1] = l; r[2] = bo; r[3] = ri; return 1;
}
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

/* ---- drawing ---- */
/* 2583:0006 for row-run images, the same rules for the others: the image at (x, y) cut to the entry's box; mode 0
 * copies everything, the other modes leave the screen where the image is 0 */
void render_draw_entry(const draw_entry *e)
{
	const image_t *im = render_image(e->chtab, e->id); if (!im) return;
	if (e->mode != 0 && e->mode != 10 && e->mode != 2 && e->mode != 3 && e->mode != 8) return;   /* (0FB3:1214: the other modes draw nothing here) */
	for (int yy = 0; yy < im->height; yy++) {
		int sy = e->y + yy; if (sy < e->top || sy >= e->bottom || sy < 0 || sy >= SCREEN_H) continue;
		for (int xx = 0; xx < im->width; xx++) {
			int sx = e->x + xx; if (sx < e->left || sx >= e->right || sx < 0 || sx >= SCREEN_W) continue;
			int ix = e->mirror ? im->width - 1 - xx : xx, k = yy * im->width + ix;
			if (e->mode != 0 && im->pixels[k] == 0) continue;
			screen_buf[sy * SCREEN_W + sx] = im->pixels[k];
		}
	}
}
void render_draw_table(int n)
{
	draw_entry *t = n == 0 ? back_table : fore_table;
	for (int i = 0; i < table_counts[n]; i++) render_draw_entry(&t[i]);
}
