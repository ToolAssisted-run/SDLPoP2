#pragma once
#include <stddef.h>
#include <stdint.h>
typedef struct state_field { const char *name; uint16_t ds; uint16_t size; void *p; } state_field;   /* ds 0: not a DS variable */
extern const state_field snap_fields[]; extern const int snap_nfields;
size_t state_size(void);
void state_save(uint8_t *buf);
void state_load(const uint8_t *buf);
uint64_t state_hash(void);
void state_load_ds_statics(const uint8_t *ds);   /* DS image: the EXE-initialised variables */
size_t checkpoint_state_size(void); void checkpoint_state_save(uint8_t *buf); void checkpoint_state_load(const uint8_t *buf);   /* level.c */
void glue_select_guard_dat(unsigned char type);
