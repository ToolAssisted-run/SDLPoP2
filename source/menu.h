#pragma once
/* The screens outside the game (segment 0D5E): saved games (PRINCE.SAV), the options menu (PRINCE.OPT), the hall of
 * fame (PRINCE.HOF), the copy protection question, the title credits, and the line editor (25BF) they use.
 * See docs/SHELL.md. */
#include <stdint.h>

extern uint16_t word_1392;        /* DS:1392: restore a saved game (Alt-L asked during a demo or a scene) */
extern uint8_t sound_volume;      /* DS:2086: 0xF sound on, 0 off (194C:3380) */

int  menu_allowed(void);          /* 0D5E:0516: the clock still runs and no falling objects */
int  menu_can_save(void);         /* 0D5E:0CD4: a level (<= 14), the prince alive with hit points */
int  game_restore(void);          /* 0D5E:0544: the "Resume Saved Game" screen; > 0 a game was loaded, -1 cancelled */
int  game_save(void);             /* 0D5E:060A: the "Saved Games" screen; 1 saved, 0 failed, -1 cancelled */
void options_menu(void);          /* 0D5E:0696 */
void options_load(void);          /* 0D5E:2166: PRINCE.OPT at start */
void options_save(void);          /* 0D5E:20F4 */
int  sound_toggle(void);          /* 0823:0AF0: -> 1 now on */
int  hall_of_fame(int interactive);   /* 0D5E:1BEA: show the list (Alt-H, or the title loop); a key -> nonzero */
void hall_of_fame_enter(int minutes);  /* 0D5E:1684: the game was won: a new entry, its name typed in */
void copy_protection(void);       /* 0D5E:1288 (the story "scene" 0x64): three tries, then the program quits */
int  title_credits(void);         /* 0D5E:225C: the five credit pages; a key -> nonzero */

/* PRINCE.SAV: a 0xFC-byte header (a word, then ten 25-byte names) and ten 0x2597-byte slots (the checkpoint block
 * DS:5AB2 with the clock and a few variables, see docs/SHELL.md) */
enum { SAV_HEADER = 0xFC, SAV_NAME = 0x19, SAV_SLOT = 0x2597 };
int  save_slot_write(int slot, const uint8_t header[SAV_HEADER]);   /* 0D5E:0E5A */
int  save_slot_read(int slot);    /* 0D5E:0CF2: load it into the game state */
int  save_header_read(uint8_t header[SAV_HEADER]);                  /* 0D5E:0C0C (0 when the file is not valid) */

/* PRINCE.HOF: a word count (<= 5), then 29-byte entries (27-byte name, word minutes left), best first */
typedef struct hof_list { int16_t n; struct { char name[27]; int16_t minutes; } e[6]; } hof_list;
int  hof_read(hof_list *h);       /* in 0D5E:1B1C */
int  hof_insert(hof_list *h, int minutes);   /* 0D5E:19F4: -> the new entry's index */
int  hof_write(const hof_list *h);           /* 0D5E:1CC2 */
