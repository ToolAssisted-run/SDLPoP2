#include <stdio.h>
#include <string.h>
#include "../src/types.h"
#include "../src/globals.h"
#include "snap.h"
int SNAP_BASE = 0x2B00, SNAP_SIZE = 0x4100;
static int in_snap(int i);
extern int16_t image_height, image_width;
#include "../src/state.h"
#define fields snap_fields
#define NF snap_nfields
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
