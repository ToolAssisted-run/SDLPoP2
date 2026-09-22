/* Whole-DS snapshots (oracle probes of DS:2B00..6C00, 16640 bytes) mapped onto the reconstructed globals. */
#pragma once
#include <stdint.h>
#define SNAP_BASE 0x2B00
#define SNAP_SIZE 0x4100
void snap_load(const uint8_t *ds);                       /* snapshot -> globals */
void snap_store(uint8_t *ds);                            /* globals -> snapshot layout (only mapped fields) */
int  snap_diff(const uint8_t *got, const uint8_t *exp, const char *const *regions, int verbose);   /* compare named regions */
