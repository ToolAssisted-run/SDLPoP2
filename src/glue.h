#pragma once
void glue_init(const char *seqdat, const char *levelbin);
void missing_reset(void);
const char *missing_log(void);
void control(void);
void glue_load_exe_tables(const char *prince_exe);
void debug_case_tiles(void);   /* tests/testutil.c */
void load_fram_det_col_nocol(void); void debug_opp(void);
void glue_load_ds_tables(const unsigned char *ram);
void glue_select_guard_dat(unsigned char type);
int glue_open_seq(const char *seqpath); const char *game_path(const char *name);
