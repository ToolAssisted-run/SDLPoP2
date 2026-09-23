/* Snapshot equivalence tests: load the whole-DS snapshot taken before a phase of the tick (169B:05E0), run the
 * reconstructed phase, compare with the snapshot taken after it.
 *   room : 2D3E:108A check_kid_left_room + 0823:0E72 switch_room   (pairs ds_preleave -> ds_postroom)
 *   chars: 169B:07EC play_all_chars                                (pairs ds_prechars -> ds_postchars)
 *   tick : 169B:05E0 the whole tick body (ds_tick -> ds_postroom, plus the prince's kc_ctrl:8 kc_ctrl1:16) */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/types.h"
#include "../src/globals.h"
#include "../src/dat.h"
#include "stubs.h"
#include "snap.h"
static uint8_t kctl[8], kc1[16]; static int tick_mode;
/* 0823:10A0 read_input replaced by the captured controls (taken inside control(), after the facing flips of 0AFF:12CA) */
int read_input(void)
{
	next_room = 0;
	control_x = Char.direction == 0 ? -(int8_t)kctl[0] : (int8_t)kctl[0]; control_y = word_5d38 ? -(int8_t)kctl[1] : (int8_t)kctl[1]; control_shift = kctl[2];
	(void)kc1; return (int8_t)control_shift;
}
/* the sound driver is not modelled: a dead prince's counter that did not advance means the death sound was playing */
static int sound_busy;
int death_sound_playing(int both) { (void)both; return sound_busy; }
extern int coll_debug;
int main(int argc, char **argv)
{
	if (argc < 6) { fprintf(stderr, "usage: snaptest room|chars SEQUENCE.DAT ram.bin PRINCE.EXE pairs.bin\n"); return 2; }
	int chars_mode = !strcmp(argv[1], "chars"); tick_mode = !strcmp(argv[1], "tick"); int start_mode = !strcmp(argv[1], "start"), between_mode = !strcmp(argv[1], "between");
	stubs_init(argv[2], "/dev/null"); stubs_load_frame_tables(argv[4]);
	static uint8_t ram[655360]; FILE *rf = fopen(argv[3], "rb"); if (!rf || fread(ram, 1, sizeof ram, rf) != sizeof ram) return 2; fclose(rf); stubs_load_ds_tables(ram);
	level_roomlinks = (uint8_t *)&level + 0x17BC;
	FILE *f = fopen(argv[5], "rb"); if (!f) return 2;
	FILE *hf = argc > 6 ? fopen(argv[6], "rb") : NULL;   /* optional near-heap samples at each tick start (collapsing floors) */
	static uint8_t heap[0x1000], prev_objs[4][0x65]; uint32_t hsize = 0; uint16_t prev_ptrs[4] = {0}; int prev_ok = 0, prev_n = 0; uint32_t prev_frame = 0;
	static uint8_t a[SNAP_MAX], b[SNAP_MAX], got[SNAP_MAX]; uint32_t frame, size; int n = 0, bad = 0, busy = 0, skipped = 0;
	static const char *const room_regions[] = {"Kid", "chars", "level", "drawn_room", "room_L", "room_R", "room_A", "room_B", "room_AL", "room_AR", "room_BL", "room_BR", "next_room", "exit_dir", "pal_slots", "word_922a", NULL};
	static const char *const chars_regions[] = {"chars", "Kid", "level", "random_seed", "drawn_room", "room_L", "room_R", "word_6140", "word_6146", "word_68ec", "word_68f0", "word_922e", NULL};
	static const char *const tick_regions[] = {"Kid", "chars", "level", "trobs", "trob_count", "mobs", "mob_count", "random_seed", "drawn_room", "room_L", "room_R", "room_A", "room_B", "next_room", "exit_dir", "pal_slots", "word_6140", "word_6146", "word_68ec", "word_68f0", "word_922e", "word_922a", "floor_ptrs", "kid_ctrl1_saved", "word_5cd0", "word_5cda", "word_5cdc", "minutes_left", "clock_ticks", "word_5cc0", NULL};
	static const char *const start_regions[] = {"Kid", "Char", "chars", "level", "trobs", "trob_count", "mobs", "mob_count", "drawn_room", "next_room", "kid_ctrl1_saved", "coll", "knock", "word_6142", "word_6146", "word_8a84",
		"word_5d36", "byte_5cbb", "word_5cbe", "word_8604", "start_hp", "byte_5cba", "floor_ptrs", NULL};
	static const char *const between_regions[] = {"Kid", "chars", "level", "trobs", "trob_count", "mobs", "mob_count", "drawn_room", "next_room", "tick", "word_5d38", "word_5cda", "word_5cdc", "word_5cd0", "minutes_left", "clock_ticks",
		"word_2b90", "word_5cee", "word_5cce", "word_922a", "word_5d36", "word_5ce8", "floor_ptrs", NULL};
	const char *const *regions = tick_mode ? tick_regions : chars_mode ? chars_regions : start_mode ? start_regions : between_mode ? between_regions : room_regions;
	int only = getenv("CASE") ? atoi(getenv("CASE")) : 0;
	while (fread(&frame, 4, 1, f) == 1 && fread(&size, 4, 1, f) == 1 && size <= SNAP_MAX && (SNAP_SIZE = size, SNAP_BASE = 0x6C00 - size, 1) && fread(a, 1, SNAP_SIZE, f) == SNAP_SIZE && fread(b, 1, SNAP_SIZE, f) == SNAP_SIZE && (!tick_mode || (fread(kctl, 1, 8, f) == 8 && fread(kc1, 1, 16, f) == 16))) {
		n++; stubs_reset();
		{ const char_type *ak = (const char_type *)(a + 0x5B36 - SNAP_BASE), *bk = (const char_type *)(b + 0x5B36 - SNAP_BASE); sound_busy = bk->alive >= 0 && bk->alive == (ak->alive < 0 ? 0 : ak->alive); }
		if (hf && (fread(&hsize, 4, 1, hf) != 1 || hsize > sizeof heap || fread(heap, 1, hsize, hf) != hsize)) hsize = 0;
		if (hf && prev_ok && hsize && !memcmp(prev_ptrs, a + 0x2B6C - SNAP_BASE, 8) && snap_diff_heap(prev_ptrs, (const uint8_t (*)[0x65])prev_objs, heap, hsize, 0)) {   /* last tick's objects against this tick's start (when it follows it: same slots) */
			bad++; if (!only || only == prev_n) { printf("case %d (frame %u): collapsing-floor objects\n", prev_n, prev_frame); snap_diff_heap(prev_ptrs, (const uint8_t (*)[0x65])prev_objs, heap, hsize, 1); }
		}
		prev_ok = 0;
		snap_load(a); if (hsize) snap_load_heap(heap, hsize); stubs_select_guard_dat(level.type); coll_debug = only == n;
		uint8_t dr0 = drawn_room; int8_t nch = room_nchars(drawn_room);
		if (tick_mode) { if (tick_main() == -1) { skipped++; continue; } busy += drawn_room != dr0; }
		else if (start_mode) { if (a[0x5AB2 - SNAP_BASE] | a[0x5AB3 - SNAP_BASE] | a[0x5AB4 - SNAP_BASE] | a[0x5AB5 - SNAP_BASE]) { printf("case %d: checkpoint in far memory, skipped\n", n); skipped++; continue; } level_begin(); }
		else if (between_mode) { if (tick_tail() == -1 || frame_end() == -1) { skipped++; continue; } frame_begin(); }
		else if (chars_mode) { if (nch > 0) busy++; play_all_chars(); }
		else { check_kid_left_room(); switch_room(); if (drawn_room != dr0) busy++; }
		memcpy(got, a, SNAP_SIZE); snap_store(got);
		memcpy(prev_objs, floor_objs, sizeof prev_objs); memcpy(prev_ptrs, b + 0x2B6C - SNAP_BASE, 8); prev_ok = SNAP_BASE <= 0x2B6C; prev_n = n; prev_frame = frame;
		int d = snap_diff(got, b, regions, 0);
		if (d) { bad++; if (!only || only == n) { printf("case %d (frame %u): room %u, %d chars [%s]\n", n, frame, dr0, nch, stubs_log()); snap_diff(got, b, regions, 1); } }
	}
	printf("%s: %d ticks (%d %s, %d restart ticks skipped), %d mismatching\n", argv[1], n, busy, chars_mode ? "with characters" : "room changes", skipped, bad); return bad != 0;
}
