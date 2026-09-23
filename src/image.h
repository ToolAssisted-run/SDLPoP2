#pragma once
#include <stdint.h>
typedef struct image_t { int width, height, depth, flags; uint8_t *pixels; } image_t;   /* pixels: one palette index per byte */
int image_decode(const uint8_t *res, int size, image_t *img);
void image_free(image_t *img);
