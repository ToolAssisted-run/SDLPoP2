/* Minimal PoP2 DAT reader (index of typed tables, 11-byte entries, 1 checksum byte before each body). */
#include "dat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int dat_open(dat_file *d, const char *path)
{
	FILE *f = fopen(path, "rb"); if (!f) return 0;
	fseek(f, 0, SEEK_END); d->size = ftell(f); fseek(f, 0, SEEK_SET);
	d->data = malloc(d->size); if (fread(d->data, 1, d->size, f) != d->size) { fclose(f); return 0; }
	fclose(f); return 1;
}
static uint16_t rd16(const uint8_t *p) { return p[0] | (p[1] << 8); }
static uint32_t rd32(const uint8_t *p) { return rd16(p) | ((uint32_t)rd16(p + 2) << 16); }
/* tag as 4 chars in file order (stored reversed, e.g. "SQES" for SEQS); pass the file-order tag or NULL for the untyped table */
const uint8_t *dat_find(const dat_file *d, const char *tag, uint16_t id, uint16_t *size)
{
	uint32_t to = rd32(d->data); uint16_t ntypes = rd16(d->data + to);
	for (unsigned t = 0; t < ntypes; t++) {
		const uint8_t *te = d->data + to + 2 + t * 6;
		int untyped = te[0] == 0 && te[1] == 0 && te[2] == 0 && te[3] == 0;
		if (tag ? memcmp(te, tag, 4) != 0 : !untyped) continue;
		uint32_t off = to + rd16(te + 4); uint16_t cnt = rd16(d->data + off);
		for (unsigned i = 0; i < cnt; i++) {
			const uint8_t *e = d->data + off + 2 + i * 11;
			if (rd16(e) == id) { if (size) *size = rd16(e + 6) - 1; return d->data + rd32(e + 2) + 1; }
		}
	}
	return NULL;
}
