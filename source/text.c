/* Text, fonts and the few graphics primitives of the 194C library that the shell's screens use (see text.h). The
 * library is platform code; what is kept here is what decides the pixels: clipping, the glyph and rectangle rules,
 * word wrapping and justification (194C:64FE / 537A), and the 0D5E / 0FB3 text styles. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "text.h"
#include "loader.h"
#include "render.h"
#include "image.h"
#include "glue.h"

extern uint16_t word_5cda, word_5cdc, word_5cd8, minutes_left, clock_ticks;   /* DS:5CDA / 5CDC / 5CD8 / 5CD2 / 5CEA */
extern uint16_t input_device;   /* DS:2BA2 */

const qrect rect_screen = {0, 0, 200, 320}, rect_game = {0, 0, 192, 320}, rect_status = {193, 98, 202, 235};
gport port_screen = { screen_buf, 0, 0, {0, 0, 200, 320}, 0, 0, 0, 0, 0xF, 0, 0 };
gport *the_port = &port_screen, *port_back;

gport *port_new(const qrect *r)
{
	gport *p = calloc(1, sizeof *p);
	p->bits = calloc(SCREEN_W, SCREEN_H); p->own = 1;
	p->origin_v = r->top; p->origin_h = r->left;   /* 194C:4FC2: the template's rectangles moved to r's origin */
	p->clip = (qrect){ r->top, r->left, r->bottom, r->right };
	p->fg = 0xF;   /* (the template DS:247E: bg 0, pen 0, mode 0, fg 0xF) */
	return p;
}
void port_free(gport *p) { if (!p) return; if (the_port == p) the_port = &port_screen; if (p->own) free(p->bits); free(p); }

qrect rect_inset(qrect r, int dv, int dh) { r.top += dv; r.left += dh; r.bottom -= dv; r.right -= dh; return r; }
qrect rect_offset(qrect r, int dv, int dh) { r.top += dv; r.left += dh; r.bottom += dv; r.right += dh; return r; }
static int sect(const qrect *a, const qrect *b, qrect *o)   /* 194C:5266 */
{
	o->top = a->top > b->top ? a->top : b->top; o->bottom = a->bottom < b->bottom ? a->bottom : b->bottom;
	o->left = a->left > b->left ? a->left : b->left; o->right = a->right < b->right ? a->right : b->right;
	return o->top < o->bottom && o->left < o->right;
}
static inline uint8_t *pix(gport *p, int v, int h) { return p->bits + (v - p->origin_v) * SCREEN_W + (h - p->origin_h); }

/* 194C:6F00 (DS:[247A] = 194C:6C64 in VGA mode): mode 0 sets the colour */
void gfx_fill_rect(int color, const qrect *r)
{
	qrect c; if (!sect(r, &the_port->clip, &c)) return;
	for (int v = c.top; v < c.bottom; v++) memset(pix(the_port, v, c.left), color, c.right - c.left);
}
void gfx_erase_rect(const qrect *r) { gfx_fill_rect(the_port->bg, r); }
/* 194C:4D8E: top and bottom lines, then the left and right columns between them */
void gfx_frame_rect(const qrect *r)
{
	int c = the_port->fg; qrect d = *r;
	if (r->top + 1 > r->bottom) return;
	if (r->top + 1 != r->bottom) { d.bottom = r->top + 1; gfx_fill_rect(c, &d); }
	d.bottom = r->bottom; d.top = r->bottom - 1; gfx_fill_rect(c, &d);
	d.top = r->top + 1; d.bottom = r->bottom - 1;
	if (r->left + 1 > r->right) return;
	if (r->left + 1 != r->right) { d.left = r->left; d.right = r->left + 1; gfx_fill_rect(c, &d); }
	d.right = r->right; d.left = r->right - 1; gfx_fill_rect(c, &d);
}
void gfx_copy_bits(const gport *src, gport *dst, const qrect *sr, const qrect *dr)
{
	qrect c; if (!sect(dr, &dst->clip, &c)) return;
	for (int v = c.top; v < c.bottom; v++)
		memcpy(pix(dst, v, c.left), pix((gport *)src, v - dr->top + sr->top, c.left - dr->left + sr->left), c.right - c.left);
}
void gfx_move_to(int v, int h) { the_port->pen_v = v; the_port->pen_h = h; }
void gfx_move(int dv, int dh) { the_port->pen_v += dv; the_port->pen_h += dh; }

/* ---- fonts: [0] first char, [1] last, word 2 ascent, 4 descent, 6 leading, 8 extra advance, then per char a word
 * offset to {height, width, 0, rows of (width+7)/8 bytes, high bit first} ---- */
/* font 0 is the ports' default (the template DS:247E's handle): the status line's font, not a resource but part of
 * PRINCE.EXE's data (3891:0D20, file offset 0x3B220): characters 0x1B..0x7E, 7 pixels high, bold */
static const uint8_t *system_font(void)
{
	static uint8_t f[0x800]; static int loaded;
	if (!loaded) { loaded = -1; FILE *e = fopen(game_path("PRINCE.EXE"), "rb"); if (e) { if (!fseek(e, 0x3B220, SEEK_SET) && fread(f, 1, sizeof f, e) == sizeof f) loaded = 1; fclose(e); } }
	return loaded > 0 ? f : NULL;
}
/* fonts a frontend supplies (not the game's; e.g. the overlay menu's): gfx_add_font */
static const uint8_t *added_font[4]; static uint16_t added_id[4];
void gfx_add_font(uint16_t id, const uint8_t *data)
{
	for (int i = 0; i < 4; i++) if (!added_font[i] || added_id[i] == id) { added_font[i] = data; added_id[i] = id; return; }
}
static const uint8_t *font_data(uint16_t id)
{
	for (int i = 0; i < 4 && added_font[i]; i++) if (added_id[i] == id) return added_font[i];
	uint16_t n; if (!id) return system_font();
	const uint8_t *f = res_get("FONT", id, &n);
	if (!f && res_open("PRINCE.DAT")) f = res_get("FONT", id, &n);   /* (without the shell nothing has opened it) */
	return f;
}
static int16_t rd16s(const uint8_t *p) { return (int16_t)(p[0] | p[1] << 8); }
void gfx_text_font(uint16_t id) { the_port->font = id; }
static const uint8_t *glyph(const uint8_t *f, uint8_t c)
{
	if (!f || c > f[1] || c < f[0]) return NULL;
	return f + (uint16_t)rd16s(f + 10 + (c - f[0]) * 2);
}
int gfx_text_width(const char *s, int n)
{
	const uint8_t *f = font_data(the_port->font), *g; int w = 0;
	for (int i = 0; i < n; i++) if ((g = glyph(f, (uint8_t)s[i]))) w += rd16s(g + 2) + rd16s(f + 8);
	return w;
}
/* DS:[2478] = 194C:6B06: the set bits in the colour, clipped */
static void draw_glyph(const uint8_t *g, int top, int left, int color)
{
	int h = rd16s(g), w = rd16s(g + 2), bpr = (w + 7) / 8; const uint8_t *rows = g + 6;
	for (int y = 0; y < h; y++) {
		int v = top + y; if (v < the_port->clip.top || v >= the_port->clip.bottom) continue;
		for (int x = 0; x < w; x++) {
			int hh = left + x; if (hh < the_port->clip.left || hh >= the_port->clip.right) continue;
			if (rows[y * bpr + (x >> 3)] & (0x80 >> (x & 7))) *pix(the_port, v, hh) = (uint8_t)color;
		}
	}
}
int gfx_draw_text(const char *s, int n)
{
	const uint8_t *f = font_data(the_port->font), *g; int h0 = the_port->pen_h;
	if (!f) return 0;
	int top = the_port->pen_v - rd16s(f + 2);
	for (int i = 0; i < n; i++)
		if ((g = glyph(f, (uint8_t)s[i]))) { int x = the_port->pen_h; the_port->pen_h += rd16s(g + 2) + rd16s(f + 8); draw_glyph(g, top, x, the_port->fg); }
	return the_port->pen_h - h0;
}
void gfx_draw_string(const char *s) { gfx_draw_text(s, (int)strlen(s)); }
/* 194C:537A: how many characters of s fit in `width` (a break after '\r', '-', or at a space) */
static int line_break(int hjust, int width, int n, const char *s)
{
	int di = 0, brk = 0, w = 0;
	while (di != n) {
		w += gfx_text_width(s + di, 1);   /* 194C:4C20 */
		if (w > width) return brk ? brk : di;
		char c = s[di++];
		if (c == '\r') return di;
		if (c == '-') { brk = di; continue; }
		if (hjust > 0) { if (s[di] == ' ' && c != ' ') brk = di; }
		else if (c == ' ' || s[di] == ' ') brk = di;
	}
	return di;
}
void gfx_text_box(const char *s, int n, int vjust, int hjust, const qrect *r)
{
	int save_v = the_port->pen_v, save_h = the_port->pen_h;   /* 194C:4E70 / 5316 */
	int W = r->right - r->left, H = r->bottom - r->top, nl = 0;
	const char *line[64]; int len[64]; const char *p = s; int left = n;
	while (left > 0 && nl < 64) { int k = line_break(hjust, W, left, p); if (!k) break; line[nl] = p; len[nl] = k; nl++; p += k; left -= k; }
	const uint8_t *f = font_data(the_port->font); if (!f) return;
	int lh = rd16s(f + 2) + rd16s(f + 4) + rd16s(f + 6); unsigned total = (unsigned)(lh * nl - rd16s(f + 6));
	int v = r->top;
	if (vjust == 0) v = r->top + (H >> 1) + (H & 1) - (int)(total >> 1) - (int)(total & 1);
	else if (vjust > 0) v = r->top + H - (int)total;
	the_port->pen_v = v + rd16s(f + 2);
	for (int i = 0; i < nl; i++) {
		const char *q = line[i]; int m = len[i];
		if (hjust < 0 && q != s && q[0] == ' ' && q[-1] != '\r') {   /* a left-justified line does not start with its space */
			q++; m--;
			if (m && q[0] == ' ' && q[-2] == '.') { q++; m--; }
		}
		unsigned w = (unsigned)gfx_text_width(q, m); int h = r->left;
		if (hjust == 0) h = r->left + (W >> 1) - (int)(w >> 1);
		else if (hjust > 0) h = r->left + W - (int)w;
		the_port->pen_h = h; gfx_draw_text(q, m);
		the_port->pen_v += lh;
	}
	the_port->pen_v = save_v; the_port->pen_h = save_h;
}
void gfx_text_box_str(const char *s, int vjust, int hjust, const qrect *r) { gfx_text_box(s, (int)strlen(s), vjust, hjust, r); }

/* ---- shape sets and their images ---- */
static uint16_t set_mask[16], set_id[16];
static int set_slot(uint16_t id) { for (int i = 0; i < 16; i++) if (set_id[i] == id) return i; for (int i = 0; i < 16; i++) if (!set_id[i]) { set_id[i] = id; return i; } return 0; }
int gfx_shape_set(uint16_t id, uint16_t bank_mask, int set_palette)
{
	uint16_t n; const uint8_t *s = res_get("SHPL", id, &n); if (!s) return 0;
	/* (the menus load the set fresh each time and release it after: its colours are installed every time) */
	int k = set_slot(id);
	set_mask[k] = bank_mask ? bank_mask : 1;
	if (set_palette) { int bank = 0; for (uint16_t m = set_mask[k] >> 1; m; m >>= 1) bank++; pal_set(s + 7, bank * 16, 16); }   /* 194C:082A: 16 colours from +7 */
	return set_mask[k];
}
static int shape_image(uint16_t set, int index, image_t *im, uint8_t *add)
{
	uint16_t n; const uint8_t *s = res_get("SHPL", set, &n); if (!s) return 0;
	const uint8_t *r = res_get("SHAP", (uint16_t)(rd16s(s) + index - 1), &n); if (!r) return 0;
	int k = set_slot(set), bank = 0; for (uint16_t m = (set_mask[k] ? set_mask[k] : 1) >> 1; m; m >>= 1) bank++;
	*add = (uint8_t)(bank * 16);
	return image_decode(r, n, im);
}
void gfx_shape_size(uint16_t set, int index, int *height, int *width)
{
	image_t im; uint8_t add; *height = *width = 0;
	if (shape_image(set, index, &im, &add)) { *height = im.height; *width = im.width; image_free(&im); }
}
/* 2583:0006 on the 25A1:00FA unpacking: pixel 0 is transparent; mirror: 0823:1447 flips the rows */
void gfx_draw_shape(uint16_t set, int index, int v, int h, int mirror)
{
	image_t im; uint8_t add; if (!shape_image(set, index, &im, &add)) return;
	for (int y = 0; y < im.height; y++) {
		int vv = v + y; if (vv < the_port->clip.top || vv >= the_port->clip.bottom) continue;
		for (int x = 0; x < im.width; x++) {
			int hh = h + x; if (hh < the_port->clip.left || hh >= the_port->clip.right) continue;
			uint8_t p = im.pixels[y * im.width + (mirror ? im.width - 1 - x : x)];
			if (p) *pix(the_port, vv, hh) = (uint8_t)(p + add);
		}
	}
	image_free(&im);
}

void pal_get(uint8_t *dst, int first, int n) { memcpy(dst, render_palette + first * 3, n * 3); }
void pal_set(const uint8_t *src, int first, int n) { if (src) memcpy(render_palette + first * 3, src, n * 3); else memset(render_palette + first * 3, 0, n * 3); }

/* 0D5E:00B4: a string three times: font n in colour 0 (alt: 2) one pixel to the right, font n in 0xE (alt: 0xD), then
 * font n+1 in 0xF (alt: 4). In r when given (justified by just[0] / just[1] as 194C:64FE's vjust / hjust; just NULL:
 * centred vertically, left), else at the point pt (v, h) */
void text_shadowed(const int16_t *just, const qrect *r, const int16_t *pt, int alt, int font, const char *s)
{
	int c1 = alt ? 2 : 0, c2 = alt ? 0xD : 0xE, c3 = alt ? 4 : 0xF;
	int vj = just ? just[0] : 0, hj = just ? just[1] : -1;
	gfx_text_font((uint16_t)font); the_port->fg = c1;
	if (r) { qrect d = rect_offset(*r, 0, 1); gfx_text_box_str(s, vj, hj, &d); } else { gfx_move_to(pt[0], pt[1] + 1); gfx_draw_string(s); }
	the_port->fg = c2;
	if (r) gfx_text_box_str(s, vj, hj, r); else { gfx_move_to(pt[0], pt[1]); gfx_draw_string(s); }
	gfx_text_font((uint16_t)(font + 1)); the_port->fg = c3;
	if (r) gfx_text_box_str(s, vj, hj, r); else { gfx_move_to(pt[0], pt[1]); gfx_draw_string(s); }
}
/* 0D5E:01F0: the same with a TXT4 text */
void text_res_shadowed(const int16_t *just, const qrect *r, const int16_t *pt, int alt, int font, uint16_t txt)
{
	uint16_t n; const char *s = txt4_get(txt, &n);
	if (s) text_shadowed(just, r, pt, alt, font, s);
}

/* ---- the status line ---- */
static void on_screen(void (*f)(void *), void *a) { gport *save = the_port; the_port = &port_screen; f(a); the_port = save; }
static qrect status_line(void) { qrect r = rect_inset(rect_status, 0, -rect_status.left); r.right = 0x140; return r; }   /* (DS:0990 = the left edge) */
static void do_clear(void *a)
{
	qrect r = word_5cdc == 0x258 ? status_line() : rect_status;
	gfx_fill_rect(0, &r);
	if (*(int *)a) word_5cdc = word_5cda = 0;
}
void status_clear(int reset) { on_screen(do_clear, &reset); }
static void do_erase(void *a) { (void)a; qrect r = status_line(); gfx_erase_rect(&r); }
void status_erase_line(void) { on_screen(do_erase, NULL); }
static void do_message(void *a)
{
	char t[100]; snprintf(t, sizeof t, "%s", (const char *)a);
	for (char *p = t; *p; p++) *p = (char)toupper((unsigned char)*p);   /* 2812:1986 */
	int z = 1; do_clear(&z);
	gfx_text_box_str(t, 1, 0, &rect_status);
}
void status_message(const char *s) { on_screen(do_message, (void *)s); }
static void do_press(void *a)
{
	(void)a; if (word_5cd8) return;
	do_erase(NULL); qrect r = status_line();
	gfx_text_box_str(input_device == 2 ? "PRESS BUTTON TO CONTINUE" : "PRESS KEY TO CONTINUE", 1, 0, &r);
}
void status_press_key(void) { on_screen(do_press, NULL); }
/* 0823:0DF3..0E58 */
void time_message(void)
{
	char t[40];
	if ((int16_t)minutes_left <= 0) snprintf(t, sizeof t, "TIME HAS EXPIRED!");
	else if (minutes_left == 1) { unsigned s = (clock_ticks + 1u) / 12u; if (s == 1) snprintf(t, sizeof t, "1 SECOND LEFT"); else snprintf(t, sizeof t, "%u SECONDS LEFT", s); }
	else snprintf(t, sizeof t, "%d MINUTES LEFT", minutes_left);
	status_message(t);
}
