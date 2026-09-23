/* rendertest RAM_BEFORE RAM_AFTER TABLES.hex DAT [OUT.pgm]: draw the back and fore tables (0FB3:0B78's view: the
 * 0x1450 bytes at 39E0:0000 as hex, then the counts) with render.c over the buffer of RAM_BEFORE and compare rows 0..191
 * with RAM_AFTER's buffer (phys 0x4CF22) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/types.h"
#include "../src/globals.h"
#include "../src/glue.h"
#include "../src/render.h"
static uint8_t *load(const char *p) { FILE *f = fopen(p, "rb"); if (!f) { perror(p); exit(2); } uint8_t *b = malloc(655360); if (fread(b, 1, 655360, f) != 655360) exit(2); fclose(f); return b; }
static int hexv(int c) { return c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10; }
int main(int argc, char **argv)
{
	if (argc < 5) { fprintf(stderr, "usage: rendertest RAM_BEFORE RAM_AFTER TABLES.hex DAT [OUT.pgm]\n"); return 2; }
	uint8_t *a = load(argv[1]), *b = load(argv[2]);
	glue_load_ds_tables(a);
	FILE *f = fopen(argv[3], "r"); static char hex[0x3000 * 2 + 16]; if (!f || !fgets(hex, sizeof hex, f)) return 2; fclose(f);
	static uint8_t tb[0x1460]; for (int i = 0; i < 0x1450 + 10 && hex[2 * i] && hex[2 * i + 1] > ' '; i++) tb[i] = (uint8_t)(hexv(hex[2 * i]) << 4 | hexv(hex[2 * i + 1]));
	memcpy(back_table, tb, sizeof back_table); memcpy(fore_table, tb + 0xA28, sizeof fore_table);
	for (int i = 0; i < 5; i++) table_counts[i] = tb[0x1450 + 2 * i] | tb[0x1451 + 2 * i] << 8;
	render_set_chtab(4, argv[4], 3500, 0x40);
	memcpy(screen_buf, a + 0x4CF22, sizeof screen_buf);
	render_draw_table(0); render_draw_table(1);
	int same = 0, diff = 0, x0 = 320, x1 = -1, y0 = 200, y1 = -1;
	for (int i = 0; i < 320 * 192; i++) { if (screen_buf[i] == b[0x4CF22 + i]) same++; else { diff++; int x = i % 320, y = i / 320; if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y; } }
	printf("tables %d/%d: %d same, %d differ (box %d..%d x %d..%d)\n", table_counts[0], table_counts[1], same, diff, x0, x1, y0, y1);
	if (argc > 5) { FILE *o = fopen(argv[5], "wb"); fprintf(o, "P5 320 200 255\n"); fwrite(screen_buf, 1, sizeof screen_buf, o); fclose(o); }
	return diff != 0;
}
