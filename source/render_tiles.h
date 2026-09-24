#pragma once
#include <stdint.h>
#include "render.h"

typedef struct tile_mod { uint8_t tile; uint32_t mod; } tile_mod;   /* a tile byte and its 4-byte modifier */
/* what 0FB3 passes a tile type's drawer (a pointer to 8 bytes on its stack) */
typedef struct __attribute__((packed)) tile_args { uint8_t layer; int8_t col, row; uint8_t tile; uint32_t mod; } tile_args;
typedef void (*tile_drawer)(tile_args *a);
typedef struct kind_drawers { tile_drawer by_tile[0x2D]; tile_drawer special; } kind_drawers;   /* DS:[0x6188] and DS:618A */

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
void render_draw_layer(uint8_t layer);   /* 0FB3:03DC */
void render_draw_fore(void);             /* 0FB3:0858 */
int description_row2_check(void);   /* 0CD6:0666 */

extern const kind_drawers kind_caverns, kind_ruins, kind_temple;   /* render_kind3.c / render_kind4.c / render_kind2.c */
const kind_drawers *kind_drawers_for(int kind);
/* the drawers several kinds share (the 3443 overlay: tiles 0x0C and 0x0D of ruins and temple), render_kind_common.c */
void draw_3443_0050(tile_args *a);
void draw_3443_01d6(tile_args *a);

/* render_desc.c: the drawn room's description */
void render_desc_load(uint8_t room);   /* 0CD6:0398 */
int render_desc_loaded(void);
int render_desc_bg(void);
int16_t render_desc_word(int off);    /* a word of the description's header */
int render_desc_count_raw(void);      /* [0] */
int render_desc_image_id(int i);      /* object i's id in image set 4 */
void render_desc_save_under(void);    /* 0CD6:06B4 */
void render_desc_entry_saved(uint16_t id, const int16_t *rect);   /* 0FB3:0CBA */
void render_desc_set(const uint8_t *raw, int len);   /* (tests: the game's copy) */
void render_desc_after_redraw(void);  /* 0CD6:0792(0) */
int render_desc_restore_obj(uint8_t image);   /* 0CD6:0684 (the slot, -1 none) */
void desc_grab_rect(int16_t *r);   /* 33FD:030E (rooftops) */
void desc_obj_to_tile(uint8_t *o, int8_t col, int8_t row);   /* 0CD6:0108 */
void render_desc_objects(uint8_t layer);   /* 0FB3:0624 */
void render_lever5_tile12(tile_args *a); void render_lever5_tile1b(tile_args *a); void render_bridge_tile2c(tile_args *a);   /* render_ovl37f0.c (37F0:012A / 06EE / 0610) */
extern uint16_t word_2ba6;
void draw_object(int i, uint8_t layer);   /* 0FB3:0712 */
uint8_t *desc_obj(int i);
int desc_count(void);
void desc_obj_image_rect(uint8_t *o);
void desc_obj_offset(uint8_t *o, int16_t dx, int16_t dy);
void desc_draw_obj_at(tile_args *a, int i);   /* 0CD6:007A */
void desc_draw_obj_pair(tile_args *a, int i);
extern const kind_drawers kind_desert, kind_rooftops, kind_final;   /* render_kind_desc.c */
/* drawing parts of the level overlays (render_ovl37f0.c, render_hooks.c, render_kind2/4.c) and the sprite list */
void draw_hook_37f0_044a(void); void draw_hook_37f0_05fc(void); void draw_mob_37f0_0782(void);
void obj_hook_37f0_023a(uint8_t type); void obj_hook_37f0_08d2(uint8_t type);
void ruins_0ec0(tile_args *a); void temple_0cc0(tile_args *a);
const int16_t *sprite_rect_of(int chtab, uint8_t type);   /* render_frame.c (0FB3:1BF2) */
