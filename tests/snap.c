#include <stdio.h>
#include <string.h>
#include "../src/types.h"
#include "../src/globals.h"
#include "snap.h"
int SNAP_BASE = 0x2B00, SNAP_SIZE = 0x4100;
static int in_snap(int i);
extern int16_t image_height, image_width;
/* one table drives load, store and diff: DS offset, size, address of the C global */
struct field { const char *name; uint16_t ds; uint16_t size; void *p; };
extern uint16_t word_5cd8, word_6142, word_6146;
static struct field fields[] = {
	{"level", 0x2BB8, sizeof(level_type), &level}, {"tiles0", 0x2B9A, 30, tiles0}, {"coll", 0x2B24, 0x44, &coll},
	{"Char", 0x5AB6, 64, &Char}, {"Opp", 0x5AF6, 64, &Opp}, {"Kid", 0x5B36, 64, &Kid}, {"chars", 0x5B76, 320, chars},
	{"word_8604", 0x5CC4, 2, &word_8604}, {"cur_frame", 0x5CC6, 7, &cur_frame}, {"control_x", 0x5CD4, 1, &control_x}, {"control_y", 0x5CD5, 1, &control_y}, {"control_shift", 0x5CD6, 1, &control_shift},
	{"word_5cd8", 0x5CD8, 2, &word_5cd8}, {"drawn_room", 0x5CDE, 1, &drawn_room},
	{"room_L", 0x5CDF, 1, &room_L}, {"room_R", 0x5CE0, 1, &room_R}, {"room_A", 0x5CE1, 1, &room_A}, {"room_B", 0x5CE2, 1, &room_B},
	{"room_AL", 0x5CE3, 1, &room_AL}, {"room_AR", 0x5CE4, 1, &room_AR}, {"room_BL", 0x5CE5, 1, &room_BL}, {"room_BR", 0x5CE6, 1, &room_BR},
	{"counter_5cec", 0x5CEC, 2, &counter_5cec}, {"tick", 0x5D04, 4, &tick}, {"pal_slots", 0x5D08, 2, pal_slots},
	{"obj_x", 0x60FC, 2, &obj_x}, {"obj_y", 0x60FE, 2, &obj_y}, {"obj_id", 0x6100, 2, &obj_id}, {"obj_chtab", 0x6102, 1, &obj_chtab},
	{"image_height", 0x6112, 2, &image_height}, {"image_width", 0x6114, 2, &image_width}, {"char_x_left", 0x6116, 2, &char_x_left}, {"char_x_right", 0x6118, 2, &char_x_right},
	{"char_x_left_coll", 0x611A, 2, &char_x_left_coll}, {"char_x_right_coll", 0x611C, 2, &char_x_right_coll}, {"char_top_y", 0x611E, 2, &char_top_y},
	{"ctrl1_forward", 0x6122, 1, &ctrl1_forward}, {"ctrl1_backward", 0x6123, 1, &ctrl1_backward}, {"ctrl1_up", 0x6124, 1, &ctrl1_up}, {"ctrl1_down", 0x6125, 1, &ctrl1_down}, {"ctrl1_shift", 0x6126, 1, &ctrl1_shift},
	{"curr_tile", 0x612E, 1, &curr_tile}, {"curr_room", 0x6132, 1, &curr_room}, {"tile_col", 0x6133, 1, &tile_col}, {"tile_row", 0x6134, 1, &tile_row},
	{"char_col_left", 0x6135, 1, &char_col_left}, {"char_col_right", 0x6136, 1, &char_col_right}, {"char_top_row", 0x6137, 1, &char_top_row}, {"char_bottom_row", 0x6138, 1, &char_bottom_row},
	{"knock", 0x613E, 2, &knock}, {"word_6140", 0x6140, 2, &word_6140}, {"word_6142", 0x6142, 2, &word_6142}, {"word_8a84", 0x6144, 2, &word_8a84}, {"word_6146", 0x6146, 2, &word_6146},
	{"word_922a", 0x68EA, 2, &word_922a}, {"word_922e", 0x68EE, 2, &word_922e}, {"word_68f0", 0x68F0, 2, &word_68f0}, {"exit_dir", 0x68F2, 2, &exit_dir},
	{"byte_9276", 0x6936, 1, &byte_9276}, {"byte_6937", 0x6937, 1, &byte_6937}, {"byte_2b68", 0x2B68, 1, &byte_2b68}, {"prev_coll_flags", 0x6948, 10, prev_coll_flags}, {"curr_row_coll_flags", 0x6952, 10, curr_row_coll_flags},
	{"next_room", 0x6B6D, 1, &next_room}, {"random_seed", 0x2B7A, 4, &random_seed}, {"word_68ec", 0x68EC, 2, &word_68ec}, {"byte_5cba", 0x5CBA, 1, &byte_5cba}, {"mob_count", 0x6186, 2, &mob_count}, {"mobs", 0x293E, 390, mobs}, {"trob_count", 0x6670, 2, &trob_count}, {"trobs", 0x6676, 80, trobs}, {"word_5ce8", 0x5CE8, 2, &word_5ce8}, {"floor_ptrs", 0x2B6C, 8, floor_ptrs}, {"kid_ctrl1_saved", 0x6128, 5, kid_ctrl1_saved}, {"word_5d38", 0x5D38, 2, &word_5d38},
	{"word_2b96", 0x2B96, 2, &word_2b96}, {"puzzle_answer", 0x2B6A, 1, &puzzle_answer}, {"puzzle_last", 0x2B6B, 1, &puzzle_last}, {"minutes_left", 0x5CD2, 2, &minutes_left}, {"word_2b90", 0x2B90, 2, &word_2b90}, {"word_2b92", 0x2B92, 2, &word_2b92}, {"word_5cee", 0x5CEE, 2, &word_5cee}, {"word_5cce", 0x5CCE, 2, &word_5cce}, {"byte_6b6c", 0x6B6C, 1, &byte_6b6c}, {"clock_ticks", 0x5CEA, 2, &clock_ticks}, {"word_5cb6", 0x5CB6, 2, &word_5cb6}, {"word_5cc0", 0x5CC0, 2, &word_5cc0}, {"start_hp", 0x6B71, 1, &start_hp}, {"word_5d36", 0x5D36, 2, &word_5d36}, {"byte_5cbb", 0x5CBB, 1, &byte_5cbb}, {"word_5cbe", 0x5CBE, 2, &word_5cbe}, {"word_5cd0", 0x5CD0, 2, &word_5cd0}, {"word_5cda", 0x5CDA, 2, &word_5cda}, {"word_5cdc", 0x5CDC, 2, &word_5cdc},
};
#define NF (int)(sizeof fields / sizeof fields[0])
void snap_load(const uint8_t *ds)
{
	for (int i = 0; i < NF; i++) if (in_snap(i)) memcpy(fields[i].p, ds + fields[i].ds - SNAP_BASE, fields[i].size);
	curr_modifier = ds[0x612F - SNAP_BASE] | ds[0x6130 - SNAP_BASE] << 8;
	{ int t = (ds[0x613C - SNAP_BASE] | ds[0x613D - SNAP_BASE] << 8) - 0x2B9A, a = (ds[0x613A - SNAP_BASE] | ds[0x613B - SNAP_BASE] << 8) - 0x2F00;   /* DS:613C/613A room pointers */
	  if (t >= 0 && t < 33 * 30) curr_room_tiles = t < 30 ? tiles0 + t : (uint8_t *)&level + (t - 30);
	  if (a >= 0 && a < 33 * 0x78) curr_room_attrs = (uint32_t *)((uint8_t *)&level + 0x348 + a); }
	level_kind = level.hdr_pad2[4]; level_number = level.number; word_32d8 = counter_5cec;
}
void snap_store(uint8_t *ds) { for (int i = 0; i < NF; i++) if (in_snap(i)) memcpy(ds + fields[i].ds - SNAP_BASE, fields[i].p, fields[i].size); }
static int in_snap(int i) { return fields[i].ds >= SNAP_BASE && fields[i].ds + fields[i].size <= SNAP_BASE + SNAP_SIZE; }
/* character bytes written only by the draw pass (0993:1130 -> 1375:0F5A/1020: +0x36 = redraw because another sprite
 * overlaps; the game draws inside the captured window) */
static int draw_only(int i, int o) { int k = strcmp(fields[i].name, "chars") ? (!strcmp(fields[i].name, "Char") || !strcmp(fields[i].name, "Opp") || !strcmp(fields[i].name, "Kid") ? o : -1) : o % 64; return k == 0x36 || k == 0x37; }
int snap_diff(const uint8_t *got, const uint8_t *exp, const char *const *regions, int verbose)
{
	int bad = 0;
	for (const char *const *r = regions; *r; r++)
		for (int i = 0; i < NF; i++) if (!strcmp(fields[i].name, *r) && in_snap(i)) {
			const uint8_t *g = got + fields[i].ds - SNAP_BASE, *e = exp + fields[i].ds - SNAP_BASE; int d = 0;
			if (!strcmp(fields[i].name, "floor_ptrs")) {   /* near-heap addresses: only in use / free is comparable */
				for (int w = 0; w < 4; w++) if (!(g[2 * w] | g[2 * w + 1]) != !(e[2 * w] | e[2 * w + 1])) { if (verbose) printf("  floor_ptrs: slot %d in use %d!=%d\n", w, !!(g[2 * w] | g[2 * w + 1]), !!(e[2 * w] | e[2 * w + 1])); bad++; }
				continue;
			}
			for (int o = 0; o < fields[i].size; o++) if (g[o] != e[o] && !draw_only(i, o)) { if (verbose && d < 24) { if (!d) printf("  %s:", fields[i].name); printf(" +%X:%02X!=%02X", o, g[o], e[o]); } d++; }
			if (d && verbose) printf("%s\n", d > 24 ? " ..." : "");
			bad += d != 0;
		}
	return bad;
}

/* collapsing-floor objects live in the near heap (DS:A8F4..): a probe window of the heap at HEAP_BASE */
#define HEAP_BASE 0xA800
void snap_load_heap(const uint8_t *heap, uint32_t size)
{
	for (int i = 0; i < 4; i++) { int o = floor_ptrs[i] - HEAP_BASE; if (floor_ptrs[i] && o >= 0 && o + 0x65 <= (int)size) memcpy(floor_objs[i], heap + o, 0x65); }
}
int snap_diff_heap(const uint16_t *exp_ptrs, const uint8_t (*objs)[0x65], const uint8_t *heap, uint32_t size, int verbose)
{
	int bad = 0;
	for (int i = 0; i < 4; i++) {
		int o = exp_ptrs[i] - HEAP_BASE;
		if (!exp_ptrs[i] || o < 0 || o + 0x65 > (int)size) continue;
		for (int k = 0; k < 0x65; k++) if (objs[i][k] != heap[o + k]) { if (verbose) printf("  floor_obj[%d]: +%X:%02X!=%02X\n", i, k, objs[i][k], heap[o + k]); bad++; break; }
	}
	return bad;
}
/* the field holding DS address a (NULL outside every field) */
const char *snap_field_at(uint16_t a, int *off)
{
	for (int i = 0; i < NF; i++) if (a >= fields[i].ds && a < fields[i].ds + fields[i].size) { *off = a - fields[i].ds; return fields[i].name; }
	return NULL;
}
