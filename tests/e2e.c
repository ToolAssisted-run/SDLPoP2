/* Free-running end-to-end test: start from the oracle's snapshot right after the level load (probe ls_a at
 * 169B:00F5), then run level_begin, the first room and whole frames in C with the capture script's keys, and
 * compare every tick (after 0823:0E72) with the oracle's ds_postroom snapshot. Nothing is reloaded on the way.
 * usage: e2e SEQUENCE.DAT ram.bin PRINCE.EXE events.txt capture.script */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/types.h"
#include "../src/globals.h"
#include "../src/glue.h"
#include "snap.h"
#include "../src/state.h"
#include "../src/core.h"

static struct { const char *name; int pos; } keymap[] = {   /* oracle key names -> DS:1D00 key table positions */
	{"left", 0x58}, {"right", 0x5A}, {"up", 0x55}, {"down", 0x5D}, {"home", 0x54}, {"pageup", 0x56}, {"end", 0x5C}, {"pagedown", 0x5E},
	{"kp7", 0x54}, {"kp8", 0x55}, {"kp9", 0x56}, {"kp4", 0x58}, {"kp5", 0x59}, {"kp6", 0x5A}, {"kp1", 0x5C}, {"kp2", 0x5D}, {"kp3", 0x5E},
	{"leftshift", 0x37}, {"rightshift", 0x43}, {"leftctrl", 0x2A}, {"rightctrl", 0x2A}, {"leftalt", 0x45}, {NULL, 0}};
static struct { int frame, pos, down; } keyev[4096]; static int nkeyev;
static uint8_t *tick_keys, *tick_flags, *tick_set;   /* scripts with per-tick input (probepoke ds_tick N ...) */
static uint8_t flags_of(const uint8_t *k) { return (k[0x37] ? 2 : 0) | (k[0x43] ? 1 : 0) | (k[0x2A] ? 4 : 0) | (k[0x45] ? 8 : 0); }
static int same_frame_keys;   /* keys set at the tick's own frame were already seen (1) or not yet (0) */
/* The keys as the tick at `frame` saw them. A key set during the tick's own frame may arrive before or after the tick
 * (the keyboard interrupt vs the tick's place in the frame): the controls control() received (probe kc_ctrl, 3 bytes,
 * after the facing flips) decide. */
static uint8_t before[0x70], with[0x70]; static int keys_differ;
static void use_keys(int w) { const uint8_t *pick = w ? with : before; memcpy(key_table, pick, sizeof key_table); bios_shift_flags = flags_of(pick); same_frame_keys = w; }
static void keys_at(int frame, const uint8_t *seen, int dead, int reload_follows)
{
	memset(before, 0, sizeof before); memset(with, 0, sizeof with);
	for (int i = 0; i < nkeyev && keyev[i].frame <= frame; i++) { if (keyev[i].frame < frame) before[keyev[i].pos] = keyev[i].down; with[keyev[i].pos] = keyev[i].down; }
	int8_t x, y, sh; const uint8_t *pick = with; same_frame_keys = 1;
	/* a dead prince's controls stay zero and say nothing: his same-frame keys have always been in (restarts, releases) */
	(void)reload_follows;
	if (!dead && seen && memcmp(before, with, sizeof with)) {
		keyboard_controls(with, flags_of(with), &x, &y, &sh);
		if (Kid.direction == 0) x = -x;
		if (word_5d38) y = -y;
		if ((uint8_t)x != seen[0] || (uint8_t)y != seen[1] || (uint8_t)sh != seen[2]) { pick = before; same_frame_keys = 0; }
	}
	memcpy(key_table, pick, sizeof key_table); bios_shift_flags = flags_of(pick);
	keys_differ = memcmp(before, with, sizeof with) != 0;
}
/* the keystroke queue read by 0823:02BE (2768:02CA, the library's event queue): every key-down of a non-modifier key,
 * plus the keyboard's auto-repeat of the last key pressed (DOSBox-X: after 500 ms, then every 33 ms; frames at 70.086 Hz) */
static double keyq[65536]; static int nkeyq, keyq_next;
static void build_keyq(void)
{
	const double fr = 1000.0 / 70.086;
	for (int i = 0; i < nkeyev; i++) {
		int p = keyev[i].pos;
		if (!keyev[i].down || p == 0x37 || p == 0x43 || p == 0x2A || p == 0x45) continue;
		keyq[nkeyq++] = keyev[i].frame;
		/* repeats until this key is released or another key goes down */
		double end = 1e9;
		for (int j = i + 1; j < nkeyev; j++) if ((keyev[j].pos == p && !keyev[j].down) || (keyev[j].down && keyev[j].pos != p)) { end = keyev[j].frame; break; }
		for (double t = keyev[i].frame + 500 / fr; t < end && nkeyq < 65535; t += 33 / fr) keyq[nkeyq++] = t;
	}
	for (int i = 1; i < nkeyq; i++) for (int j = i; j > 0 && keyq[j - 1] > keyq[j]; j--) { double t = keyq[j]; keyq[j] = keyq[j - 1]; keyq[j - 1] = t; }
}
static int cur_tick_frame;
/* The library's event queue (194C:9858 / 9A0F): up to 8 keystrokes, new ones dropped when full. A read (0823:02BE via
 * 2768:02CA) pops it, or polls DOS (0823:16EE) directly when it is empty; after the read, the rest of the frame pumps
 * every keystroke waiting in the BIOS buffer into the queue. */
static int libq; static double pump_until = -1;
static int bios_ready(double t) { return keyq_next < nkeyq && keyq[keyq_next] < t; }
int bios_key(void)
{
	double now = cur_tick_frame + (same_frame_keys ? 0.5 : 0.0);
	/* the pump of the previous frames */
	while (bios_ready(pump_until)) { keyq_next++; if (libq < 8) libq++; }
	int got = 0;
	if (libq) { libq--; got = 1; }
	else if (bios_ready(now)) { keyq_next++; got = 1; }
	pump_until = cur_tick_frame + 0.5;
	return got ? 0x100 : 0;
}
static int hexval(int c) { return c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10; }
static int sound_busy, ambient_draws, rng_lost, restarts, resync, scenes, timing_flips, sound_answers; int death_sound_playing(int both) { (void)both; return sound_busy; }
/* sounds gating a random draw inside a tick (level 2's room-3 edge, 33FD:02E9): not playing if the capture's
 * post-tick seed lies ahead of ours */
static uint32_t seed_b[8192]; static int seed_b_ok[8192], cur_tick_ix = -1;
/* the lateness meter DS:2BA4 at each tick's start: this frame was late if the next tick starts with it higher */
static uint16_t lag_a[8192]; static int nt_all, pace_ix = -1;
int frame_on_time(void) { return !(pace_ix >= 0 && pace_ix + 1 < nt_all && lag_a[pace_ix + 1] > word_2ba4); }
/* the answers to "is sound n playing" (the sound driver's timing is not modelled) in the current tick: by default from
 * the capture's seed (a random draw follows a "no"); when the tick disagrees with the capture, other answers are tried */
static int sq_forced, sq_count; static unsigned sq_mask;
int sound_playing(uint16_t id)
{
	(void)id; int r;
	if (sq_forced) r = (sq_mask >> sq_count) & 1;
	else if (cur_tick_ix < 0 || !seed_b_ok[cur_tick_ix]) r = 1;
	else { uint32_t s = random_seed, want = seed_b[cur_tick_ix]; r = 1; for (int k = 1; k <= 4 && r; k++) { s = s * 0x343FD + 0x269EC3; if (s == want) r = 0; } }
	sq_count++; return r;
}

int main(int argc, char **argv)
{
	if (argc < 6) { fprintf(stderr, "usage: e2e SEQUENCE.DAT ram.bin PRINCE.EXE events.txt capture.script\n"); return 2; }
	glue_init(argv[1], "/dev/null"); glue_load_exe_tables(argv[3]);
	static uint8_t ram[655360]; FILE *rf = fopen(argv[2], "rb"); if (!rf || fread(ram, 1, sizeof ram, rf) != sizeof ram) return 2; fclose(rf);
	if (getenv("E2E_EXE_DS")) {   /* static tables from PRINCE.EXE's data segment (file 0x3CE40 = DS:0), runtime values from the dump */
		static uint8_t img[655360]; memcpy(img, ram, sizeof img); FILE *xf = fopen(argv[3], "rb"); memset(img + 0x3B250, 0, 0x2900);
		fseek(xf, 0x3CE40, SEEK_SET); if (fread(img + 0x3B250, 1, 0x27BF, xf) != 0x27BF) return 2; fclose(xf);
		for (const char *q = getenv("E2E_EXE_DS"); *q; ) { unsigned a0, n0; int used; if (sscanf(q, "%x:%x%n", &a0, &n0, &used) != 2) break; memcpy(img + 0x3B250 + a0, ram + 0x3B250 + a0, n0); q += used; if (*q == ',') q++; }
		glue_load_ds_tables(img);
	} else glue_load_ds_tables(ram);
	level_roomlinks = (uint8_t *)&level + 0x17BC;
	FILE *sf = fopen(argv[5], "r"); char line[65536];
	while (sf && fgets(line, sizeof line, sf)) {
		int f, d; char name[32];
		{ unsigned hit, phys; char hex[600];   /* per-tick input written at the tick's start (plan2script.py) */
		  if (sscanf(line, "probepoke ds_tick %u %x %599s", &hit, &phys, hex) == 3 && hit < 8192) {
			if (!tick_keys) tick_keys = calloc(8192, 0x70), tick_flags = calloc(8192, 1), tick_set = calloc(8192, 1);
			if (phys == 0x3CF50) { for (int q = 0; q < 0x70 && hex[2 * q]; q++) tick_keys[hit * 0x70 + q] = hexval(hex[2 * q]) << 4 | hexval(hex[2 * q + 1]); tick_set[hit] = 1; }
			else if (phys == 0x417) tick_flags[hit] = hexval(hex[0]) << 4 | hexval(hex[1]);
			continue; } }
		if (sscanf(line, "key %d %31s %d", &f, name, &d) != 3) continue;
		for (int k = 0; keymap[k].name; k++) if (!strcmp(keymap[k].name, name) && nkeyev < 4096) { keyev[nkeyev].frame = f; keyev[nkeyev].pos = keymap[k].pos; keyev[nkeyev++].down = d; }
	}
	build_keyq();
	FILE *ef = fopen(argv[4], "r"); if (!ef) return 2;
	/* first pass: the prince's death count before and after each tick (the death sound is not modelled: a count that
	 * did not advance means it was still playing) */
	static int8_t alive_a[8192], alive_b[8192]; static char reload_after[8192]; static uint8_t *postmem[8192]; static uint8_t kc[8192][3]; static char kc_ok[8192]; int nt = 0;
	{ static char l2[0x20000]; while (fgets(l2, sizeof l2, ef)) {
		char *p = strstr(l2, " probe="), *m = strstr(l2, "mem="); if (!p || !m) continue;
		char nm2[32] = ""; sscanf(strchr(p + 1, ' ') + 1, "%31s", nm2);
		if (!strcmp(nm2, "kc_ctrl") && nt && !kc_ok[nt - 1]) { for (int q = 0; q < 3; q++) kc[nt - 1][q] = hexval(m[4 + 2 * q]) << 4 | hexval(m[5 + 2 * q]); kc_ok[nt - 1] = 1; continue; }
		int off = (0x5B36 + 0x11 - 0x2900) * 2; if ((int)strlen(m + 4) < off + 2) continue;
		int8_t v = (int8_t)(hexval(m[4 + off]) << 4 | hexval(m[5 + off]));
		if (!strcmp(nm2, "ls_a") && nt) reload_after[nt - 1] = 1;
		if (!strcmp(nm2, "ds_tick") && nt < 8191) { int lo = (0x2BA4 - 0x2900) * 2; lag_a[nt] = (hexval(m[4 + lo]) << 4 | hexval(m[5 + lo])) | (hexval(m[6 + lo]) << 4 | hexval(m[7 + lo])) << 8; alive_a[nt] = v; alive_b[nt] = -128; kc_ok[nt] = 0; reload_after[nt] = 0; nt++; }
		else if (!strcmp(nm2, "ds_postroom") && nt) { if (!postmem[nt - 1] && strlen(m + 4) >= 2 * 0x4300) { postmem[nt - 1] = malloc(0x4300); for (int q = 0; q < 0x4300; q++) postmem[nt - 1][q] = hexval(m[4 + 2 * q]) << 4 | hexval(m[5 + 2 * q]); } alive_b[nt - 1] = v; int so = (0x2B7A - 0x2900) * 2; uint32_t w = 0; for (int q = 3; q >= 0; q--) w = w << 8 | (hexval(m[4 + so + 2 * q]) << 4 | hexval(m[5 + so + 2 * q])); seed_b[nt - 1] = w; seed_b_ok[nt - 1] = 1; }
	} rewind(ef); }
	nt_all = nt;
	for (int q = 0; q + 1 < nt; q++) if (alive_b[q] == -128) alive_b[q] = alive_a[q + 1];   /* ticks without a post-tick sample: the next tick's start */
	int tick_ix = 0, sc;
	static char big[0x20000]; static uint8_t mem[0x4300], got[0x4300];
	static const char *const regions[] = {"Kid", "chars", "level", "trobs", "trob_count", "mobs", "mob_count", "random_seed", "drawn_room", "room_L", "room_R", "room_A", "room_B", "next_room", "exit_dir",
		"word_6140", "word_6146", "word_68ec", "word_68f0", "word_922e", "floor_ptrs", "kid_ctrl1_saved", "minutes_left", "clock_ticks", NULL};
	static const char *const extra[] = {"coll", "prev_coll_flags", "curr_row_coll_flags", "Char", "Opp", "cur_frame", "obj_x", "obj_y", "obj_id", "obj_chtab", "char_x_left", "char_x_right", "char_top_y", "char_col_left", "char_col_right",
		"char_top_row", "char_bottom_row", "tile_col", "tile_row", "curr_tile", "curr_room", "knock", "word_8a84", "word_6142", "word_5cd8", "ctrl1_forward", "ctrl1_backward", "ctrl1_up", "ctrl1_down", "ctrl1_shift", "byte_9276", "word_5ce8", "tick", "word_5d38", NULL};
	/* E2E_STRICT: every mapped field; a summary of the fields that ever differ */
	static const char *strict[400]; static int strict_bad[400]; int nstrict = 0;
	/* not compared: drawing and sound state (DS:2B68 the level-1 palette, 33FD:0128; DS:2B9A the ambient sound, 1611:03CC) */
	if (getenv("E2E_STRICT")) { for (int i = 0; i < snap_nfields && nstrict < 399; i++) if (strcmp(snap_fields[i].name, "byte_2b68")) strict[nstrict++] = snap_fields[i].name; strict[nstrict] = NULL; }
	int started = 0, n = 0, bad = 0, first_bad = 0, pending = 0, ticks = 0, tick_n = 0;
	while (fgets(big, sizeof big, ef)) {
		char *lab = strstr(big, " probe="); if (!lab) continue;
		int frame = atoi(big + 6); char *m = strstr(big, "mem="); char nm[32] = ""; sscanf(strchr(lab + 1, ' ') + 1, "%31s", nm);
		int len = 0; if (m) for (char *p = m + 4; p[0] && p[1] && p[0] != '\n' && len < (int)sizeof mem; p += 2) mem[len++] = hexval(p[0]) << 4 | hexval(p[1]);
		if (resync && !strcmp(nm, "ls_a") && len == 0x4300) {   /* a story scene (NIS, not reconstructed) played before this level load */
			snap_load(mem); glue_select_guard_dat(level.type); level_begin(); level_first_room(); resync = 0; pending = 0;
			while (keyq_next < nkeyq && keyq[keyq_next] < frame) keyq_next++;   /* the scene took the keystrokes made while it played */
			libq = 0; pump_until = frame; continue;
		}
		if (resync) { if (!strcmp(nm, "ds_tick")) tick_ix++; continue; }   /* (keep the per-tick tables of the first pass aligned) */
		if (!started) {
			if (strcmp(nm, "ls_a") || len != 0x4300) continue;
			SNAP_SIZE = 0x4300; SNAP_BASE = 0x6C00 - SNAP_SIZE;
			if (getenv("E2E_COLD")) {   /* a new game from zeroed memory: 169B:0006 then the level load; the seed comes from the capture */
				uint32_t seed = mem[0x2B7A - SNAP_BASE] | mem[0x2B7B - SNAP_BASE] << 8 | mem[0x2B7C - SNAP_BASE] << 16 | (uint32_t)mem[0x2B7D - SNAP_BASE] << 24;
				int lv = mem[0x43FF - SNAP_BASE]; char dir[512]; snprintf(dir, sizeof dir, "%s", argv[3]); *strrchr(dir, '/') = 0;
				if (!pop2_init(dir)) return 2;
				pop2_new_game_loaded(lv, seed);   /* up to the level load (the ls_a probe); level_begin follows below */
				memcpy(got, mem, SNAP_SIZE); snap_store(got);
				int nd = 0, unk = 0; char last[64] = "";
				for (int a = 0; a < SNAP_SIZE; a++) if (got[a] != mem[a]) {
					int off; const char *f = snap_field_at(SNAP_BASE + a, &off);
					if (!f) { unk++; continue; }
					if (strcmp(last, f)) { if (nd) printf("\n"); printf("  cold %s:", f); snprintf(last, sizeof last, "%s", f); }
					if (nd++ < 400) printf(" +%X:%02X!=%02X", off, got[a], mem[a]);
				}
				printf("\ncold start: %d differing bytes in known fields\n", nd);
			} else { snap_load(mem); if (level_kind == 1) byte_14a0 = 0xFF; }   /* (DS:14A0 is outside the sample; the load just set it, 1286:02D9) */
			glue_select_guard_dat(level.type);
			level_begin(); level_first_room(); started = 1; continue;
		}
		if (!strcmp(nm, "ds_tick")) {
			if (pending) { frame_after_tick(0); frame_wait(); pending = 0; }   /* (a normal tick without its post-tick sample) */
			/* the ambient sounds (1611:03CC) draw random numbers depending on the sound driver's timing: catch up */
			uint32_t want = mem[0x2B7A - SNAP_BASE] | mem[0x2B7B - SNAP_BASE] << 8 | mem[0x2B7C - SNAP_BASE] << 16 | (uint32_t)mem[0x2B7D - SNAP_BASE] << 24;
			int k; uint32_t s = random_seed;
			for (k = 0; k <= 4 && s != want; k++) s = s * 0x343FD + 0x269EC3;
			if (k <= 4) { random_seed = s; ambient_draws += k; } else rng_lost++;
			/* one frame: 169B:0BA6, then the tick with this tick's keys */
			const char_type *ak = (const char_type *)(mem + 0x5B36 - SNAP_BASE);
			(void)ak; sound_busy = tick_ix < nt && alive_b[tick_ix] != -128 && alive_b[tick_ix] >= 0 && alive_b[tick_ix] == (alive_a[tick_ix] < 0 ? 0 : alive_a[tick_ix]); tick_ix++; pace_ix = tick_ix - 1;
			if (tick_keys) {   /* the input the script wrote at this tick's start (the last one written before, if none) */
				int h = tick_ix; while (h > 0 && !tick_set[h]) h--;
				memcpy(key_table, tick_keys + h * 0x70, 0x70); bios_shift_flags = tick_flags[h]; keys_differ = 0; same_frame_keys = 1;
			} else keys_at(frame, tick_ix - 1 < nt && kc_ok[tick_ix - 1] ? kc[tick_ix - 1] : NULL, Kid.alive >= 0, tick_ix - 1 < nt && reload_after[tick_ix - 1]); cur_tick_frame = frame; missing_reset();
			if (getenv("E2E_TRACE") && ticks + 1 >= atoi(getenv("E2E_TRACE")) - 8 && ticks + 1 <= atoi(getenv("E2E_TRACE"))) printf("  t%d frame %d: kid alive %d busy %d keyq %d/%d next %.1f\n", ticks + 1, frame, Kid.alive, sound_busy, keyq_next, nkeyq, keyq_next < nkeyq ? keyq[keyq_next] : -1.0);
			/* a key changing during the tick's own frame reached it or not (the keyboard interrupt vs the tick's place in
			 * the frame), and the sound queries: when the first choice gives a state the capture does not have, the other
			 * key timing and other answers are tried */
			static uint8_t *st0; if (!st0) st0 = malloc(state_size());
			int can_retry = tick_ix - 1 < nt && postmem[tick_ix - 1] != NULL, r;
			int kq0 = keyq_next, lq0 = libq, sf0 = same_frame_keys; double pu0 = pump_until;
			uint8_t kt0[0x70], bf0 = bios_shift_flags; memcpy(kt0, key_table, sizeof kt0);
			if (can_retry) state_save(st0);
			cur_tick_ix = tick_ix - 1; sq_forced = 0; sq_count = 0;
			frame_begin(); r = tick_main();
			#define TICK_DIFFERS() (memcpy(got, postmem[tick_ix - 1], SNAP_SIZE), snap_store(got), snap_diff(got, postmem[tick_ix - 1], regions, 0))
			if (can_retry && r == 0 && TICK_DIFFERS()) {
				int nq = sq_count > 4 ? 4 : sq_count, found = 0;
				for (int kk = 0; kk < (keys_differ ? 2 : 1) && !found; kk++)
					for (unsigned mm = 0; mm < (1u << nq) && !found; mm++) {
						state_load(st0); keyq_next = kq0; libq = lq0; pump_until = pu0; if (kk) use_keys(!sf0); else { memcpy(key_table, kt0, sizeof kt0); bios_shift_flags = bf0; same_frame_keys = sf0; }
						sq_forced = nq > 0; sq_mask = mm; sq_count = 0;
						frame_begin(); r = tick_main();
						if (r == 0 && !TICK_DIFFERS()) { found = 1; if (kk) timing_flips++; if (nq) sound_answers++; }
					}
				if (!found) { state_load(st0); keyq_next = kq0; libq = lq0; pump_until = pu0; memcpy(key_table, kt0, sizeof kt0); bios_shift_flags = bf0; same_frame_keys = sf0; sq_forced = 0; sq_count = 0; frame_begin(); r = tick_main(); }
			}
			sq_forced = 0;
			ticks++; cur_tick_ix = -1;
			if (r == 0) { pending = 1; tick_n = ticks; continue; }   /* compared at ds_postroom (169B:064F) */
			r = frame_after_tick(r); frame_wait();   /* frozen (-2) or quit (-1): no post-tick sample */
			if (r == -1) { printf("tick %d: level left (-1)\n", ticks); break; }
			if (r >= 0) { printf("tick %d: level %d (re)starts\n", ticks, r); if (r == 0) break; restarts++; if (!word_5cb6 && (sc = story_scene((int8_t)word_32d8, r))) { scene_played(sc); resync = 1; scenes++; continue; } if (!load_level(r)) break; level_begin(); level_first_room(); }
			continue;
		}
		if (strcmp(nm, "ds_postroom") || !pending) continue;
		pending = 0;
		const char_type *ek = (const char_type *)(mem + 0x5B36 - SNAP_BASE);
		(void)ek;
		memcpy(got, mem, SNAP_SIZE); snap_store(got); n++;
		if (nstrict) memcpy(got + 0x2B9A - SNAP_BASE, mem + 0x2B9A - SNAP_BASE, 2);
		if (nstrict) for (int i = 0; i < nstrict; i++) { const char *one[2] = {strict[i], NULL}; if (snap_diff(got, mem, one, 0) && !strict_bad[i]++ && getenv("E2E_STRICT")[0] == 'v') { printf("strict: %s first differs at tick %d\n", strict[i], tick_n); snap_diff(got, mem, one, 1); } }
		if (snap_diff(got, mem, regions, 0)) { bad++; if (!first_bad) first_bad = tick_n; if (bad <= 5) { printf("tick %d (frame %d) [%s]\n", tick_n, frame, missing_log()); snap_diff(got, mem, regions, 1); } }
		if (getenv("E2E_ALL") && tick_n >= atoi(getenv("E2E_ALL")) - 2 && tick_n <= atoi(getenv("E2E_ALL"))) { printf("tick %d other state:\n", tick_n); snap_diff(got, mem, extra, 1); }
		int r = frame_after_tick(0); frame_wait();
		if (r == -1) { printf("tick %d: level left (-1)\n", tick_n); break; }
		if (r >= 0) { printf("tick %d: level %d (re)starts\n", tick_n, r); if (r == 0) break; restarts++; if (!word_5cb6 && (sc = story_scene((int8_t)word_32d8, r))) { scene_played(sc); resync = 1; scenes++; continue; } if (!load_level(r)) break; level_begin(); level_first_room(); }
	}
	if (nstrict) { printf("strict:"); for (int i = 0; i < nstrict; i++) if (strict_bad[i]) printf(" %s(%d)", strict[i], strict_bad[i]); printf("\n"); }
	printf("e2e: %d ticks free-running (%d compared), %d mismatching (first at tick %d); %d restarts (%d after a story scene, resynced); %d ambient random draws synced, %d unmatched; %d same-frame key timings and %d sound answers taken from the capture\n", ticks, n, bad, first_bad, restarts, scenes, ambient_draws, rng_lost, timing_flips, sound_answers);
	return bad != 0;
}
