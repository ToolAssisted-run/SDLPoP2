/* Shared test scaffolding: globals, resource access, and logging stubs for not-yet-reconstructed routines. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/types.h"
#include "../src/globals.h"
#include "../src/dat.h"
#include "stubs.h"
char_type Char, Opp, Kid, chars[5]; level_type level; uint8_t tiles0[30]; uint32_t tick; int16_t knock, is_feather_fall;
int8_t control_x, control_y, control_shift; uint8_t drawn_room; uint16_t counter_5cec, word_27c0, counter_27d6, word_6140;
uint8_t flag_5cb9, byte_5cb8, level_kind, level_number, room_A; uint8_t *level_roomlinks;
int8_t ctrl1_forward, ctrl1_backward, ctrl1_up, ctrl1_down, ctrl1_shift;
static dat_file seqdat; static char log_[512];
static void note(const char *s) { strncat(log_, s, sizeof log_ - strlen(log_) - 1); }
static int16_t colx_l[32], colx_r[32]; const int16_t *col_x_left = colx_l + 8, *col_x_right = colx_r + 8;   /* DS:0D06 = 130 + 32*col, valid from col -5 */
void stubs_init(const char *seqpath, const char *levelbin)
{
	if (!dat_open(&seqdat, seqpath)) { fprintf(stderr, "cannot open %s\n", seqpath); exit(2); }
	FILE *f = fopen(levelbin, "rb"); if (f) { if (fread(&level, 1, sizeof level, f) != sizeof level) fprintf(stderr, "short level\n"); fclose(f); }
	level_roomlinks = (uint8_t *)&level + 0x17BC; level_number = ((uint8_t *)&level)[0x1847]; level_kind = ((uint8_t *)&level)[0x1845];
	for (int i = -8; i < 24; i++) { colx_l[i + 8] = 130 + 32 * i; colx_r[i + 8] = 162 + 32 * i; }   /* DS:0D06 / 0D08 */
}
void stubs_reset(void) { log_[0] = 0; }
const char *stubs_log(void) { return log_; }
const uint16_t *get_seq_words(uint16_t id) { uint16_t n; return (const uint16_t *)dat_find(&seqdat, "SQES", id, &n); }
int get_seq_resource(uint16_t id) { return dat_find(&seqdat, "SQES", id, NULL) != NULL; }
void seq_reload_current(void) { note(" reload"); }
int seq_condition(uint16_t c) { char t[32]; snprintf(t, sizeof t, " COND(%u)?", c); note(t); return 0; }
void seq_jump_to(uint16_t id) { Char.seq_id = id; Char.seq_pos = 0; }
void clear_char(void) { note(" clear_char"); }
void seq_ctl_1954(void) { note(" ctl1954"); }
void ovl_366c_1704(void) { note(" ovl1704"); }
void flash_on(uint16_t v) { (void)v; note(" flash_on"); } void flash_off(void) { note(" flash_off"); }
void play_sound(uint16_t n) { (void)n; } void sound_1611_01a8(uint16_t n) { (void)n; } int ovl_366c_11f8(uint8_t r) { (void)r; return 0; }
void shadow_hook_2f9a2(void) { note(" shadow"); }
void rtlink_fatal(int code) { char t[32]; snprintf(t, sizeof t, " FATAL(%x)", code); note(t); }
/* control.c externs not yet reconstructed */
uint8_t kid_f34, byte_2ab4, edge_type, start_room; int16_t word_3bf62; uint16_t word_6d46, word_8a84;
int tile_passable_2f800(uint16_t m, uint8_t t) { (void)m; return !tile_is_wall_kind(t); }
int shadow_seq_2f86a(void) { return -1; } int sword_seq_0317c4(void) { note(" sword0317c4?"); return -1; }
void ovl_2f86_0a5c(void) { note(" 2f86_0a5c"); } uint8_t find_char_02dcc8(void) { return (uint8_t)find_opponent(Char.direction); } void shadow_2fba4(void) {} void ovl_34024(void) {}
void ovl_383fa(void) { note(" 383fa"); } void ovl_35f5a(void) { note(" 35f5a"); } void ovl_35a88(void) {} int ovl_34350(void) { return 0; } int ovl_35240(int a) { (void)a; return 0; } int ovl_34ab2(void) { return 0; }
int control_sword_check_030e3c(void) { note(" swordcheck?"); return 0; } void ovl_384e8(void) {} int ovl_32a0e(void) { note(" 32a0e?"); return 0; } int8_t tile_col_in_drawn_room(void) { return tile_col; }
int gate_blocks_0329b6(void) { return can_bump_into_gate(); } void ovl_2f86_08d8(void) {} int level_door_open_0cfa(void) { return curr_modifier > 0x29; } void ovl_30b52(void) { note(" 30b52"); }
uint16_t word_922e, word_922c, word_8604, word_927e;
int ovl_377c6(void) { return 0; } void ovl_3741a(void) {} uint8_t room_nchars(uint8_t r) { return ((uint8_t *)&level)[0x17F3 + r * 0x74]; }
int8_t find_char_02dcc8_dir(int d) { return find_opponent((int8_t)d); } int rtlink_0dd5(void) { return 0; } 
void control_dead_0307a2(void) { note(" dead0307a2"); } void control_0d9_0e2(void) { note(" 0d9_0e2?"); }
int is_dead_frame(uint8_t f) { if (f == 0xB9) return 1; if (Char.charid == 0) return f == 0xF2 || f == 0xF3 || f == 0x10F || f == 0x10A; return 0; }

static uint8_t kidtab[20736];
void stubs_load_frame_tables(const char *exe)
{
	FILE *f = fopen(exe, "rb"); if (!f) { fprintf(stderr, "cannot open %s\n", exe); exit(2); }
	fseek(f, 0x3A500, SEEK_SET); if (fread(kidtab, 1, sizeof kidtab, f) < 12000) fprintf(stderr, "short data resource\n");
	fclose(f); frame_table_kid = kidtab;
	static dat_file princedat; if (dat_open(&princedat, getenv("PRINCE_DAT") ? getenv("PRINCE_DAT") : "PRINCE.DAT")) { uint16_t n; const uint8_t *g = dat_find(&princedat, "MARF", 1000, &n); frame_table_guard = g ? g : kidtab; } else frame_table_guard = kidtab;
	sword_table = frame_table_guard;
}

#include <stdlib.h>
void debug_case_tiles(void)
{
	printf("   dbg: room %u row %d col %d x %d dir %d | ahead1 %u infront %u atchar %u above %u | dist %d dx %d flags %02X\n", Char.room, Char.curr_row, Char.curr_col, Char.x, Char.direction,
	       get_tile_infrontof(1), get_tile_behind_char(), get_tile_at_char(), get_tile_above_char(), distance_to_edge_weight(), cur_frame.dx, cur_frame.flags);
}
void load_fram_det_col_nocol(void) { load_frame(); }   /* the game ran load_fram_det_col before control(); curr_col is already in the captured record */
void debug_opp(void) { printf("   misc: word_8604 %u drawn_room %u | ", word_8604, drawn_room); printf("   opp: charid %u room %u row %d x %d dir %d f12 %u f23 %u | char opp_index %u f12 %u f14 %d dist %d\n", Opp.charid, Opp.room, Opp.curr_row, Opp.x, Opp.direction, Opp.f12, Opp.f23, Char.opp_index, Char.f12, Char.hp_delta, opp_distance()); }

/* collision / kid stubs */
uint8_t room_L, room_R, room_B, room_AL, room_AR, room_BL, room_BR; int16_t word_440a; const uint8_t *sword_table;
void ovl_366c2(void) { note(" 366c2"); } void ovl_37bca(void) { note(" 37bca"); } int ovl_34ce6(void) { return 0; }
void ovl_34bd2(uint8_t *f, uint8_t *r, int8_t row) { (void)f; (void)r; (void)row; } int ovl_343c2(void) { note(" 343c2?"); return 0; }
int16_t ovl_34b28(int8_t row, uint8_t room, int8_t dir) { (void)row; (void)room; (void)dir; return 0; } int16_t ovl_352ca(void) { return 0; } void ovl_3211a(void) { note(" 3211a"); }
void loose_floor_184e(int8_t how) { char t[24]; snprintf(t, sizeof t, " LOOSE(%d)", how); note(t); } void ovl_348e6(void) { note(" CHOMPER"); } void ovl_3564e(void) { note(" 3564e"); }
void spikes_15d4(void) { note(" SPIKES"); } void spikes_16a0(void) { note(" spikes16a0"); } void ovl_34724(void) { note(" 34724"); } void ovl_37826(void) { note(" 37826"); }
void ovl_349be(void) {} void fall_scream_1611_0030(void) {} void sound_194c_83d2(uint16_t n) { (void)n; } int sound_playing_8426(void) { return 0; }
void shake_loose_row(int8_t row, uint8_t room) { char t[32]; snprintf(t, sizeof t, " SHAKE(%d,%u)", row, room); note(t); } void level_kind_hooks(void) { note(" kindhooks"); }
static dat_file kiddat; static int kiddat_ok;
/* 0993:0FE2 + 26BC:06B6: the SHAP resource header of the sprite (chtab 2 = KID.DAT, base id 25001: image+1, or image-399 above 221) */
int res_image_size(uint8_t chtab, int16_t image, int16_t *height, int16_t *width_m1)
{
	if (chtab != 2 || image < 0) { note(" IMGSIZE?"); return 0; }
	if (!kiddat_ok) { kiddat_ok = dat_open(&kiddat, getenv("KID_DAT") ? getenv("KID_DAT") : "KID.DAT") ? 1 : -1; }
	if (kiddat_ok < 0) return 0;
	uint16_t n; const uint8_t *r = dat_find(&kiddat, "PAHS", image <= 0xDD ? 25002 + image : 24602 + image, &n);
	if (!r) { note(" NOSHAP"); return 0; }
	*height = r[0] | (r[1] << 8); *width_m1 = r[2] | (r[3] << 8); return 1;
}
