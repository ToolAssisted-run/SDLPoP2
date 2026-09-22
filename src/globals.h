/* Globals (DS 3B25) referenced so far. Names follow SDLPoP where the meaning matches. */
#pragma once
#include "types.h"
extern char_type Char;            /* DS:5AB6 */
extern char_type Opp;             /* DS:5AF6 */
extern char_type Char_saved;      /* DS:5B36 */
extern char_type chars[5];        /* DS:5B76 */
extern level_type level;          /* DS:2BB8 */
extern uint8_t   tiles0[30];      /* DS:2B9A dummy room 0 */
extern uint32_t  tick;            /* DS:5D04 */
extern int16_t   knock;           /* DS:613E */
extern int16_t   is_feather_fall; /* DS:5D36 */
extern int8_t    control_x, control_y, control_shift;  /* DS:5CD4..5CD6 */
extern uint8_t   drawn_room;      /* DS:5CDE */
extern uint16_t  counter_5cec, word_27c0; extern uint8_t flag_5cb9;
/* routines referenced by seq.c, to be reconstructed */
const uint16_t *get_seq_words(uint16_t seq_id);
int  get_seq_resource(uint16_t seq_id);
void seq_reload_current(void);
int  seq_condition(uint16_t cond);
void seq_jump_to(uint16_t seq_id);
void char_y_to_floor(void);       /* 0AFF:07B0: y = curr_row*63 + 56 (+37 for actions 7/8) */
void clear_char(void);            /* 0AFF:1BA2 */
void seq_sound(uint16_t n);       /* 0AFF:0716 */
void seq_ctl_1954(void);          /* 0AFF:1954 */
void seq_set_85f8(uint16_t v);    /* 0AFF:0794 */
void ovl_366c_1704(void);
void flash_on(uint16_t v); void flash_off(void);   /* 0FB3:2A34 / 294C */
int16_t char_dx_forward(int16_t dx);              /* 0AFF:0376 */
