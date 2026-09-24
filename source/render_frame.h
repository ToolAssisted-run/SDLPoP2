#pragma once
/* The frame's drawing: render_frame.c (the tile pass over the redraw requests, the objects' sprites) and
 * render_sprites.c (the characters and falling objects: the objects and the requests) */
#include <stdint.h>

/* a redraw request: a flag word and a rectangle (10 bytes) */
typedef struct redraw_rect { uint16_t set; int16_t rect[4]; } redraw_rect;
typedef struct redraw_t {
	redraw_rect above[10];      /* DS:61E4: the row above, by column */
	redraw_rect fore2[30];      /* DS:6248: the fore layer (1) of a tile over a box (1375:0F16) */
	uint8_t objs_at[30];        /* DS:6374: objects keyed to this tile */
	uint16_t fore_full[30];     /* DS:6392: the fore layer (2) of the tile again (1375:0E56) */
	redraw_rect fore_part[30];  /* DS:63CE: the fore layer (2) over a box (1375:0E12) */
	redraw_rect back[30];       /* DS:64FA: the back layers over a box (1375:0DC6) */
	uint16_t full[30];          /* DS:6626: the whole tile (1375:0E8C) */
} redraw_t;
_Static_assert(sizeof(redraw_t) == 0x6662 - 0x61E4, "redraw_t");
extern redraw_t redraw;

/* the frame's objects (DS:5D3A, 0x17 bytes, at most 30; count DS:60F8) */
typedef struct __attribute__((packed)) frame_obj {
	int16_t x, y;       /* +00 left, bottom */
	uint8_t chtab;      /* +04 */
	uint16_t id;        /* +05 image (0-based) */
	uint16_t frame;     /* +07 the character's frame */
	uint8_t dir;        /* +09 0: mirrored */
	uint8_t type;       /* +0A 0 the prince, 1..: characters, 0x80..: falling objects by kind */
	int16_t rect[4];    /* +0B clip */
	uint8_t key;        /* +13 the tile it is drawn with (0x1E first, 0xFF last, 0xFE drawn) */
	uint16_t mask;      /* +14 palette mask */
	uint8_t charidx;    /* +16 */
} frame_obj;
_Static_assert(sizeof(frame_obj) == 0x17, "frame_obj");
#define OBJ_MAX 30
extern frame_obj objs[OBJ_MAX]; extern uint16_t obj_count;
extern uint8_t obj_list[OBJ_MAX]; extern uint16_t obj_list_n;

/* the drawing variables DS:60FA..610D */
typedef struct spr_vars { uint8_t key, dir; int16_t x0; int16_t rect[4]; uint16_t mask; uint8_t charidx; } spr_vars;   /* 60FA, 60FB, 61E2, 6103, 610B, 610D (60FC..6102: obj_x, obj_y, obj_id, obj_chtab) */
extern spr_vars sv;

int8_t tile_index_of(int8_t row, int8_t col);   /* 0AFF:0220 */
void mark_fore(int8_t t, const int16_t *r);      /* 1375:0F16 */
void mark_back(int8_t t, const int16_t *r);      /* 1375:0DC6 */
void mark_fore_part(int8_t t, const int16_t *r); /* 1375:0E12 */
void mark_fore_full(int8_t t);                   /* 1375:0E56 */
void mark_tile(int8_t t);                        /* 1375:0E8C */
void mark_tiles_under(void (*mark)(int8_t, const int16_t *), const int16_t *r, uint8_t id);   /* 1375:0F5A */
int add_sprite(uint8_t chtab, uint16_t id1, int16_t x, uint8_t mode, int16_t y);   /* 0993:03CC */
void draw_objs_at(uint8_t key);    /* 0FB3:18DE */
void redraw_requested(void);       /* 0FB3:1308 */
int render_rect_at_tile(int row, int col, const int16_t *src, int16_t *dst);   /* 17C1:016E */
void render_frame_tables(void);   /* 0FB3:12F4 */
void render_frame_objects(void);  /* 0FB3:12F4 up to 1308 */
void render_status_hp(void);      /* 0823:0F38 */
void render_mob_mark(int how);     /* 1375:2296 */
void render_mob_obj(int16_t y);    /* 1375:22DC */
void render_frame(void);          /* 169B:0A8A: a frame's drawing */
void render_redraw_all(void);     /* 169B:0430: the whole room */
void render_redraw_room(void);    /* 169B:0430 up to the frame over it */
void render_erase_rect(const int16_t *r);   /* 194C:4D72 */
void render_copy_rect(const int16_t *rect);   /* 0823:1326 */
void render_present(void);       /* 0FB3:218A (with the upside-down flips) */
void render_present_all(void);   /* 169B:04CC .. 04F6 */
void render_kid_hp(int hp, int max);   /* 0FB3:24EA */
void render_opp_hp(uint8_t index, int hp, int max);   /* 0FB3:25D4 */
void render_hp_bars(void);        /* 0FB3:259C */
/* the palette (render_palette.c) */
int render_pal_load(int sub, int count, int start, int res);   /* 0FB3:2B1C */
void render_pal_restore(void);    /* 0FB3:294C */
void render_pal_blackout(void);   /* 0FB3:29B8 */
void render_pal_flash(int n);     /* 0FB3:2A34 */
void render_pal_rotate(int start, int count);   /* 2699:0048 */
void render_desert_gate_tick(int8_t tp);   /* 33FD:0576 (level 2, tile 4 at tick time) */
void render_desert_wave_tick(int8_t tp);   /* 33FD:067C / 0708 (tiles 0x1C / 0x1D) */
void render_desert_tile1e_tick(void);     /* 33FD:08BE (tile 0x1E moving) */
void render_desert_press(int col);        /* 33FD:0904 (a puzzle tile pressed) */
int render_trob_rect(uint16_t tmpl, int16_t *r);   /* 1375:0454: the rect DS:tmpl at the animated tile (DS:6672) */
void render_trob_request(int which, uint16_t arg);   /* 1375:01F4..0416 (which: the routine's offset): a tile animation's redraw request */
void render_lever5_images(void); void render_lever5_entry(void); void render_lever5_mouth_tick(void); void render_lever5_trap_tick(void);   /* render_ovl37f0.c */
void render_bridge_tick(int8_t tp, uint8_t v); void render_roof25_tick(int8_t tp); void render_roof26_tick(uint16_t m); void render_roof27_tick(uint16_t m); void render_lever5_set_saved(int v);
void render_pal_guards(void);     /* 2D3E:0F50 */
void render_pal_level_start(void);
void render_room_enter_palette(int bg);   /* the palette parts of the description rooms' hooks (entry 0, room loaded) */
void render_room_leave_palette(int bg);   /* (entry 1, room left) */
extern int render_track_tiles;   /* frontends: redraw the drawn room's changed tiles (render_frame.c) */
void render_level_loaded(void);
void render_check_level(void);       /* render_level_loaded() unless done for this level number */      /* 1286:01F2 (a full level load): image colors forgotten, the level's palettes */
void render_mob_rect(int16_t *r);  /* 1375:219E */
int16_t render_mob_box(uint16_t a);             /* DS:0810 / 082A (with the drawing's rewrites) */
void render_mob_box_set(uint16_t a, int16_t v);
void render_mob_box_reset(void);
void render_room_switch(const int16_t *old_box);   /* 0FB3:29B8 */
void render_msg_erase(void);   /* 0FB3:2104 */
void render_msg_clear(void);   /* 0FB3:2136 */
int render_pal_saved(void);
