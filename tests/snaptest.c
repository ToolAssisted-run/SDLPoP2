/* Snapshot equivalence tests: load the whole-DS snapshot taken before a phase of the tick (169B:05E0), run the
 * reconstructed phase, compare with the snapshot taken after it.
 *   room : 2D3E:108A check_kid_left_room + 0823:0E72 switch_room   (pairs ds_preleave -> ds_postroom)
 *   chars: 169B:07EC play_all_chars                                (pairs ds_prechars -> ds_postchars) */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/types.h"
#include "../src/globals.h"
#include "../src/dat.h"
#include "stubs.h"
#include "snap.h"
extern int coll_debug;
int main(int argc, char **argv)
{
	if (argc < 6) { fprintf(stderr, "usage: snaptest room|chars SEQUENCE.DAT ram.bin PRINCE.EXE pairs.bin\n"); return 2; }
	int chars_mode = !strcmp(argv[1], "chars");
	stubs_init(argv[2], "/dev/null"); stubs_load_frame_tables(argv[4]);
	static uint8_t ram[655360]; FILE *rf = fopen(argv[3], "rb"); if (!rf || fread(ram, 1, sizeof ram, rf) != sizeof ram) return 2; fclose(rf); stubs_load_ds_tables(ram);
	level_roomlinks = (uint8_t *)&level + 0x17BC;
	FILE *f = fopen(argv[5], "rb"); if (!f) return 2;
	static uint8_t a[SNAP_SIZE], b[SNAP_SIZE], got[SNAP_SIZE]; uint32_t frame; int n = 0, bad = 0, busy = 0;
	static const char *const room_regions[] = {"Kid", "chars", "level", "drawn_room", "room_L", "room_R", "room_A", "room_B", "room_AL", "room_AR", "room_BL", "room_BR", "next_room", "exit_dir", "pal_slots", "word_922a", NULL};
	static const char *const chars_regions[] = {"chars", "Kid", "level", "random_seed", "drawn_room", "room_L", "room_R", "word_6140", "word_6146", "word_68ec", "word_68f0", "word_922e", NULL};
	const char *const *regions = chars_mode ? chars_regions : room_regions;
	int only = getenv("CASE") ? atoi(getenv("CASE")) : 0;
	while (fread(&frame, 4, 1, f) == 1 && fread(a, 1, SNAP_SIZE, f) == SNAP_SIZE && fread(b, 1, SNAP_SIZE, f) == SNAP_SIZE) {
		n++; stubs_reset(); snap_load(a); coll_debug = only == n;
		uint8_t dr0 = drawn_room; int8_t nch = room_nchars(drawn_room);
		if (chars_mode) { if (nch > 0) busy++; play_all_chars(); }
		else { check_kid_left_room(); switch_room(); if (drawn_room != dr0) busy++; }
		memcpy(got, a, SNAP_SIZE); snap_store(got);
		int d = snap_diff(got, b, regions, 0);
		if (d) { bad++; if (!only || only == n) { printf("case %d (frame %u): room %u, %d chars [%s]\n", n, frame, dr0, nch, stubs_log()); snap_diff(got, b, regions, 1); } }
	}
	printf("%s: %d ticks (%d %s), %d mismatching\n", argv[1], n, busy, chars_mode ? "with characters" : "room changes", bad); return bad != 0;
}
