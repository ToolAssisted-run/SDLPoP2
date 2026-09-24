/* SDLPoP2's own cheats (source/cheats.c) on the core: random play on every level, the same inputs without and with
 * god mode (with it: no death, no hit point lost), then every cheat used at random (look, teleport, fly, the sword,
 * the shadow / flame) with savestate round trips that must replay the same states.
 * usage: cheatstest GAME_DIR [frames] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../source/core.h"
#include "../source/types.h"
#include "../source/globals.h"

static uint32_t rng;
static uint32_t rnd(void) { rng = rng * 1103515245 + 12345; return rng >> 16; }
static pop2_input random_input(void)
{
	pop2_input in = {0}; uint32_t r = rnd();
	in.x = (int8_t)(r % 3) - 1; in.y = (int8_t)((r / 3) % 3) - 1;
	if (rnd() % 4) in.y = 0;
	in.shift = rnd() % 5 == 0 ? 1 : rnd() % 9 == 0 ? 2 : 0; in.keystroke = rnd() % 40 == 0;
	return in;
}
typedef struct { int deaths, hurts, frames; } play_stats;
/* frames of random play from a new game on level lv; god mode as given */
static play_stats play(int lv, int frames, int god)
{
	play_stats st = {0};
	pop2_new_game(lv, 0x9E3779B9u * (uint32_t)lv); rng = (uint32_t)lv * 7919u;
	cheat_god = (uint8_t)god;
	int level = pop2_level(), hp = (int8_t)Kid.f12, alive = (int8_t)Kid.alive < 0;
	for (int f = 0; f < frames; f++) {
		pop2_input in = random_input();
		int r = pop2_frame(&in); st.frames++;
		if (r == POP2_QUIT) break;
		if (pop2_level() != level || r > 0) { level = pop2_level(); hp = (int8_t)Kid.f12; alive = (int8_t)Kid.alive < 0; continue; }
		int now_alive = (int8_t)Kid.alive < 0, now_hp = (int8_t)Kid.f12;
		if (alive && !now_alive) st.deaths++;
		else if (alive && now_alive && now_hp < hp) st.hurts++;
		alive = now_alive; hp = now_hp;
	}
	cheat_god = 0;
	return st;
}
/* one frame with random cheats (as the shell's keys would use them, between two ticks) */
static void do_cheat(int act)   /* (as inside the prince's control, where the shell runs them: Char = the prince) */
{
	loadkid();
	switch (act) {
	case 0: case 1: case 2: case 3: cheat_look(act); break;
	case 4: cheat_teleport(); break;
	case 5: cheat_sword(); break;
	case 6: case 8: cheat_leave_body(act == 8); break;
	case 7: cheat_god_toggle(); break;
	}
	Kid = Char;
}
static void cheat_frame(pop2_input *in, int *act)
{
	*act = (int)(rnd() % 60);
	do_cheat(*act);
	cheat_fly_key = rnd() % 8 == 0 || (cheat_flying && rnd() % 4);
	*in = random_input();
}
static void replay_cheat_frame(const pop2_input *in, int act, uint8_t fly)
{
	do_cheat(act);
	cheat_fly_key = fly;
	pop2_frame(in);
}
/* the sweep: from a level's start the prince is put (look + teleport) standing on each floor tile of each room, then
 * plays at random for a while: without god mode he meets the level's dangers, with it none may hurt him */
static play_stats sweep(int lv, int frames_each, int god, const uint8_t *base)
{
	play_stats st = {0};
	for (int room = 1; room <= level.nrooms && room <= 28; room++)
		for (int tp = 0; tp < 30; tp++) {
			pop2_load(base);
			if (!tile_is_floor(ROOM_TILES(room)[tp])) continue;
			if (drawn_room != room) { cheat_view = (uint8_t)room; pop2_input z = {0}; pop2_frame(&z); if (drawn_room != room) continue; }
			Kid.curr_row = (int8_t)(tp / 10); Kid.curr_col = (int8_t)(tp % 10); Kid.x = col_x_left[tp % 10] + 0xE;
			loadkid(); char_y_to_floor(); Char.fall_x = Char.fall_y = 0; Kid = Char;
			if (Kid.room != room) { cheat_looking = 1; loadkid(); cheat_teleport(); Kid = Char; }
			cheat_god = (uint8_t)god; rng = (uint32_t)(lv * 1000 + room * 30 + tp);
			int level0 = pop2_level(), hp = (int8_t)Kid.f12, alive = (int8_t)Kid.alive < 0;
			for (int f = 0; f < frames_each; f++) {
				pop2_input in = random_input(); in.keystroke = 0;
				int r = pop2_frame(&in); st.frames++;
				if (r == POP2_QUIT || r > 0 || pop2_level() != level0) break;
				int now_alive = (int8_t)Kid.alive < 0, now_hp = (int8_t)Kid.f12;
				if (alive && !now_alive) { st.deaths++; break; }
				if (alive && now_alive && now_hp < hp) st.hurts++;
				alive = now_alive; hp = now_hp;
			}
		}
	cheat_god = 0;
	return st;
}
int main(int argc, char **argv)
{
	if (argc < 2) { fprintf(stderr, "usage: cheatstest GAME_DIR [frames]\n"); return 2; }
	if (!pop2_init(argv[1])) { fprintf(stderr, "pop2_init failed\n"); return 2; }
	int frames = argc > 2 ? atoi(argv[2]) : 3000, bad = 0, total_deaths = 0;
	for (int lv = 1; lv <= 14; lv++) {
		play_stats a = play(lv, frames, 0), b = play(lv, frames, 1);
		total_deaths += a.deaths + a.hurts;
		printf("level %2d: without god mode %d deaths, %d hurts; with it %d deaths, %d hurts%s\n", lv, a.deaths, a.hurts, b.deaths, b.hurts,
		       b.deaths || b.hurts ? "  FAIL" : "");
		if (b.deaths || b.hurts) bad++;
	}
	{   /* the sweep */
		size_t sn = pop2_state_size(); uint8_t *base = malloc(sn);
		for (int lv = 1; lv <= 14; lv++) {
			pop2_new_game(lv, 777u * (uint32_t)lv); pop2_input z = {0}; for (int f = 0; f < 3; f++) pop2_frame(&z);
			pop2_save(base);
			play_stats a = sweep(lv, 60, 0, base), b = sweep(lv, 60, 1, base);
			total_deaths += a.deaths + a.hurts;
			printf("level %2d sweep (%d rooms): without god mode %d deaths, %d hurts; with it %d deaths, %d hurts%s\n", lv, level.nrooms, a.deaths, a.hurts, b.deaths, b.hurts,
			       b.deaths || b.hurts ? "  FAIL" : "");
			if (b.deaths || b.hurts) bad++;
		}
		free(base);
	}
	if (total_deaths == 0) { printf("the random play met no danger: the god mode test proves nothing\n"); bad++; }
	/* every cheat at random, and savestate round trips through them */
	size_t n = pop2_state_size(); uint8_t *st = malloc(n);
	static pop2_input ins[20000]; static int acts[20000]; static uint8_t flys[20000]; static uint64_t h[20000];
	int rt_bad = 0;
	for (int lv = 1; lv <= 14; lv++) {
		pop2_new_game(lv, 12345u + (uint32_t)lv); rng = (uint32_t)lv;
		int k = frames / 3, flew = 0, looked = 0, n_f = frames;
		for (int f = 0; f < frames; f++) {
			if (f == k) pop2_save(st);
			cheat_frame(&ins[f], &acts[f]); flys[f] = cheat_fly_key;
			int r = pop2_frame(&ins[f]); h[f] = pop2_hash();
			if (cheat_flying) flew++;
			if (cheat_looking) looked++;
			if (r == POP2_QUIT) { n_f = f + 1; break; }
		}
		pop2_new_game(lv % 14 + 1, 99); for (int f = 0; f < 50; f++) { pop2_input z = random_input(); pop2_frame(&z); }
		pop2_load(st);
		int first = -1;
		for (int f = k; f < n_f; f++) { replay_cheat_frame(&ins[f], acts[f], flys[f]); if (pop2_hash() != h[f] && first < 0) first = f; }
		printf("level %2d: %d frames with cheats (flying %d, looking %d), level %d at the end, round trip from %d: %s\n", lv, n_f, flew, looked, pop2_level(), k,
		       first < 0 ? "ok" : "DIFFERS");
		if (first >= 0) rt_bad++;
		cheat_god = 0; cheat_fly_key = 0;
	}
	printf("cheatstest: %d god-mode failures, %d of 14 round trips differ\n", bad, rt_bad);
	free(st);
	return bad || rt_bad;
}
