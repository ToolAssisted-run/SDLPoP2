/* Globals (DS 3B25) referenced so far. Names follow SDLPoP where the meaning matches. */
#pragma once
#include "types.h"
extern char_type Char;            /* DS:5AB6 */
extern char_type Opp;             /* DS:5AF6 */
extern char_type Kid;             /* DS:5B36 - the prince's persistent record (play_kid_frame loads/saves it) */
extern char_type chars[5];        /* DS:5B76 */
extern level_type level;          /* DS:2BB8 */
extern uint8_t   tiles0[30];      /* DS:2B9A dummy room 0 */
extern uint32_t  tick;            /* DS:5D04 */
extern int16_t   knock;           /* DS:613E */
extern int16_t   is_feather_fall; /* DS:5D36 */
extern int8_t    control_x, control_y, control_shift;  /* DS:5CD4..5CD6 */
extern uint8_t   drawn_room;      /* DS:5CDE */
extern uint16_t  counter_5cec, word_27c0, counter_27d6, word_6140; extern uint8_t flag_5cb9, byte_5cb8, lvl_43fd;
void play_sound(uint16_t n); void sound_1611_01a8(uint16_t n); int ovl_366c_11f8(uint8_t room);
/* routines referenced by seq.c, to be reconstructed */
const uint16_t *get_seq_words(uint16_t seq_id);
int  get_seq_resource(uint16_t seq_id);
void seq_reload_current(void);
int  seq_condition(uint16_t cond);
void seq_jump_to(uint16_t seq_id);
void clear_char(void);            /* 0AFF:1BA2 */
void seq_ctl_1954(void);          /* 0AFF:1954 */
void ovl_366c_1704(void);
void flash_on(uint16_t v); void flash_off(void);   /* 0FB3:2A34 / 294C */
int16_t char_dx_forward(int16_t dx); void char_y_to_floor(void); void seq_sound(uint16_t n); void seq_set_85f8(uint16_t v);
void play_seq(void);              /* 0AFF:03AA */
void fall_accel(void); void fall_speed(void); void save_char(void); void load_char(int n); void loadkid(void);
void load_char_and_opp(int n); int char_out_of_level(void); void load_fram_det_col(void); void rtlink_fatal(int code);
/* tiles.c */
extern uint8_t curr_tile; extern uint16_t curr_modifier; extern uint8_t curr_tilepos, curr_room; extern int8_t tile_col, tile_row;
static const int8_t dir_front[] = {-1, 1};  /* DS:0CFB indexed by direction+1 (-1 left, 0 right) */
#define level_links(room) (level_roomlinks + (room) * 4)   /* DS:4374 */
extern uint8_t *level_roomlinks;
uint8_t level_edge_tile(int8_t row, int8_t col);   /* 0AFF:0174 */
uint8_t get_tile(int8_t row, int8_t col, uint8_t room); uint8_t get_tile_at_char(void); uint8_t get_tile_above_char(void);
uint8_t get_tile_infrontof_char(void); uint8_t get_tile_n_ahead(int8_t n); void get_room_address(uint8_t room); uint8_t find_room_of_tile(void);
int tile_is_empty_kind(uint8_t t); int tile_is_wall_kind(uint8_t t); int tile_is_floor(uint8_t t); int tile_is_loose_kind(uint8_t t); int tile_is_solid_floor(uint8_t t);
void seqtbl_offset_char(uint16_t seq_id); void shadow_hook_2f9a2(void);
