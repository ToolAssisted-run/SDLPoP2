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
int shadow_seq_2f86a(void) { return -1; } int sword_seq_0317c4(void) { note(" sword0317c4?"); return -1; }
void ovl_2f86_0a5c(void) { note(" 2f86_0a5c"); } void shadow_2fba4(void) {} void ovl_34024(void) {}
void ovl_383fa(void) { note(" 383fa"); } void ovl_35f5a(void) { note(" 35f5a"); } void ovl_35a88(void) {} int ovl_34350(void) { return (level_kind == 2 || level_kind == 4) ? blade_running_here() : 0; }   /* 33FD:0380 in OVL05 */ int ovl_35240(int a) { (void)a; return 0; } int ovl_34ab2(void) { return 0; }
int control_sword_check_030e3c(void) { note(" swordcheck?"); return 0; } void ovl_384e8(void) {} int ovl_32a0e(void) { note(" 32a0e?"); return 0; } 
int gate_blocks_0329b6(void) { return can_bump_into_gate(); } void ovl_2f86_08d8(void) {}
uint16_t word_922e, word_922c, word_8604, word_927e;
int ovl_377c6(void) { return level_kind == 4 ? head_biting(Char.index, Char.room) : 0; }   /* 366C:1106 */ void ovl_3741a(void) {} 
int rtlink_0dd5(void) { return 0; } 
void control_dead_0307a2(void) { note(" dead0307a2"); } void control_0d9_0e2(void) { note(" 0d9_0e2?"); }

static uint8_t kidtab[20736];
void stubs_load_frame_tables(const char *exe)
{
	FILE *f = fopen(exe, "rb"); if (!f) { fprintf(stderr, "cannot open %s\n", exe); exit(2); }
	fseek(f, 0x3A500, SEEK_SET); if (fread(kidtab, 1, sizeof kidtab, f) < 12000) fprintf(stderr, "short data resource\n");
	fclose(f); frame_table_kid = kidtab;
	static dat_file princedat, guarddat; uint16_t n;
	/* sword frames: PRINCE.DAT FRAM 1000 (1286:0544); guard frames: the guard DAT's FRAM table (GUARD.DAT 750 on level 1, DS:0CB8) */
	sword_table = dat_open(&princedat, getenv("PRINCE_DAT") ? getenv("PRINCE_DAT") : "PRINCE.DAT") ? dat_find(&princedat, "MARF", 1000, &n) : NULL;
	(void)guarddat; frame_table_guard = kidtab;   /* set per level type by stubs_select_guard_dat() */
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
void ovl_348e6(void) { if (level_kind == 4) ruins_crumble(); else note(" CHOMPER"); }   /* 347C:0126 */ void ovl_3564e(void) { note(" 3564e"); }
void ovl_34724(void) { note(" 34724"); } void ovl_37826(void) { note(" 37826"); }
void ovl_349be(void) {} void fall_scream_1611_0030(void) {} void sound_194c_83d2(uint16_t n) { (void)n; } int sound_playing_8426(void) { return 0; }

static uint16_t guard_bank2[8];
const uint16_t *refract_timer; static uint16_t refract_tbl[16];
static dat_file kiddat, guardshp, envdat; static int kiddat_ok, guardshp_ok, envdat_kind = -1;
static int16_t env_bank2[8];   /* DS:05AC: per level kind, images at or above it come from the second bank (+200) */
/* 0993:0FE2 + 26BC:06B6: the SHAP resource header of the sprite (chtab 2 = KID.DAT, base id 25001: image+1, or image-399 above 221) */
/* DS:0672: the guard file of each level type (type 4 has none); opened on demand */
static const char *guard_names[10] = {"GUARD.DAT", "FLAME.DAT", "SKELETON.DAT", "GUARD.DAT", NULL, "HEAD.DAT", "HEAD.DAT", "BIRD.DAT", "HEAD.DAT", "JINNEE.DAT"};
static const dat_file *guard_file_of_type(uint8_t t)
{
	static dat_file f[10]; static int8_t ok[10];
	if (t > 9 || !guard_names[t]) return NULL;
	if (!ok[t]) { char path[512]; const char *dir = getenv("PRINCE2_DIR"); snprintf(path, sizeof path, "%s/%s", dir ? dir : ".", guard_names[t]); ok[t] = dat_open(&f[t], path) ? 1 : -1; }
	return ok[t] > 0 ? &f[t] : NULL;
}
const uint8_t *guard_frame_table(uint8_t charid)
{
	uint8_t t = (charid == 10 || charid == 12) ? charid_to_type[charid] : level.type;
	const dat_file *gf = guard_file_of_type(t); uint16_t n; const uint8_t *f = gf ? dat_find(gf, "MARF", 750, &n) : NULL;
	return f ? f : frame_table_guard;
}
int res_image_size(uint8_t chtab, int16_t image, int16_t *height, int16_t *width_m1)
{
	if (getenv("IMGDBG")) printf("IMG chtab %u image %d frame %u charid %u x %d\n", chtab, image, Char.frame, Char.charid, Char.x);
	if (chtab == 3 && image >= 0) {   /* guards: GUARD.DAT SHAP 751 + image */
		/* 0993:0F36: images at or above the guard type's threshold (DS:06BC[type] -> first word) come from the second bank (+100) */
		uint8_t t = (Char.charid == 10 || Char.charid == 12) ? charid_to_type[Char.charid] : level.type;
		int id = 751 + image; if (t != 5 && t != 6 && t < 8 && guard_bank2[t] && image >= guard_bank2[t]) id += 100;
		const dat_file *gf = guard_file_of_type(t);
		uint16_t n; const uint8_t *r = gf ? dat_find(gf, "PAHS", id, &n) : NULL;
		if (!r) { note(" NOGSHAP"); return 0; }
		*height = r[0] | (r[1] << 8); *width_m1 = r[2] | (r[3] << 8); return 1;
	}
	if (chtab == 4 && image >= 0) {   /* 0993:0DC8: the level kind's scenery file (DS:059C names), SHAP 3500 + image + 1 (+200 in the second bank) */
		static const char *env[7] = {NULL, "DESERT.DAT", "TEMPLE.DAT", "CAVERNS.DAT", "RUINS.DAT", "ROOFTOPS.DAT", "FINAL.DAT"};
		if (envdat_kind != level_kind) { char path[512]; envdat_kind = level_kind; snprintf(path, sizeof path, "%s/%s", getenv("PRINCE2_DIR") ? getenv("PRINCE2_DIR") : ".", level_kind < 7 && env[level_kind] ? env[level_kind] : "-");
			if (!dat_open(&envdat, path)) envdat.data = NULL; }
		int id = 3501 + image + (level_kind < 8 && env_bank2[level_kind] <= image ? 200 : 0);
		uint16_t n; const uint8_t *r = envdat.data ? dat_find(&envdat, "PAHS", id, &n) : NULL;
		if (!r) { note(" NOESHAP"); return 0; }
		*height = r[0] | (r[1] << 8); *width_m1 = r[2] | (r[3] << 8); return 1;
	}
	if (chtab != 2 || image < 0) { if (getenv("IMGDBG")) printf("IMG chtab %u image %d frame %u charid %u\n", chtab, image, Char.frame, Char.charid); note(" IMGSIZE?"); return 0; }
	if (!kiddat_ok) { kiddat_ok = dat_open(&kiddat, getenv("KID_DAT") ? getenv("KID_DAT") : "KID.DAT") ? 1 : -1; }
	if (kiddat_ok < 0) return 0;
	uint16_t n; const uint8_t *r = dat_find(&kiddat, "PAHS", image <= 0xDD ? 25002 + image : 24602 + image, &n);
	if (!r) { note(" NOSHAP"); return 0; }
	*height = r[0] | (r[1] << 8); *width_m1 = r[2] | (r[3] << 8); return 1;
}

/* room.c stubs */
uint16_t word_68f0;
void ovl_37d2a(void) { note(" 37d2a"); } void ovl_352b4(void) { note(" 352b4"); } int ovl_342b4(void) { note(" 342b4?"); return -1; } void ovl_34210(void) { note(" 34210"); }
void ovl_34958(void) { level6_entrance(); } void ovl_34370(void) { note(" 34370"); } void ovl_2f9f2(void) { note(" 2f9f2"); }
void load_guard_sprites(uint8_t t) { (void)t; } void ovl_guard6_sprites(void) {}
level_char_init *ovl_36ada(level_char_init *r) { note(" 36ada?"); return r; }
void ovl_36712(void) { note(" 36712"); } void room_music_087e(void) {} void redraw_room(void) {} void hp_bar_clear(void) {} void hp_bar_draw(uint8_t index, int a, uint8_t hp) { (void)index; (void)a; (void)hp; }
static uint8_t dstables[0x20];
void stubs_load_ds_tables(const uint8_t *ram)   /* DS:0096 type->charid, DS:00A2 charid->type (static data) */
{ memcpy(dstables, ram + 0x3B250 + 0x96, 0x20); type_to_charid = dstables; charid_to_type = dstables + 0x0C; guard_set_prob_tables(ram + 0x3B250); mobs_set_tables(ram + 0x3B250); caverns_set_tables(ram + 0x3B250); heads_set_tables(ram + 0x3B250); blades_set_tables(ram + 0x3B250); byte_016a = (int8_t)ram[0x3B250 + 0x16A]; cheat_mode = ram[0x3B250 + 0x10C2] | ram[0x3B250 + 0x10C3] << 8; word_0366 = ram[0x3B250 + 0x366] | ram[0x3B250 + 0x367] << 8;   /* outside the snapshot window */ for (int i = 0; i < 16; i++) refract_tbl[i] = ram[0x3B250 + 0x13D0 + 2 * i] | ram[0x3B250 + 0x13D1 + 2 * i] << 8; refract_timer = refract_tbl;
  for (int i = 0; i < 8; i++) { uint16_t p = ram[0x3B250 + 0x6BC + 2 * i] | ram[0x3B250 + 0x6BD + 2 * i] << 8; guard_bank2[i] = p ? (ram[0x3B250 + p] | ram[0x3B250 + p + 1] << 8) : 0;
    env_bank2[i] = (int16_t)(ram[0x3B250 + 0x5AC + 2 * i] | ram[0x3B250 + 0x5AD + 2 * i] << 8); } }

/* guard.c / play_all_chars stubs */
int ovl_383d2(void) { note(" 383d2?"); return 1; }
void ovl_shadow_37f0_78(void) { note(" shadow78"); } void ovl_33fd_694(void) { note(" 694?"); } void ovl_366c_e0a(void) { if (level_kind == 4) heads_ai(); else note(" e0a?"); } void ovl_366c_11(void) { note(" 11da?"); }
int ovl_36ed6(int16_t d) { if (level_kind == 4) return head_wall(d); note(" 36ed6?"); return -1; }   /* 366C:0816 */ void dead_char_sound_1611(void) {} void ovl_15db_64(void) { note(" 15db"); } void ovl_37d28(void) { note(" 37d28"); }

/* fight/tick stubs */
int ovl_366c_6ac(void) { if (level_kind == 4) return head_hit(); note(" 6ac?"); return -1; } int ovl_366c_1580(void) { note(" 1580?"); return -1; } void ovl_33fd_6ae(void) { note(" 6ae?"); }
int ovl_366c_fc(void) { note(" 366c_fc?"); return 0; } void music_1286_07ce(uint8_t k) { (void)k; } void ovl_366c_f24(void) { if (level_kind == 4) head_knock_back(); else note(" f24?"); }
void anim_tile_other(uint8_t t) { char m[24]; snprintf(m, sizeof m, " ANIM%02X?", t); note(m); }
void level_kind_tick(void) { if (level_kind == 5) { if (Kid.room == 0x13 || Kid.room == 0x10 || byte_9276 == 10) note(" KIND5?"); for (int i = 0; i < room_nchars(drawn_room); i++) if (chars[i].room == 0x13 || chars[i].room == 0x10 || byte_9276 == i) note(" KIND5c?"); } else if (level_kind == 2) temple_tick();   /* 347C:0FC4 */
  else if (level_kind == 3) { if (level_number == 5 && (drawn_room == 10 || drawn_room == 7 || drawn_room == 12)) note(" KIND3WATER?"); }   /* 33FD:0BEA */
  else note(" KINDTICK?"); }
void anim_start_other(uint8_t t, int8_t tp, uint8_t room, int si) { (void)tp; (void)room; (void)si; char m[24]; snprintf(m, sizeof m, " ASTART%02X?", t); note(m); }
/* mobs stubs */
void ovl_347c_b3e(uint8_t r, int8_t tp, int k) { (void)r; (void)tp; (void)k; note(" b3e?"); }
int ovl_2a31_dad(uint8_t r, int8_t tp) { (void)r; (void)tp; note(" dad?"); return -1; }
void ovl_347c_e8e(void) { slab_shake(); } void ovl_347c_126(void) { if (level_kind == 4) ruins_crumble(); else note(" 126?"); }
void ovl_mob_other(uint8_t t) { if (t == 10 && level_kind == 2) { slab_mob(); return; } char m[24]; snprintf(m, sizeof m, " MOB%u?", t); note(m); } int ovl_torch_347c(int c) { if (level_kind == 2) return temple_torch(c); note(" torch347c?"); return c; }
int ovl_347c_a0e(void) { note(" a0e?"); return 0; }
void note_missing(const char *what) { char m[40]; snprintf(m, sizeof m, " %s?", what); note(m); }
void ovl_347c_e48(void) { slab_step(); } void ovl_kind6_char(void) { note(" kind6?"); }

/* the guard sprite/frame file of a level type */
void stubs_select_guard_dat(uint8_t type)
{
	/* DS:0672: the guard file of each level type (type 4 has none) */
	static const char *names[] = {"GUARD.DAT", "FLAME.DAT", "SKELETON.DAT", "GUARD.DAT", NULL, "HEAD.DAT", "HEAD.DAT", "BIRD.DAT", "HEAD.DAT", "JINNEE.DAT"};
	static int cur = -1; if (type == cur || type > 9 || !names[type]) return; cur = type;
	char path[512]; const char *dir = getenv("PRINCE2_DIR"); snprintf(path, sizeof path, "%s/%s", dir ? dir : ".", names[type]);
	guardshp_ok = dat_open(&guardshp, path) ? 1 : -1;
	uint16_t n; const uint8_t *f = guardshp_ok > 0 ? dat_find(&guardshp, "MARF", 750, &n) : NULL;
	frame_table_guard = f ? f : kidtab;
}
int ovl_2a31_ddf(void) { note(" ddf?"); return 1; }
void seq_music_1611(uint8_t m) { (void)m; } void restart_prompt(void) { note(" restart"); }   /* 1611:0002 death music; 169B:123E prompt */
__attribute__((weak)) int death_sound_playing(int both) { (void)both; return 0; }
__attribute__((weak)) int level_end_sound_playing(void) { return 0; }
__attribute__((weak)) void ambient_sound(void) {}
__attribute__((weak)) int bios_key(void) { return 0; }
__attribute__((weak)) void platform_wait_frame(void) {}
const uint8_t *level_resource(uint16_t id, uint16_t *size) { static dat_file d; static int ok; if (!ok) ok = dat_open(&d, getenv("PRINCE_DAT") ? getenv("PRINCE_DAT") : "PRINCE.DAT") ? 1 : -1; return ok > 0 ? dat_find(&d, NULL, id, size) : NULL; }
