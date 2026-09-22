/* Shared test scaffolding: globals, resource access, and logging stubs for not-yet-reconstructed routines. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/types.h"
#include "../src/globals.h"
#include "../src/dat.h"
#include "stubs.h"
char_type Char, Opp, Kid, chars[5]; level_type level; uint8_t tiles0[30]; uint32_t tick; int16_t knock, is_feather_fall;
int8_t control_x, control_y, control_shift; uint8_t drawn_room; uint16_t counter_5cec, word_27c0, counter_27d6, word_6140;
uint8_t flag_5cb9, byte_5cb8, level_kind, level_number, room_A; uint8_t *level_roomlinks;
int8_t ctrl1_forward, ctrl1_backward, ctrl1_up, ctrl1_down, ctrl1_shift;
static dat_file seqdat; static char log_[512];
static void note(const char *s) { strncat(log_, s, sizeof log_ - strlen(log_) - 1); }
void stubs_init(const char *seqpath, const char *levelbin)
{
	if (!dat_open(&seqdat, seqpath)) { fprintf(stderr, "cannot open %s\n", seqpath); exit(2); }
	FILE *f = fopen(levelbin, "rb"); if (f) { if (fread(&level, 1, sizeof level, f) != sizeof level) fprintf(stderr, "short level\n"); fclose(f); }
	level_roomlinks = (uint8_t *)&level + 0x17BC; level_number = ((uint8_t *)&level)[0x1847]; level_kind = ((uint8_t *)&level)[0x1845];
}
void stubs_reset(void) { log_[0] = 0; }
const char *stubs_log(void) { return log_; }
const uint16_t *get_seq_words(uint16_t id) { uint16_t n; return (const uint16_t *)dat_find(&seqdat, "SQES", id, &n); }
int get_seq_resource(uint16_t id) { return dat_find(&seqdat, "SQES", id, NULL) != NULL; }
void seq_reload_current(void) { note(" reload"); }
int seq_condition(uint16_t c) { char t[32]; snprintf(t, sizeof t, " COND(%u)?", c); note(t); return 0; }
void seq_jump_to(uint16_t id) { Char.seq_id = id; Char.seq_pos = 0; }
void clear_char(void) { note(" clear_char"); }
void seq_ctl_1954(void) { note(" ctl1954"); }
void ovl_366c_1704(void) { note(" ovl1704"); }
void flash_on(uint16_t v) { (void)v; note(" flash_on"); } void flash_off(void) { note(" flash_off"); }
void play_sound(uint16_t n) { (void)n; } void sound_1611_01a8(uint16_t n) { (void)n; } int ovl_366c_11f8(uint8_t r) { (void)r; return 0; }
void shadow_hook_2f9a2(void) { note(" shadow"); }
void rtlink_fatal(int code) { char t[32]; snprintf(t, sizeof t, " FATAL(%x)", code); note(t); }
void load_fram_det_col(void) { note(" load_fram_det_col"); }
