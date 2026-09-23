#pragma once
/* The drawing: the offscreen buffer the game composes each frame in (conventional memory at phys 0x4CF22, copied to
 * VGA mode 13h), the draw tables (segment 0x39E0) and their images. */
#include <stdint.h>
#include "image.h"

#define SCREEN_W 320
#define SCREEN_H 200
extern uint8_t screen_buf[SCREEN_W * SCREEN_H];

/* a draw-table entry (20 bytes, the original's layout) */
typedef struct __attribute__((packed)) draw_entry {
	uint8_t  chtab;          /* +00 image set (DS:60E6 index: 0 PRINCE.DAT 1000, 1 3000, 2 KID, 3 guards, 4 the level's tiles) */
	uint8_t  piece;          /* +01 byte 0 of the tile type's piece record */
	uint16_t id;             /* +02 image id within the set */
	int16_t  x, y;           /* +04 top-left on the screen */
	uint8_t  col, row;       /* +08 the tile drawn (DS:6B6F / 6B6E) */
	int16_t  top, left, bottom, right;   /* +0A the clip: the image's box cut to the drawing area (DS:60DE) */
	uint8_t  mode;           /* +12 transfer mode (0 opaque, 0x0A zero fill runs transparent, ...) */
	uint8_t  mirror;         /* +13 drawn mirrored */
} draw_entry;
_Static_assert(sizeof(draw_entry) == 20, "draw_entry");

#define BACK_MAX 130
#define FORE_MAX 100
extern draw_entry back_table[BACK_MAX], fore_table[FORE_MAX];   /* 39E0:0000 / 39E0:0A28 */
extern uint16_t table_counts[5];                                /* DS:60F0 */
extern int16_t draw_clip[4];                                    /* DS:60DE top, left, bottom, right */

/* image sets (chtabs) */
typedef struct chtab_t { const char *dat; uint16_t first; uint8_t pal_base; } chtab_t;
void render_set_chtab(int n, const char *dat, uint16_t first, uint8_t pal_base);
const image_t *render_image(int chtab, int id);
void render_register_image(int chtab, int id, const char *dat, int res);   /* image `id` of set `chtab` is resource `res` of `dat` */   /* cached decode; NULL if missing */

/* 0993:0008 (register arguments al, dx, bx): fill *e from a piece (image id, x, y) at the current tile, cut to the
 * drawing area; 0 when nothing shows */
int render_set_entry(draw_entry *e, uint8_t chtab, int16_t id_override, const int16_t piece[3], uint8_t col, uint8_t row, uint8_t mode, uint8_t mirror);
/* 0FB3:0BFC / 11AE: draw an entry */
void render_draw_entry(const draw_entry *e);
/* 0FB3:0B78: draw table n (0 back, 1 fore) */
void render_draw_table(int n);
void render_sort_tables(void);   /* 0FB3:13C2: 1ECE / 1F68 */
