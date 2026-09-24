#pragma once
#include <stdint.h>
#include <stddef.h>
typedef struct { uint8_t *data; size_t size; } dat_file;
int dat_open(dat_file *d, const char *path);
const uint8_t *dat_find(const dat_file *d, const char *tag, uint16_t id, uint16_t *size);
