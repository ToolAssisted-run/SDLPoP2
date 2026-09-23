/* Explore a level with the core (Go-Explore style): keep the first state that reached each cell (room, row, column),
 * restart from rarely tried cells and play random held inputs. Writes the per-tick inputs that lead to the chosen
 * cell (default: the last new room found; or a given room) as a plan file: one line per tick "x y shift".
 * EXPLORE_HP=n starts the prince with n hp; EXPLORE_PREFIX=plan plays a plan first; EXPLORE_CTRL=1 presses Ctrl too.
 * usage: explore GAME_DIR LEVEL SEED ITERATIONS OUT.plan [TARGET_ROOM]
 * build: cc -O2 -o explore tools/explore.c src/(all).c -lm */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../src/types.h"
#include "../src/globals.h"
#include "../src/core.h"

typedef struct { uint8_t *state; pop2_input *path; int len, tries, order; uint8_t room; } cell;
static cell cells[33 * 4 * 12 * 2]; static int ncells;
static int cell_index(uint8_t room, int8_t row, int8_t col) { if (room == 0 || room > 32 || row < -1 || row > 2 || col < -1 || col > 10) return -1; return ((Kid.charid == 1) * 33 * 4 + room * 4 + row + 1) * 12 + col + 1; }   /* the spirit (charid 1) apart */
static uint32_t rng = 1;
static uint32_t rnd(void) { rng = rng * 1103515245u + 12345u; return rng >> 16; }

int main(int argc, char **argv)
{
	if (argc < 6) { fprintf(stderr, "usage: explore GAME_DIR LEVEL SEED ITERATIONS OUT.plan [TARGET_ROOM]\n"); return 2; }
	if (!pop2_init(argv[1])) { fprintf(stderr, "pop2_init failed\n"); return 2; }
	int level = atoi(argv[2]), iters = atoi(argv[4]), target = argc > 6 ? atoi(argv[6]) : -1;
	uint32_t seed = (uint32_t)strtoul(argv[3], NULL, 0);
	size_t n = pop2_state_size();
	pop2_new_game(level, seed);
	if (getenv("EXPLORE_HP")) { Kid.f12 = Kid.f13 = (uint8_t)atoi(getenv("EXPLORE_HP")); }   /* (the oracle script pokes the same at tick 1) */
	static pop2_input path[4096]; int plen = 0, order = 0, best = -1; int room_seen[33] = {0};
	if (getenv("EXPLORE_PREFIX")) {   /* a plan to play first (kept at the start of every path) */
		FILE *pf = fopen(getenv("EXPLORE_PREFIX"), "r"); int x, y, sh;
		while (pf && plen < 3000 && fscanf(pf, "%d %d %d", &x, &y, &sh) == 3) { pop2_input in = {0}; in.x = x; in.y = y; in.shift = sh; path[plen++] = in; pop2_frame(&in); }
		if (pf) fclose(pf);
	}
	#define ADD_CELL(ci) do { cell *c = &cells[ci]; if (!c->state) { c->state = malloc(n); ncells++; c->order = order++; c->room = Kid.room; \
		if (Kid.room == target || (target < 0 && !room_seen[Kid.room < 33 ? Kid.room : 0])) best = ci; } else free(c->path); \
		pop2_save(c->state); c->path = malloc(sizeof(pop2_input) * (plen ? plen : 1)); memcpy(c->path, path, sizeof(pop2_input) * plen); c->len = plen; } while (0)
	int ci = cell_index(Kid.room, Kid.curr_row, Kid.curr_col); if (ci < 0) { fprintf(stderr, "start outside the grid (room %d row %d col %d)\n", Kid.room, Kid.curr_row, Kid.curr_col); return 2; } ADD_CELL(ci);
	room_seen[Kid.room < 33 ? Kid.room : 0] = 1; long ticks = 0, deaths = 0;
	for (int it = 0; it < iters; it++) {
		/* pick a cell: rarely tried ones first */
		double tot = 0; for (int i = 0; i < (int)(sizeof cells / sizeof cells[0]); i++) if (cells[i].state) tot += 1.0 / sqrt(cells[i].tries + 1.0);
		double r = (rnd() / 65536.0) * tot; int pick = -1;
		for (int i = 0; i < (int)(sizeof cells / sizeof cells[0]) && pick < 0; i++) if (cells[i].state) { r -= 1.0 / sqrt(cells[i].tries + 1.0); if (r <= 0) pick = i; }
		if (pick < 0) continue;
		cell *c = &cells[pick]; c->tries++;
		pop2_load(c->state); memcpy(path, c->path, sizeof(pop2_input) * c->len); plen = c->len;
		int lv = level;
		for (int t = 0; t < 60 && plen < 4000; ) {
			pop2_input in = {0}; uint32_t a = rnd();
			in.x = (int8_t)(a % 3) - 1; in.y = (int8_t)((a / 3) % 3) - 1; in.shift = (a / 9) % 4 == 0 ? 1 + ((a / 36) % 2 && getenv("EXPLORE_CTRL")) : 0;   /* EXPLORE_CTRL: Ctrl too */
			int hold = 1 + rnd() % 8;
			for (int h = 0; h < hold && t < 60 && plen < 4000; h++, t++) {
				path[plen++] = in; int res = pop2_frame(&in); ticks++;
				if (res == POP2_QUIT || (res > 0 && res != lv)) { t = 60; break; }   /* left the level: stop there */
				if (Kid.alive >= 0) { deaths++; t = 60; break; }                                /* dead: not a useful state */
				int k = cell_index(Kid.room, Kid.curr_row, Kid.curr_col);
				if (k < 0) continue;
				if (!cells[k].state || cells[k].len > plen) { int fresh = !room_seen[Kid.room < 33 ? Kid.room : 0]; ADD_CELL(k); if (fresh) { room_seen[Kid.room < 33 ? Kid.room : 0] = 1; fprintf(stderr, "iteration %d: room %d after %d ticks\n", it, Kid.room, plen); } }
			}
		}
	}
	if (best < 0) { fprintf(stderr, "target room not reached\n"); return 1; }
	FILE *f = fopen(argv[5], "w"); if (!f) return 2;
	for (int i = 0; i < cells[best].len; i++) fprintf(f, "%d %d %d\n", cells[best].path[i].x, cells[best].path[i].y, cells[best].path[i].shift);
	fclose(f);
	printf("%ld ticks, %ld deaths; %d cells, rooms:", ticks, deaths, ncells); for (int i = 0; i < 33; i++) if (room_seen[i]) printf(" %d", i);
	printf("; plan to room %d: %d ticks\n", cells[best].room, cells[best].len);
	return 0;
}
