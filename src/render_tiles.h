#pragma once
#include <stdint.h>
#include "render.h"

typedef struct tile_mod { uint8_t tile; uint32_t mod; } tile_mod;   /* a tile byte and its 4-byte modifier */
/* what 0FB3 passes a tile type's drawer (a pointer to 8 bytes on its stack) */
typedef struct __attribute__((packed)) tile_args { uint8_t layer; int8_t col, row; uint8_t tile; uint32_t mod; } tile_args;
typedef void (*tile_drawer)(tile_args *a);
typedef struct kind_drawers { tile_drawer by_tile[0x2C]; tile_drawer special; } kind_drawers;   /* DS:[0x6188] and DS:618A */

extern tile_mod left_col[3], right_col[3], row_above[11], row_below[11], cur_tile, left_tile, right_tile;
extern int8_t draw_row, draw_col;
extern uint16_t redraw_all_flag;
extern const kind_drawers *tile_drawers;
extern int16_t screen_rect[4];   /* DS:097E */

void get_room_address_draw(uint8_t room);
void load_side_columns(void);
void load_row_above(void);
void load_row_below(void);
void load_cur_tiles(void);
void tile_rect(int8_t row, int8_t col, int16_t *rect);   /* 17C1:0112 */
uint8_t tile_at_left(int8_t row, int8_t col);    /* 17C1:06BA */
uint8_t tile_at_right(int8_t row, int8_t col);   /* 17C1:0738 */
uint8_t tile_above(int8_t row, int8_t col);      /* 17C1:04F6 */
uint8_t tile_above_right(int8_t row, int8_t col);   /* 17C1:0584 */

/* the adders (0993:0124 / 019C / 0220 / 0330): a piece of tile type `tile` (its record in the piece table, DS:[0x1090],
 * 19 bytes: [0] a byte, then three pieces of image id, x, y) with the given image id */
int add_back_piece_a(uint8_t tile, int16_t id, int8_t col, int8_t row, uint8_t mode, uint8_t mirror);
int add_back_piece_b(uint8_t tile, int16_t id, int8_t col, int8_t row, uint8_t mode, uint8_t mirror);
int add_fore_piece_b(uint8_t tile, int16_t id, int8_t col, int8_t row, uint8_t mode, uint8_t mirror);
int add_fore_piece_c(uint8_t tile, int16_t id, int8_t col, int8_t row, uint8_t mode, uint8_t mirror);
extern uint8_t *piece_table;   /* DS:[0x1090] (a copy of the level's PIEC resource: some drawers rewrite piece positions) */

void draw_tile_0a(tile_args *a);   /* 0FB3:2394 */
void draw_room_tiles(void);   /* 0FB3:0122 */
void draw_one_tile(void);     /* 0FB3:01CA */
int description_row2_check(void);   /* 0CD6:0666 */
