#pragma once
/* The drawing: the offscreen buffer the game composes each frame in (conventional memory at phys 0x4CF22, 192 rows of
 * 320, copied to VGA mode 13h), the draw tables (segment 0x39E0) and their images. */
#include <stdint.h>
#include "image.h"

#define SCREEN_W 320
#define SCREEN_H 200
extern uint8_t screen_buf[SCREEN_W * SCREEN_H];   /* the screen (VGA mode 13h memory): rows 0..191 copied from the offscreen buffer, 192..199 the status line */
extern uint8_t offscreen[SCREEN_W * 192];          /* the offscreen buffer the game composes in (phys 0x4CF22) */
extern uint8_t render_palette[256 * 3];            /* the VGA DAC (6-bit components) */

/* a draw-table entry (20 bytes, the original's layout) */
typedef struct __attribute__((packed)) draw_entry {
	uint8_t  chtab;          /* +00 image set (DS:60E6 index: 0 PRINCE.DAT 1000, 1 3000, 2 KID, 3 guards, 4 the level's tiles) */
	uint8_t  piece;          /* +01 byte 0 of the tile type's piece record */
	uint16_t id;             /* +02 image id within the set (1-based) */
	int16_t  x, y;           /* +04 top-left on the screen */
	uint8_t  col, row;       /* +08 the tile drawn (DS:6B6F / 6B6E) */
	int16_t  top, left, bottom, right;   /* +0A the clip: the image's box cut to the drawing area (DS:60DE) */
	uint8_t  mode;           /* +12 transfer mode (0 opaque, 0x0A zero fill runs transparent, ...) */
	uint8_t  mirror;         /* +13 drawn mirrored */
} draw_entry;
_Static_assert(sizeof(draw_entry) == 20, "draw_entry");
/* an entry of table 3, the characters' sprites (39E0:11F8, 20 bytes) */
typedef struct __attribute__((packed)) sprite_entry {
	int16_t  x, y;           /* +00 left (right edge when mirrored), top */
	uint8_t  chtab;          /* +04 */
	uint16_t id;             /* +05 image id (0-based) */
	int16_t  rect[4];        /* +07 clip: top, left, bottom, right */
	uint8_t  mode;           /* +0F */
	uint16_t mask;           /* +10 palette mask (4-bit sets: 16 * log2(mask) is the high nibble) */
	uint8_t  mirror;         /* +12 */
	uint8_t  layer;          /* +13 */
} sprite_entry;
_Static_assert(sizeof(sprite_entry) == 20, "sprite_entry");

#define BACK_MAX 130
#define FORE_MAX 100
#define SPRITE_MAX 30
#define DIRTY_MAX 40
extern draw_entry back_table[BACK_MAX], fore_table[FORE_MAX];   /* 39E0:0000 / 39E0:0A28 */
extern sprite_entry sprite_table[SPRITE_MAX];                   /* 39E0:11F8 */
extern uint8_t sprite_guard[SPRITE_MAX];                         /* the guard type of a set-3 sprite's image (0xFF: the level's) */
extern uint16_t table_counts[5];                                /* DS:60F0: back, fore, -, sprites, - */
extern int16_t draw_clip[4];                                    /* DS:60DE top, left, bottom, right */
extern uint16_t dirty_count; extern int16_t dirty_rects[DIRTY_MAX][4];   /* DS:27D8 / DS:27FC */

/* image sets (chtabs) */
void render_set_chtab(int n, const char *dat, uint16_t first, uint8_t pal_base);   /* (kept for the tests: the sets follow the game state) */
const image_t *render_image(int chtab, int id);   /* image id (1-based) of a set; cached decode; NULL if missing */
int render_image_res(int chtab, int id, const char **dat, int *res);
void render_register_image(int chtab, int id, const char *dat, int res);   /* image `id` of set `chtab` is resource `res` of `dat` */
extern uint8_t render_guard_type;   /* 0xFF: the guard set is the level type's */

/* 0993:0008 (register arguments al, dx, bx): fill *e from a piece (image id, x, y) at the current tile, cut to the
 * drawing area; 0 when nothing shows */
int render_set_entry(draw_entry *e, uint8_t chtab, int16_t id_override, const int16_t piece[3], uint8_t col, uint8_t row, uint8_t mode, uint8_t mirror);
int render_sect_rect(int16_t *r, const int16_t *a, const int16_t *b);   /* 194C:5266 */
/* 0FB3:0BFC / 11AE: draw an entry */
void render_draw_entry(const draw_entry *e);
void render_draw_sprite(const sprite_entry *s);   /* 0FB3:0EE0 */
/* 0FB3:0B78: draw table n (0 back, 1 fore, 3 sprites) */
void render_draw_table(int n);
void render_sort_tables(void);   /* 0FB3:13C2: 1ECE / 1F68 */
void render_draw_tables(void);   /* 0FB3:13C2 */
void render_add_dirty(const int16_t *r);   /* 0FB3:143E */
extern int16_t *render_owner, render_owner_id;   /* (tests) when set, the entry id that last wrote each pixel */

/* the saved backgrounds (DS:5FEC / 5FEE) */
#define SAVED_MAX 40
typedef struct saved_bg { int16_t rect[4]; uint8_t id, kind; uint16_t flag; uint8_t *bits; } saved_bg;
extern saved_bg saved_bgs[SAVED_MAX]; extern uint16_t saved_count;
void render_save_under(int16_t left, int16_t right, int16_t top, int16_t height, uint8_t id, uint8_t kind);   /* 0993:04F0 */
void render_restore_saved(void);   /* 0993:0684 */
void render_free_saved(void);      /* 0993:075E */
void render_image_to_screen(int chtab, int n, int x, int y, int mode, int type);   /* 26BC:0592 on the screen port */
void render_erase_screen(const int16_t *r);   /* 194C:4D72 on the screen port */
void render_reset_images(void);
void render_load_pieces(void);    /* the level kind's piece table (PIEC 3500), as a full level load reads it */
void render_check_pieces(void);   /* (loaded when missing or of another level kind) */   /* the image sets made again (a level start, 1286:066A): KID.DAT's images' colors forgotten */
