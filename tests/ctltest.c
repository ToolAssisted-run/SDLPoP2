/* Equivalence test for control(): replay (Char, controls, ctrl1) -> Char quads captured at 2FDF:048C entry
 * and the following play_seq entry. Between them the game runs: control(), then control_dispatch flips back. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/types.h"
#include "../src/globals.h"
#include "../src/dat.h"
#include "stubs.h"
int main(int argc, char **argv)
{
	if (argc < 4) { fprintf(stderr, "usage: ctltest SEQUENCE.DAT level.bin quads.bin\n"); return 2; }
	stubs_init(argv[1], argv[2]);
	FILE *f = fopen(argv[3], "rb"); if (!f) return 2;
	unsigned char in[64], ctl[8], c1[8], out[64]; int n = 0, bad = 0;
	while (fread(in, 1, 64, f) == 64 && fread(ctl, 1, 8, f) == 8 && fread(c1, 1, 8, f) == 8 && fread(out, 1, 64, f) == 64) {
		n++; stubs_reset();
		memcpy(&Char, in, 64); control_x = ctl[0]; control_y = ctl[1]; control_shift = ctl[2];
		ctrl1_forward = c1[0]; ctrl1_backward = c1[1]; ctrl1_up = c1[2]; ctrl1_down = c1[3]; ctrl1_shift = c1[4];
		control();
		char_type exp; memcpy(&exp, out, 64);
		int d = memcmp(&Char, &exp, 64) != 0;
		if (d) { bad++; printf("case %d: frame %u seq %u ctrl1 %d %d %d %d %d -> expected seq %u pos %u got seq %u pos %u [%s]\n  diff:", n, ((char_type *)in)->frame, ((char_type *)in)->seq_id, (int8_t)c1[0], (int8_t)c1[1], (int8_t)c1[2], (int8_t)c1[3], (int8_t)c1[4], exp.seq_id, exp.seq_pos, Char.seq_id, Char.seq_pos, stubs_log()); for (int o = 0; o < 64; o++) if (((unsigned char *)&Char)[o] != out[o]) printf(" +%02X:%02X!=%02X", o, ((unsigned char *)&Char)[o], out[o]); printf("\n"); }
	}
	printf("%d cases, %d mismatches\n", n, bad); return bad != 0;
}
