/* Tick-level equivalence test: replay the prince's whole per-tick update (169B:0692 play_kid_frame) against captures
 * taken at its entry (Kid, Opp, chars, misc, collision state), inside control() (post-input controls) and at its exit. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/types.h"
#include "../src/globals.h"
#include "../src/dat.h"
#include "stubs.h"
extern int coll_debug;
static unsigned char ctl[8], c1[16];
/* 0AFF:10E8 + 11F8 with the input path replaced by the captured post-input controls */
int play_kid_control(void)
{
	if (Char.alive < 0 && Char.f12 == 0) Char.alive = 0;
	if (word_8a84) word_8a84--;
	control_x = ctl[0]; control_y = ctl[1]; control_shift = ctl[2];
	ctrl1_forward = c1[0]; ctrl1_backward = c1[1]; ctrl1_up = c1[2]; ctrl1_down = c1[3]; ctrl1_shift = c1[4];
	control();
	if (Char.alive >= 0 && is_dead_frame(Char.frame) && Char.charid <= 1 && Char.alive < 8) { Char.alive++; if (Char.alive == 8) return -1; }   /* alive 7 -> 169B:123E (level restart), no frame this tick */
	return -2;
}
static void load_level_from_ram(const char *path)
{
	FILE *f = fopen(path, "rb"); if (!f) { fprintf(stderr, "cannot open %s\n", path); exit(2); }
	static unsigned char ram[655360]; if (fread(ram, 1, sizeof ram, f) != sizeof ram) { fprintf(stderr, "short RAM dump\n"); exit(2); } fclose(f);
	memcpy(&level, ram + 0x3DE08, sizeof level); memcpy(tiles0, ram + 0x3DDEA, 30);
	level_roomlinks = (uint8_t *)&level + 0x17BC; level_kind = ((uint8_t *)&level)[0x1845]; level_number = ((uint8_t *)&level)[0x1847];
}
static int diff64(const char *what, const void *got, const unsigned char *exp, int n) { int d = 0; for (int o = 0; o < n; o++) if (((const unsigned char *)got)[o] != exp[o]) { if (!d) printf("  %s:", what); d++; printf(" +%02X:%02X!=%02X", o, ((const unsigned char *)got)[o], exp[o]); } if (d) printf("\n"); return d; }
int main(int argc, char **argv)
{
	if (argc < 6) { fprintf(stderr, "usage: ticktest SEQUENCE.DAT ram.bin PRINCE.EXE ticks.bin level.bin\n"); return 2; }
	stubs_init(argv[1], argv[5]); stubs_load_frame_tables(argv[3]); load_level_from_ram(argv[2]);
	FILE *f = fopen(argv[4], "rb"); if (!f) return 2;
	unsigned char rec[860];   /* 4 + 12*64 + 8 + 16 + 32 + 32 (flags at entry, zero-filled for old captures) */ int n = 0, bad = 0, badchar = 0, deathtimed = 0;
	unsigned char prev_flags[32], prev_img[64]; int have_prev = 0;
	while (fread(rec, 1, sizeof rec, f) == sizeof rec) {
		unsigned frame = rec[0] | rec[1] << 8 | rec[2] << 16 | (unsigned)rec[3] << 24;
		unsigned char *p = rec + 4, *in = p, *misc = p + 64, *opp = p + 128, *chs = p + 192, *collin = p + 512, *out;
		memcpy(ctl, p + 576, 8); memcpy(c1, p + 584, 16); out = p + 600; unsigned char *coll_exp = p + 664, *flags_exp = p + 728, *img_exp = p + 760;
		n++; stubs_reset();
		memcpy(&Kid, in, 64); memcpy(&Opp, opp, 64); memcpy(chars, chs, 320);
		word_8604 = misc[0] | (misc[1] << 8); word_5cd8 = misc[0x14] | (misc[0x15] << 8); drawn_room = misc[0x1A];
		room_L = misc[0x1B]; room_R = misc[0x1C]; room_A = misc[0x1D]; room_B = misc[0x1E]; room_AL = misc[0x1F]; room_AR = misc[0x20]; room_BL = misc[0x21]; room_BR = misc[0x22];
		memcpy(&coll, collin, 64);
		unsigned char *flagsin = p + 824; int have_in = 0; for (int i = 0; i < 20; i++) have_in |= flagsin[i];
		if (have_in) { memcpy(prev_coll_flags, flagsin, 10); memcpy(curr_row_coll_flags, flagsin + 10, 10); } else if (have_prev) { memcpy(prev_coll_flags, prev_flags, 10); memcpy(curr_row_coll_flags, prev_flags + 10, 10); }
		if (have_prev) { word_6140 = prev_img[0x2E] | prev_img[0x2F] << 8; word_6142 = prev_img[0x30] | prev_img[0x31] << 8; word_8a84 = prev_img[0x32] | prev_img[0x33] << 8; word_6146 = prev_img[0x34] | prev_img[0x35] << 8; }
		coll_debug = getenv("CASE") && atoi(getenv("CASE")) == n; if (coll_debug) { debug_case_tiles(); debug_opp(); }
		play_kid_frame();
		char_type exp; memcpy(&exp, out, 64);
		int d = memcmp(&Char, &exp, 64) != 0;
		if (d && exp.frame == 0xB9 && exp.alive >= 0 && Char.alive >= 0) { char_type a = Char, b = exp; a.alive = b.alive = 0; if (!memcmp(&a, &b, 64)) { d = 0; deathtimed++; } }   /* the death counter waits for the death sound (sound timing, not modelled) */
		if (d) { badchar++; printf("case %d (frame %u): in frame %u seq %u x %d y %d row %d act %u | ctrl %d %d %d ctrl1 %d %d %d %d %d -> exp frame %u seq %u x %d y %d, got frame %u seq %u x %d y %d [%s]\n", n, frame, ((char_type *)in)->frame, ((char_type *)in)->seq_id, ((char_type *)in)->x, ((char_type *)in)->y, ((char_type *)in)->curr_row, ((char_type *)in)->action, (int8_t)ctl[0], (int8_t)ctl[1], (int8_t)ctl[2], (int8_t)c1[0], (int8_t)c1[1], (int8_t)c1[2], (int8_t)c1[3], (int8_t)c1[4], exp.frame, exp.seq_id, exp.x, exp.y, Char.frame, Char.seq_id, Char.x, Char.y, stubs_log()); diff64("Char", &Char, out, 64); }
		int nocoll = getenv("NOCOLL") != NULL || Kid.room == 0 || ((char_type *)in)->room == 0;   /* out of the level: the game leaves the guard's values behind */
		int dc = nocoll ? 0 : diff64("coll", &coll, coll_exp, 64);
		unsigned char flags[20]; memcpy(flags, prev_coll_flags, 10); memcpy(flags + 10, curr_row_coll_flags, 10);
		int df = nocoll ? 0 : diff64("flags", flags, flags_exp, 20);
		unsigned char img[64] = {0}; memcpy(img, &image_height, 2); memcpy(img + 2, &image_width, 2); memcpy(img + 4, &char_x_left, 2); memcpy(img + 6, &char_x_right, 2); memcpy(img + 8, &char_x_left_coll, 2); memcpy(img + 10, &char_x_right_coll, 2); memcpy(img + 12, &char_top_y, 2);
		img[0x23] = char_col_left; img[0x24] = char_col_right; img[0x25] = char_top_row; img[0x26] = char_bottom_row;
		int di = nocoll ? 0 : (memcmp(img, img_exp, 14) || memcmp(img + 0x23, img_exp + 0x23, 4)); if (di) { printf("case %d img:", n); diff64("", img, img_exp, 14); diff64("cols", img + 0x23, img_exp + 0x23, 4); }
		if (d || dc || df || di) bad++;
		memcpy(prev_flags, flags_exp, 32); memcpy(prev_img, img_exp, 64); have_prev = 1;
	}
	printf("%d ticks, %d with mismatches (%d in Char), %d dead-kid ticks differing only in the sound-timed death counter\n", n, bad, badchar, deathtimed); return bad != 0;
}
