/* Room transitions: from a DS snapshot taken before 2D3E:108A (check_kid_left_room) run it and 0823:0E72
 * (switch_room), then compare with the snapshot taken after (169B:064F). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/types.h"
#include "../src/globals.h"
#include "../src/dat.h"
#include "stubs.h"
#include "snap.h"
int main(int argc, char **argv)
{
	if (argc < 5) { fprintf(stderr, "usage: roomtest SEQUENCE.DAT ram.bin PRINCE.EXE pairs.bin\n"); return 2; }
	stubs_init(argv[1], "/dev/null"); stubs_load_frame_tables(argv[3]);
	static uint8_t ram[655360]; FILE *rf = fopen(argv[2], "rb"); if (!rf || fread(ram, 1, sizeof ram, rf) != sizeof ram) return 2; fclose(rf); stubs_load_ds_tables(ram);
	level_roomlinks = (uint8_t *)&level + 0x17BC;
	FILE *f = fopen(argv[4], "rb"); if (!f) return 2;
	static uint8_t a[SNAP_SIZE], b[SNAP_SIZE], got[SNAP_SIZE]; uint32_t frame; int n = 0, bad = 0, moves = 0;
	static const char *const regions[] = {"Kid", "chars", "level", "drawn_room", "room_L", "room_R", "room_A", "room_B", "room_AL", "room_AR", "room_BL", "room_BR", "next_room", "exit_dir", "pal_slots", "word_922a", NULL};
	while (fread(&frame, 4, 1, f) == 1 && fread(a, 1, SNAP_SIZE, f) == SNAP_SIZE && fread(b, 1, SNAP_SIZE, f) == SNAP_SIZE) {
		n++; stubs_reset(); snap_load(a);
		uint8_t dr0 = drawn_room;
		check_kid_left_room(); switch_room();
		if (drawn_room != dr0) moves++;
		memcpy(got, a, SNAP_SIZE); snap_store(got);
		int verbose = getenv("CASE") ? atoi(getenv("CASE")) == n : 1;
		int d = snap_diff(got, b, regions, 0);
		if (d) { bad++; if (verbose) { printf("case %d (frame %u): room %u -> %u (exp %u) exit %d [%s]\n", n, frame, dr0, drawn_room, b[0x5CDE - SNAP_BASE], exit_dir, stubs_log()); snap_diff(got, b, regions, 1); } }
	}
	printf("%d ticks (%d room changes), %d mismatching\n", n, moves, bad); return bad != 0;
}
