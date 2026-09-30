/* The three DOS releases (version.h, docs/VERSIONS.md): which one PRINCE.EXE is, and its static data segment in 1.1's
 * layout. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "version.h"
#include "version_maps.inc"

int game_ver = POP2_VER_11, exe_ver = POP2_VER_11;
uint32_t exe_data_base = 0x3A500;

const char *version_name(int v) { return v == POP2_VER_IR ? "IR" : v == POP2_VER_10 ? "1.0" : "1.1"; }

static const uint8_t *find(const uint8_t *h, size_t n, const char *s)
{
	size_t k = strlen(s);
	for (size_t i = 0; i + k <= n; i++) if (h[i] == (uint8_t)s[0] && !memcmp(h + i, s, k)) return h + i;
	return NULL;
}
int version_of_exe(const uint8_t *exe, size_t n, uint32_t *data_base)
{
	if (n < 0x30000 || exe[0] != 'M' || exe[1] != 'Z') return -1;
	const uint8_t *ds = find(exe, n, "MS Run-Time Library");   /* DS:8, the C runtime's banner */
	if (!ds || ds - exe < 0x2948) return -1;
	int v = find(exe, n, "PRINCE OF PERSIA 2 v1.1") ? POP2_VER_11 : find(exe, n, "PRINCE OF PERSIA 2 1.0") ? POP2_VER_10 :
	        find(exe, n, "PRINCE OF PERSIA 2") ? POP2_VER_IR : -1;
	if (data_base) *data_base = (uint32_t)(ds - exe) - 8 - 0x2940;
	return v;
}

static const int16_t (*anchors(int v, int back))[2]
{
	if (v == POP2_VER_IR) return back ? ds_from_ir : ds_to_ir;
	if (v == POP2_VER_10) return back ? ds_from_v10 : ds_to_v10;
	return NULL;
}
static uint16_t through(const int16_t (*t)[2], uint16_t off)
{
	if (!t) return off;
	int d = 0;
	for (int i = 0; t[i][0] >= 0 && t[i][0] <= off; i++) d = t[i][1];
	return (uint16_t)(off + d);
}
uint16_t version_ds_to(int v, uint16_t off) { return off >= 0x27C0 ? off : through(anchors(v, 0), off); }
uint16_t version_ds_from(int v, uint16_t off) { return off >= 0x2930 ? off : through(anchors(v, 1), off); }

/* DS pointer tables (near pointers into the static data): their values follow the layout */
static const struct { uint16_t at, n; } ds_pointers[] = {
	{0x06BC, 11}, {0x06D2, 11},              /* the guard graphics' banks per level type (render.c, glue.c) */
	{0x07F2, 5}, {0x07FC, 5}, {0x0806, 5},   /* rects per level kind (render_kind_desc.c, render_sprites.c) */
	{0x091E, 7}, {0x092C, 7},                /* the ambient pieces' bases / counts per level kind (sound.c) */
	{0x15F0, 6},                             /* the caverns' piece lists (render_kind3.c, caverns.c) */
};
int version_load_exe(const char *path, uint8_t *ds11)
{
	FILE *f = fopen(path, "rb"); if (!f) return 0;
	fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
	uint8_t *exe = n > 0 ? malloc((size_t)n) : NULL;
	int ok = exe && fread(exe, 1, (size_t)n, f) == (size_t)n; fclose(f);
	uint32_t base = 0; int v = ok ? version_of_exe(exe, (size_t)n, &base) : -1;
	size_t len = v == POP2_VER_IR ? 0x2930 : 0x27C0;   /* (the data overlay ends the file: its last paragraph is short) */
	if (v < 0 || base + 0x2940 + 0x2000 > (size_t)n) { free(exe); return 0; }
	if (base + 0x2940 + len > (size_t)n) len = (size_t)n - base - 0x2940;
	exe_ver = v; exe_data_base = base;
	const uint8_t *ds = exe + base + 0x2940;
	if (v == POP2_VER_11) memcpy(ds11, ds, 0x27BF);
	else {
		for (unsigned u = 0; u < 0x27BF; u++) { uint16_t o = version_ds_to(v, (uint16_t)u); ds11[u] = o < len ? ds[o] : 0; }
		for (size_t t = 0; t < sizeof ds_pointers / sizeof ds_pointers[0]; t++)
			for (unsigned k = 0; k < ds_pointers[t].n; k++) {
				uint8_t *p = ds11 + ds_pointers[t].at + 2 * k; uint16_t w = (uint16_t)(p[0] | p[1] << 8);
				if (w) { w = version_ds_from(v, w); p[0] = (uint8_t)w; p[1] = (uint8_t)(w >> 8); }
			}
		memset(ds11 + 0x1F12, 0, 8);   /* 1.1's empty rect (1.0 has it elsewhere) */
	}
	free(exe);
	return 1;
}
const char *version_choose(int want)
{
	if (want == POP2_VER_AUTO) { game_ver = exe_ver; return NULL; }
	if ((want == POP2_VER_IR) != (exe_ver == POP2_VER_IR))
		return want == POP2_VER_IR ? "The initial release needs its own game files (these are 1.0 / 1.1's)."
		                           : "These are the initial release's game files: 1.0 and 1.1 need theirs.";
	game_ver = want; return NULL;
}
