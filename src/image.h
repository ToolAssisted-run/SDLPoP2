#pragma once
#include <stdint.h>
typedef struct image_t { int width, height, depth, flags; uint8_t *pixels, *clear; } image_t;   /* pixels: one palette index per byte; clear (row-run images): 1 where a fill run of 0 leaves the screen as it is in the transparent modes */
int image_decode(const uint8_t *res, int size, image_t *img);
void image_free(image_t *img);
