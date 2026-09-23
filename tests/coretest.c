/* The core API on its own: random play on every level, then savestate round trips. A state saved at frame k,
 * loaded after the game went elsewhere, must replay the same inputs to the same states (hash per frame).
 * usage: coretest GAME_DIR [frames] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/core.h"

static uint32_t rng = 12345;
static uint32_t rnd(void) { rng = rng * 1103515245 + 12345; return rng >> 16; }
static pop2_input random_input(void)
{
	pop2_input in = {0}; uint32_t r = rnd();
	in.x = (int8_t)(r % 3) - 1; in.y = (int8_t)((r / 3) % 3) - 1;
	if (rnd() % 4) in.y = 0;   /* mostly running and turning */
	in.shift = rnd() % 5 == 0; in.keystroke = rnd() % 40 == 0;
	return in;
}
int main(int argc, char **argv)
{
	if (argc < 2) { fprintf(stderr, "usage: coretest GAME_DIR [frames]\n"); return 2; }
	if (!pop2_init(argv[1])) { fprintf(stderr, "pop2_init failed\n"); return 2; }
	int frames = argc > 2 ? atoi(argv[2]) : 1500, bad = 0, missing = 0;
	size_t n = pop2_state_size(); uint8_t *st = malloc(n);
	static pop2_input in[20000]; static uint64_t h[20000];
	for (int lv = 1; lv <= 14; lv++) {
		pop2_new_game(lv, 0x1234567u * lv); rng = lv;
		int k = frames / 3, levels = 0;
		for (int f = 0; f < frames; f++) {
			if (f == k) pop2_save(st);
			in[f] = random_input(); int r = pop2_frame(&in[f]); h[f] = pop2_hash();
			if (*pop2_missing()) { missing++; if (getenv("MISSING")) printf("M %d %s\n", lv, pop2_missing()); }
			if (r > 0) levels++;
			if (r == POP2_QUIT) { frames = f + 1; break; }
		}
		/* go somewhere else, then back to frame k */
		pop2_new_game(lv % 14 + 1, 99); for (int f = 0; f < 50; f++) { pop2_input z = random_input(); pop2_frame(&z); }
		pop2_load(st);
		int first = -1;
		for (int f = k; f < frames; f++) { pop2_frame(&in[f]); if (pop2_hash() != h[f] && first < 0) first = f; }
		printf("level %2d: %d frames, level %d at the end, %d (re)starts, round trip from %d: %s", lv, frames, pop2_level(), levels, k, first < 0 ? "ok" : "DIFFERS");
		if (first >= 0) { printf(" (first at frame %d)", first); bad++; }
		printf("\n");
		frames = argc > 2 ? atoi(argv[2]) : 1500;
	}
	printf("coretest: %d of 14 round trips differ; %d frames reached unreconstructed routines\n", bad, missing);
	free(st); return bad != 0;
}
