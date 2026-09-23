/* Images (the SHAP-type resources of the DAT files). Header: height, width, flags words; flags bits 12..14 = bits per
 * pixel - 1, bits 8..11 = the packing: 0 raw, 1 RLE, 2 RLE by columns, 3 LZG, 4 LZG by columns. RLE: a signed count
 * byte, >= 0 copies count+1 bytes, < 0 repeats the next byte -count times. LZG: a flag byte per 8 items (low bit
 * first), 1 = a literal byte, 0 = a big-endian word, low 10 bits a position in a 1 KB window (starting zeroed, the
 * write position at 0x3BE) and the top 6 bits the length - 3. "By columns" fills the packed rows (stride bytes wide)
 * top to bottom, column by column. The unpacked rows are then 1..8 bits per pixel, high bits first. */
#include <stdlib.h>
#include <string.h>
#include "image.h"

static void put(uint8_t *dest, int *pos, int total, int by_cols, int stride, int height, uint8_t b)
{
	if (*pos >= total) return;
	int k = *pos;
	if (by_cols) { int col = k / height, row = k % height; dest[row * stride + col] = b; }
	else dest[k] = b;
	(*pos)++;
}
static void unpack_rle(uint8_t *dest, const uint8_t *src, const uint8_t *end, int total, int by_cols, int stride, int height)
{
	int pos = 0;
	while (pos < total && src < end) {
		int8_t c = (int8_t)*src++;
		if (c >= 0) for (int i = 0; i <= c && src < end; i++) put(dest, &pos, total, by_cols, stride, height, *src++);
		else { uint8_t b = src < end ? *src++ : 0; for (int i = 0; i < -c; i++) put(dest, &pos, total, by_cols, stride, height, b); }
	}
}
static void unpack_lzg(uint8_t *dest, const uint8_t *src, const uint8_t *end, int total, int by_cols, int stride, int height)
{
	uint8_t win[0x400]; memset(win, 0, sizeof win);
	int wpos = 0x400 - 0x42, pos = 0; unsigned mask = 0;
	while (pos < total && src < end) {
		mask >>= 1;
		if (!(mask & 0xFF00)) mask = *src++ | 0xFF00u;
		if (mask & 1) { if (src >= end) break; uint8_t b = *src++; win[wpos] = b; wpos = (wpos + 1) & 0x3FF; put(dest, &pos, total, by_cols, stride, height, b); }
		else {
			if (src + 1 >= end) break;
			unsigned w = src[0] << 8 | src[1]; src += 2;
			int from = w & 0x3FF, len = (w >> 10) + 3;
			for (int i = 0; i < len && pos < total; i++) { uint8_t b = win[from]; from = (from + 1) & 0x3FF; win[wpos] = b; wpos = (wpos + 1) & 0x3FF; put(dest, &pos, total, by_cols, stride, height, b); }
		}
	}
}
/* flags low byte 1 (the 8-bit tile and background images): a word, the unpacked length, then the packing of flags bits 8..11
 * over rows of runs: per row a word (the byte count of the row's packets), then packets: 0..0x7F copy n+1 bytes,
 * 0x80..0xFF repeat the next byte (n & 0x7F) + 1 times (the blitter 2583:0006 reads this form) */
static int image_decode_rows(const uint8_t *res, int size, image_t *img)
{
	int h = res[0] | res[1] << 8, w = res[2] | res[3] << 8, flags = res[4] | res[5] << 8, method = (flags >> 8) & 0xF;
	if (size < 8 || w <= 0 || h <= 0) return 0;
	int total = res[6] | res[7] << 8; uint8_t *rows = calloc(1, total + 2);
	const uint8_t *src = res + 8, *end = res + size;
	if (method == 3) unpack_lzg(rows, src, end, total, 0, 0, 0);
	else if (method == 1) unpack_rle(rows, src, end, total, 0, 0, 0);
	else if (method == 0) memcpy(rows, src, size - 8 < total ? size - 8 : total);
	else { free(rows); return 0; }
	img->width = w; img->height = h; img->depth = 8; img->flags = flags; img->pixels = calloc(1, w * h); img->clear = calloc(1, w * h);
	int p = 0;
	for (int y = 0; y < h && p + 2 <= total; y++) {
		int len = rows[p] | rows[p + 1] << 8, q = p + 2, x = 0; len += 2;   /* (the count excludes the word) */
		while (q < p + len && q < total && x < w) {
			uint8_t c = rows[q++];
			if (c & 0x80) { uint8_t v = rows[q++]; for (int i = 0; i <= (c & 0x7F) && x < w; i++) { img->clear[y * w + x] = v == 0; img->pixels[y * w + x++] = v; } }
			else for (int i = 0; i <= c && x < w && q < total; i++) img->pixels[y * w + x++] = rows[q++];
		}
		p += len ? len : 2;
	}
	free(rows);
	return 1;
}
int image_decode(const uint8_t *res, int size, image_t *img)
{
	if (size < 6) return 0;
	int h = res[0] | res[1] << 8, w = res[2] | res[3] << 8, flags = res[4] | res[5] << 8;
	if ((flags & 0xFF) == 1) return image_decode_rows(res, size, img);
	int depth = ((flags >> 12) & 7) + 1, method = (flags >> 8) & 0xF, stride = (depth * w + 7) / 8, total = stride * h;
	if (w <= 0 || h <= 0 || w > 4096 || h > 4096) return 0;
	uint8_t *packed = calloc(1, total ? total : 1);
	const uint8_t *src = res + 6, *end = res + size;
	switch (method) {
	case 0: memcpy(packed, src, size - 6 < total ? size - 6 : total); break;
	case 1: case 2: unpack_rle(packed, src, end, total, method == 2, stride, h); break;
	case 3: case 4: unpack_lzg(packed, src, end, total, method == 4, stride, h); break;
	default: free(packed); return 0;
	}
	img->width = w; img->height = h; img->depth = depth; img->flags = flags;
	img->pixels = malloc(w * h); img->clear = NULL;
	for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
		int bit = x * depth, byte = packed[y * stride + bit / 8], shift = 8 - depth - bit % 8;
		img->pixels[y * w + x] = (uint8_t)((byte >> shift) & ((1 << depth) - 1));
	}
	free(packed);
	return 1;
}
void image_free(image_t *img) { free(img->pixels); free(img->clear); img->pixels = img->clear = NULL; }
