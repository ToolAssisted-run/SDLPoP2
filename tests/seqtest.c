/* Equivalence test for play_seq: replay captured (Char before, Char after) pairs from the DOS game. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/types.h"
#include "../src/globals.h"
#include "../src/dat.h"

char_type Char, Opp, Kid, chars[5]; level_type level; uint8_t tiles0[30]; uint32_t tick; int16_t knock, is_feather_fall;
int8_t control_x, control_y, control_shift; uint8_t drawn_room; uint16_t counter_5cec, word_27c0, counter_27d6, word_6140; uint8_t flag_5cb9, byte_5cb8, lvl_43fd;
static dat_file seqdat; static char log_[512];
static void note(const char *s) { strncat(log_, s, sizeof log_ - strlen(log_) - 1); }
const uint16_t *get_seq_words(uint16_t id) { uint16_t n; const uint8_t *b = dat_find(&seqdat, "SQES", id, &n); return (const uint16_t *)b; }
int get_seq_resource(uint16_t id) { return dat_find(&seqdat, "SQES", id, NULL) != NULL; }
void seq_reload_current(void) { note(" reload"); }
int seq_condition(uint16_t c) { char t[32]; snprintf(t, sizeof t, " COND(%u)?", c); note(t); return 0; }
void seq_jump_to(uint16_t id) { Char.seq_id = id; Char.seq_pos = 0; }
void clear_char(void) { note(" clear_char"); }
void seq_ctl_1954(void) { note(" ctl1954"); }
void ovl_366c_1704(void) { note(" ovl1704"); }
void flash_on(uint16_t v) { (void)v; note(" flash_on"); } void flash_off(void) { note(" flash_off"); }
void play_sound(uint16_t n) { (void)n; } void sound_1611_01a8(uint16_t n) { (void)n; } int ovl_366c_11f8(uint8_t r) { (void)r; return 0; }

static int cmpfield(const char *name, long a, long b, char *out) { if (a != b) { sprintf(out + strlen(out), " %s:%ld!=%ld", name, a, b); return 1; } return 0; }
int main(int argc, char **argv)
{
	if (argc < 3) { fprintf(stderr, "usage: seqtest SEQUENCE.DAT pairs.bin [level_number]\n"); return 2; }
	if (!dat_open(&seqdat, argv[1])) return 2;
	level.number = argc > 3 ? atoi(argv[3]) : 1;
	FILE *f = fopen(argv[2], "rb"); if (!f) return 2;
	unsigned char in[64], out[64]; int n = 0, bad = 0;
	while (fread(in, 1, 64, f) == 64 && fread(out, 1, 64, f) == 64) {
		n++; log_[0] = 0;
		memcpy(&Char, in, 64); char_type exp; memcpy(&exp, out, 64);
		play_seq();
		char diff[512] = ""; int d = 0;
		d += cmpfield("dir", Char.direction, exp.direction, diff); d += cmpfield("x", Char.x, exp.x, diff); d += cmpfield("y", Char.y, exp.y, diff);
		d += cmpfield("frame", Char.frame, exp.frame, diff); d += cmpfield("row", Char.curr_row, exp.curr_row, diff); d += cmpfield("action", Char.action, exp.action, diff);
		d += cmpfield("fall_x", Char.fall_x, exp.fall_x, diff); d += cmpfield("fall_y", Char.fall_y, exp.fall_y, diff); d += cmpfield("room", Char.room, exp.room, diff);
		d += cmpfield("seq_pos", Char.seq_pos, exp.seq_pos, diff); d += cmpfield("seq_id", Char.seq_id, exp.seq_id, diff); d += cmpfield("f19", Char.f19, exp.f19, diff); d += cmpfield("f24", Char.f24, exp.f24, diff);
		if (d) { bad++; printf("case %d: seq %u pos %u ->%s   [%s]\n", n, ((char_type *)in)->seq_id, ((char_type *)in)->seq_pos, diff, log_); }
	}
	printf("%d cases, %d mismatches\n", n, bad);
	return bad != 0;
}
