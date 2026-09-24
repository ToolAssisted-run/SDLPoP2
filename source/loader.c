/* The program's files: resource files, TXT4 texts, CONFIG.DAT and the game's own files (see loader.h). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "loader.h"
#include "dat.h"
#include "glue.h"

/* ---- resource files (2797:01D4 / 01B6) ---- */
#define RES_MAX 12
static struct { char name[16]; dat_file d; int open; } files[RES_MAX];
static int order[RES_MAX], norder;   /* open files, oldest first */

int res_open(const char *name)
{
	for (int i = 0; i < norder; i++) if (!strcmp(files[order[i]].name, name)) return 1;   /* 2797:0204: already open */
	int k = -1;
	for (int i = 0; i < RES_MAX; i++) if (!files[i].open) { k = i; break; }
	if (k < 0) return 0;
	if (!dat_open(&files[k].d, game_path(name))) return 0;   /* 194C:717E; else [DS:1F36] 1286:0CD6: "Couldn't find resource file" */
	snprintf(files[k].name, sizeof files[k].name, "%s", name); files[k].open = 1; order[norder++] = k;
	return 1;
}
void res_close(const char *name)
{
	for (int i = 0; i < norder; i++)
		if (!strcmp(files[order[i]].name, name)) {
			int k = order[i]; free(files[k].d.data); files[k].d.data = NULL; files[k].open = 0;
			memmove(order + i, order + i + 1, (norder - i - 1) * sizeof *order); norder--; return;
		}
}
void res_close_all(void) { while (norder) res_close(files[order[norder - 1]].name); }
static void file_tag(const char *tag, char out[4]) { for (int i = 0; i < 4; i++) out[i] = tag[3 - i]; }
const uint8_t *res_get(const char *tag, uint16_t id, uint16_t *size)
{
	char t[4]; if (tag && *tag) file_tag(tag, t);
	for (int i = norder - 1; i >= 0; i--) {
		const uint8_t *p = dat_find(&files[order[i]].d, tag && *tag ? t : NULL, id, size);
		if (p) { if (size) (*size)++; return p; }   /* (dat_find's size is one short: an entry is a checksum byte and `size` bytes) */
	}
	return NULL;
}
const uint8_t *res_get_in(const char *file, const char *tag, uint16_t id, uint16_t *size)
{
	char t[4]; if (tag && *tag) file_tag(tag, t);
	for (int i = 0; i < norder; i++) if (!strcmp(files[order[i]].name, file)) { const uint8_t *p = dat_find(&files[order[i]].d, tag && *tag ? t : NULL, id, size); if (p && size) (*size)++; return p; }
	return NULL;
}

/* ---- TXT4 (194C:8652) ---- */
extern const uint8_t *ds_ptr(uint16_t a);   /* glue.c: the static data segment */
int txt4_decode(const uint8_t *res, size_t size, char *out, size_t cap)
{
	if (size < 3) return -1;
	unsigned n = res[0] | res[1] << 8, esc = (res[2] >> 6) + 5, w = 4, len = 0;
	const uint8_t *bits = res + 2, *tab = ds_ptr(0x2500);   /* DS:2500: " aetonisrdlhugfcwypbmk,vSA.T'PMx..." (0 = escape) */
	size_t pos = 2, nbits = (size - 2) * 8;   /* the top two bits of the first byte hold the escape width */
	while (len < n) {
		if (pos + w > nbits) return -1;
		unsigned v = 0;
		for (unsigned i = 0; i < w; i++, pos++) v = v << 1 | ((bits[pos >> 3] >> (7 - (pos & 7))) & 1);
		if (v != 0 || w != 4) { if (len < cap) out[len] = (char)(w == 8 ? v : tab[v]); len++; w = 4; }
		else w = esc;   /* code 0: the next code is esc bits wide (8: a raw byte) */
	}
	if (len < cap) out[len] = 0;
	return (int)len;
}
static struct { uint16_t id; char *s; uint16_t len; } txt_cache[64]; static int ntxt;
const char *txt4_get(uint16_t id, uint16_t *len)
{
	for (int i = 0; i < ntxt; i++) if (txt_cache[i].id == id) { if (len) *len = txt_cache[i].len; return txt_cache[i].s; }
	uint16_t size; const uint8_t *p = res_get("TXT4", id, &size);
	if (!p || size < 2) return NULL;
	unsigned n = p[0] | p[1] << 8; char *s = calloc(1, n + 2);
	if (txt4_decode(p, size, s, n + 1) < 0) { free(s); return NULL; }
	if (ntxt < 64) { txt_cache[ntxt].id = id; txt_cache[ntxt].s = s; txt_cache[ntxt].len = n; ntxt++; }
	if (len) *len = n;
	return s;
}
const char *txt4_string(uint16_t id, int n)
{
	uint16_t len; const char *s = txt4_get(id, &len);
	if (!s || n < 1 || n > (uint8_t)s[0]) return NULL;   /* 194C:8580 */
	s++;
	while (--n) s += strlen(s) + 1;   /* 194C:855C */
	return s;
}

/* ---- CONFIG.DAT ---- */
int config_load(pop2_config *c)
{
	memset(c, 0, sizeof *c);
	FILE *f = fopen(game_path("CONFIG.DAT"), "rb"); if (!f) return 0;
	uint8_t b[32]; size_t n = fread(b, 1, 32, f); fclose(f);
	for (size_t i = 0; i + 1 < n; i += 2) c->w[i / 2] = (int16_t)(b[i] | b[i + 1] << 8);
	return 1;
}

/* ---- the game's own files (in the game directory, as DOS has them, unless file_dir is set) ---- */
char file_dir[400];
static const char *file_path(const char *name)
{
	static char p[512];
	if (!file_dir[0]) return game_path(name);
	snprintf(p, sizeof p, "%s/%s", file_dir, name); return p;
}
long file_size(const char *name)
{
	FILE *f = fopen(file_path(name), "rb"); if (!f) return -1;
	fseek(f, 0, SEEK_END); long n = ftell(f); fclose(f); return n;
}
long file_read(const char *name, long off, void *buf, long n)
{
	FILE *f = fopen(file_path(name), "rb"); if (!f) return -1;
	long r = fseek(f, off, SEEK_SET) == 0 ? (long)fread(buf, 1, n, f) : 0; fclose(f); return r;
}
int file_write_at(const char *name, long off, const void *buf, long n, int create)
{
	FILE *f = fopen(file_path(name), "r+b");
	if (!f && create) f = fopen(file_path(name), "w+b");
	if (!f) return 0;
	fseek(f, 0, SEEK_END); long end = ftell(f);
	while (end < off) { fputc(0, f); end++; }   /* DOS extends the file on a write past its end */
	int ok = fseek(f, off, SEEK_SET) == 0 && (long)fwrite(buf, 1, n, f) == n;
	fclose(f); return ok;
}
int file_create(const char *name, const void *buf, long n)
{
	FILE *f = fopen(file_path(name), "wb"); if (!f) return 0;
	int ok = (long)fwrite(buf, 1, n, f) == n; fclose(f); return ok;
}
