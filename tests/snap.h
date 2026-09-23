/* Whole-DS snapshots (oracle probes of DS:2B00..6C00, 16640 bytes) mapped onto the reconstructed globals. */
#pragma once
#include <stdint.h>
extern int SNAP_BASE, SNAP_SIZE;   /* DS:2B00..6C00 (older captures) or DS:2900..6C00 */
#define SNAP_MAX 0x4300
void snap_load(const uint8_t *ds);                       /* snapshot -> globals */
void snap_store(uint8_t *ds);                            /* globals -> snapshot layout (only mapped fields) */
void snap_load_heap(const uint8_t *heap, uint32_t size);
int  snap_diff_heap(const uint16_t *exp_ptrs, const uint8_t (*objs)[0x65], const uint8_t *heap, uint32_t size, int verbose);
int  snap_diff(const uint8_t *got, const uint8_t *exp, const char *const *regions, int verbose);   /* compare named regions */
const char *snap_field_at(uint16_t a, int *off);
