/* tiletest CASE RAM LEVEL_DAT: a case of tools/tilecap.py (line 1: the DS state 0x2900..0x6C00 when 0FB3:0122 starts a
 * whole-room build, line 2: the tables the game then drew, 0x1450 bytes of 39E0:0000 and the counts, as hex); RAM, a
 * dump of the same run (the static data segment); builds the tables with render_tiles.c (DS:610E set) and compares
 * them entry by entry */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/types.h"
#include "../src/globals.h"
#include "../src/glue.h"
#include "../src/dat.h"
#include "../src/render.h"
#include "../src/render_tiles.h"
#include "snap.h"
static uint8_t *load(const char *p) { FILE *f = fopen(p, "rb"); if (!f) { perror(p); exit(2); } uint8_t *b = malloc(655360); if (fread(b, 1, 655360, f) != 655360) exit(2); fclose(f); return b; }
static int hexv(int c) { return c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10; }
static void show(const char *what, const draw_entry *e) { const uint8_t *b = (const uint8_t *)e; printf("  %s", what); for (int i = 0; i < 20; i++) printf(" %02x", b[i]); printf("\n"); }
int main(int argc, char **argv)
{
	if (argc < 4) { fprintf(stderr, "usage: tiletest CASE RAM LEVEL_DAT\n"); return 2; }
	uint8_t *ram = load(argv[2]);
	glue_load_ds_tables(ram);
	FILE *cf = fopen(argv[1], "r"); static char line1[0x4300 * 2 + 16], hex[0x3000 * 2 + 16], hl[0x2000 * 2 + 16]; if (!cf || !fgets(line1, sizeof line1, cf) || !fgets(hex, sizeof hex, cf)) return 2; if (!fgets(hl, sizeof hl, cf)) hl[0] = 0; fclose(cf);
	static uint8_t st[0x4300]; for (int i = 0; i < 0x4300; i++) st[i] = (uint8_t)(hexv(line1[2 * i]) << 4 | hexv(line1[2 * i + 1]));
	SNAP_BASE = 0x2900; SNAP_SIZE = 0x4300; snap_load(st);
	static uint8_t heap[0x2000]; int nheap = (int)strlen(hl) / 2; if (nheap > 0x2000) nheap = 0x2000;   /* (line 3: DS:9800.., the collapsing floors' objects and the piece table) */
	for (int i = 0; i < nheap; i++) heap[i] = (uint8_t)(hexv(hl[2 * i]) << 4 | hexv(hl[2 * i + 1]));
	if (nheap == 0x2000) snap_load_heap(heap + 0x1000, 0x1000);
	level_roomlinks = (uint8_t *)&level + 0x17BC; level_number = ((uint8_t *)&level)[0x1847]; level_kind = st[0x43FD - 0x2900];
	for (int i = 0; i < 4; i++) screen_rect[i] = (int16_t)ds_word(0x097E + 2 * i);
	static dat_file d; char p[512]; snprintf(p, sizeof p, "%s/%s", getenv("PRINCE2_DIR"), argv[3]); if (!dat_open(&d, p)) return 2;
	uint16_t n; const uint8_t *pc = dat_find(&d, "CEIP", 3500, &n); static uint8_t pcopy[0x1000]; if (pc) memcpy(pcopy, pc, n < sizeof pcopy ? n : sizeof pcopy); piece_table = pcopy;
	{ uint16_t pt = (uint16_t)(ram[0x3B250 + 0x1090] | ram[0x3B251 + 0x1090] << 8);   /* DS:[0x1090]: the game's piece table, which some drawers rewrite */
	  if (nheap == 0x2000 && pt >= 0x9800 && pt + 0x13 * 0x3D <= 0xB800) {
	    int diff = 0; for (int i = 0; i < 0x13 * 0x3D; i++) diff += heap[pt - 0x9800 + i] != pcopy[i];
	    if (diff < 0x40) memcpy(pcopy, heap + (pt - 0x9800), 0x13 * 0x3D);   /* (the pointer is from the end of the run: only when it still holds the table, a few pieces rewritten) */
	  } }
	render_set_chtab(4, argv[3], 3500, 0x40); render_set_chtab(0, "PRINCE.DAT", 1000, 0); render_set_chtab(1, "PRINCE.DAT", 3000, 0);
	tile_drawers = kind_drawers_for(level_kind);
	memset(table_counts, 0, sizeof table_counts); redraw_all_flag = 1;
	set_neighbour_rooms();
	word_2ba6 = (uint16_t)(st[0x2BA6 - 0x2900] | st[0x2BA7 - 0x2900] << 8);
	byte_5ce7 = (uint8_t)room_has_description(drawn_room); room_bg = 0; if (byte_5ce7) { int16_t bg = room_description_bg(drawn_room); if (bg >= 0) room_bg = bg + 1; }
	render_desc_load(drawn_room);
	draw_room_tiles();
	render_sort_tables();
	static uint8_t tb[0x1460]; for (int i = 0; i < 0x1450 + 10; i++) tb[i] = (uint8_t)(hexv(hex[2 * i]) << 4 | hexv(hex[2 * i + 1]));
	int bad = 0;
	for (int t = 0; t < 2; t++) {
		int want = tb[0x1450 + 2 * t] | tb[0x1451 + 2 * t] << 8, got = table_counts[t];
		const draw_entry *exp = (const draw_entry *)(tb + (t ? 0xA28 : 0)), *mine = t ? fore_table : back_table;
		printf("table %d: %d entries, game %d\n", t, got, want);
		for (int i = 0; i < (got > want ? got : want); i++)
			if (i >= got || i >= want || memcmp(&exp[i], &mine[i], 20)) { if (bad++ < 12) { printf(" #%d\n", i); if (i < want) show("game", &exp[i]); if (i < got) show("mine", &mine[i]); } }
	}
	printf("%d entries differ; missing: %s\n", bad, missing_log());
	return bad != 0;
}
