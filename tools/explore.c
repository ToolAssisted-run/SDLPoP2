/* Explore a level with the core (Go-Explore style): keep the first state that reached each cell (room, row, column),
 * restart from rarely tried cells and play random held inputs. Writes the per-tick inputs that lead to the chosen
 * cell (default: the last new room found; or a given room) as a plan file: one line per tick "x y shift".
 * EXPLORE_HP=n starts the prince with n hp; EXPLORE_PREFIX=plan plays a plan first; EXPLORE_CTRL=1 presses Ctrl too; EXPLORE_RNG=n seeds the explorer's choices; EXPLORE_KEY=trobs adds the live animation count to the cells.
 * usage: explore GAME_DIR LEVEL SEED ITERATIONS OUT.plan [TARGET_ROOM]
 * build: cc -O2 -o explore tools/explore.c source/(all).c -lm */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../source/types.h"
#include "../source/globals.h"
#include "../source/core.h"

typedef struct { uint8_t *state; pop2_input *path; int len, tries, order; uint8_t room; } cell;
static cell cells[33 * 4 * 12 * 2 * 6]; static int ncells; static int key_trobs;   /* EXPLORE_KEY=trobs: the live animations (gates, buttons) count too */
static int key_jaffar;   /* EXPLORE_KEY=jaffar (level 14): the progress against the Jaffars instead of the animation count; a hit in rooms 7/8 or a win ends the search */
static int jaffar_hit;   /* a Jaffar in rooms 7/8 on seq 0xF3 (hit by a fireball: the win follows) */
static int jaffars_dead(void)   /* progress: Jaffars gone from room 6 plus those in rooms 7/8 (0..3); sets jaffar_hit */
{
	int live6 = 0, n78 = 0; jaffar_hit = 0;
	for (int r = 6; r <= 8; r++) {
		if (r == drawn_room) { for (int i = 0; i < room_nchars(r); i++) if (chars[i].charid == 6 && (r == 6 || chars[i].alive < 0 || chars[i].f19 == 0xF3)) { if (r == 6) live6++;   /* (room 6: bodies count too, the last one leaves only when alone) */ else { n78++; jaffar_hit |= chars[i].f19 == 0xF3; } } }
		else for (int i = 0; i < ROOM_REC(r)->nchars; i++) if (ROOM_REC(r)->chars[i].type == 3) { if (r == 6) live6++; else n78++; }
	}
	int p = 4 - live6 + n78;
	if (getenv("EXPLORE_J78")) {   /* EXPLORE_J78: the kill in rooms 7/8: 1 the spirit (hp > 2) there, 2 on a live Jaffar's row, 3 and a fireball flies */
		int row = 0, fire = 0;
		if (drawn_room == 7 || drawn_room == 8) for (int i = 0; i < room_nchars(drawn_room); i++) row |= chars[i].charid == 6 && chars[i].alive < 0 && chars[i].curr_row == Kid.curr_row;
		for (int i = 0; i < mob_count; i++) fire |= mobs[i].type == 0xC && mobs[i].speed >= 0 && mobs[i].row == (uint8_t)Kid.curr_row;
		p = (Kid.charid == 1 && (int8_t)Kid.f12 > 2 && (drawn_room == 7 || drawn_room == 8)) ? 1 + row + (row && fire) : (row && fire) ? 3 : 0;
		if (jaffar_hit) {   /* 4 a Jaffar hit there (seq 0xF3), 5 his dying frames under way (0x15F wins) */
			int fr = 0; for (int i = 0; i < room_nchars(drawn_room); i++) if (chars[i].charid == 6 && chars[i].f19 == 0xF3 && chars[i].frame > fr) fr = chars[i].frame;
			p = fr >= 0x150 ? 5 : 4;
		}
		return p;
	}
	return p > 3 ? 3 : p;
}
static int key_puzzle;   /* EXPLORE_KEY=puzzle (level 2): the puzzle's wrong tiles pressed (3..5 -> 1..3), a level exit ends the search */
static int puzzle_key(void)
{
	int n = 0;
	for (int i = 0; i < 6; i++) if (i != puzzle_answer && (ROOM_ATTRS(1)[12 + i] & 0xF) == 0) n++;
	return n >= 5 ? 3 : n >= 4 ? 2 : n >= 3 ? 1 : 0;
}
static int cell_index(uint8_t room, int8_t row, int8_t col) { if (room == 0 || room > 32 || row < -1 || row > 2 || col < -1 || col > 10) return -1; return (((key_trobs ? (int)(trob_count & 3) : key_puzzle ? puzzle_key() : key_jaffar ? (jaffars_dead() > 5 ? 5 : jaffars_dead()) : 0) * 2 + (Kid.charid == 1)) * 33 * 4 + room * 4 + row + 1) * 12 + col + 1; }   /* the spirit (charid 1) apart */
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
	key_trobs = getenv("EXPLORE_KEY") && !strcmp(getenv("EXPLORE_KEY"), "trobs"); key_jaffar = getenv("EXPLORE_KEY") && !strcmp(getenv("EXPLORE_KEY"), "jaffar"); key_puzzle = getenv("EXPLORE_KEY") && !strcmp(getenv("EXPLORE_KEY"), "puzzle"); int best_dead = 0;
	if (getenv("EXPLORE_RNG")) rng = (uint32_t)strtoul(getenv("EXPLORE_RNG"), NULL, 0);   /* the explorer's own choices */
	if (getenv("EXPLORE_HP") && *getenv("EXPLORE_HP")) { Kid.f12 = Kid.f13 = (uint8_t)atoi(getenv("EXPLORE_HP")); }   /* (the oracle script pokes the same at tick 1) */
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
				if ((key_jaffar || key_puzzle) && ((res > 0 && res != lv) || (res == POP2_QUIT && lv == 14)))   /* (level 14 won: the game ends) */ { int k2 = cell_index(Kid.room ? Kid.room : 1, 0, 0); if (k2 >= 0) { ADD_CELL(k2); best = k2; } fprintf(stderr, "iteration %d: level won after %d ticks\n", it, plen); t = 60; it = iters; break; }
				if (res == POP2_QUIT || (res > 0 && res != lv)) { t = 60; break; }   /* left the level: stop there */
				if (Kid.alive >= 0) { deaths++; t = 60; break; }                                /* dead: not a useful state */
				int k = cell_index(Kid.room, Kid.curr_row, Kid.curr_col);
				if (k < 0) continue;
				if (key_jaffar && jaffars_dead() > best_dead) { best_dead = jaffars_dead(); fprintf(stderr, "iteration %d: jaffar key %d after %d ticks (drawn %d, kid room %d)\n", it, best_dead, plen, drawn_room, Kid.room);
					if (getenv("EXPLORE_DIAG")) { for (int i = 0; i < room_nchars(drawn_room); i++) fprintf(stderr, "  char %d charid %d alive %d hp %d room %d seq %x frame %x\n", i, chars[i].charid, chars[i].alive, chars[i].f12, chars[i].room, chars[i].f19, chars[i].frame);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"   /* (ROOM_REC from &level: GCC -O3 misreads the object size) */
						for (int r = 1; r <= 28; r++) for (int i = 0; i < ROOM_REC(r)->nchars; i++) fprintf(stderr, "  room %d rec %d type %d hp %d\n", r, i, ROOM_REC(r)->chars[i].type, ROOM_REC(r)->chars[i].hp); } }
#pragma GCC diagnostic pop
				if (key_jaffar && getenv("EXPLORE_STOPKEY") && jaffars_dead() >= atoi(getenv("EXPLORE_STOPKEY"))) { ADD_CELL(k); best = k; fprintf(stderr, "iteration %d: jaffar key %d reached: plan kept (%d ticks)\n", it, jaffars_dead(), plen); t = 60; it = iters; break; }
				if (getenv("EXPLORE_STOPSEQ") && Kid.f19 == (uint16_t)strtol(getenv("EXPLORE_STOPSEQ"), NULL, 0)) { ADD_CELL(k); best = k; fprintf(stderr, "iteration %d: seq %s reached: plan kept (%d ticks)\n", it, getenv("EXPLORE_STOPSEQ"), plen); t = 60; it = iters; break; }   /* EXPLORE_STOPSEQ: stop when the prince is in that sequence */
				if (key_jaffar && getenv("EXPLORE_STOPHIT") && (jaffars_dead(), jaffar_hit)) { ADD_CELL(k); best = k; fprintf(stderr, "iteration %d: a Jaffar hit in rooms 7/8: plan kept (%d ticks)\n", it, plen); t = 60; it = iters; break; }
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
