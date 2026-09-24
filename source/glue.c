/* Glue: the globals, the game files (PRINCE.EXE tables, SEQUENCE/PRINCE/KID/guard/scenery DATs), the overlay entry
 * points (dispatched to the reconstructed routines by level kind) and a log of the ones not reconstructed yet. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "globals.h"
#include "dat.h"
#include "glue.h"
char_type Char, Opp, Kid, chars[5]; level_type level; uint8_t tiles0[30]; uint32_t tick; int16_t knock, is_feather_fall;
int8_t control_x, control_y, control_shift; uint8_t drawn_room; uint16_t counter_5cec, word_27c0, counter_27d6, word_6140;
uint8_t flag_5cb9, byte_5cb8, level_kind, level_number, room_A; uint8_t *level_roomlinks = (uint8_t *)&level + 0x17BC;
int8_t ctrl1_forward, ctrl1_backward, ctrl1_up, ctrl1_down, ctrl1_shift;
static dat_file seqdat; static char log_[512];
char glue_dir[400];   /* the game's directory (pop2_init); the tests use PRINCE2_DIR / PRINCE_DAT / KID_DAT */
/* the path of one of the game's files */
const char *game_path(const char *name)
{
	static char path[4][512]; static int k; char *p = path[k++ & 3];
	if (!strcmp(name, "PRINCE.DAT") && getenv("PRINCE_DAT")) return getenv("PRINCE_DAT");
	if (!strcmp(name, "KID.DAT") && getenv("KID_DAT")) return getenv("KID_DAT");
	const char *dir = getenv("PRINCE2_DIR") ? getenv("PRINCE2_DIR") : glue_dir[0] ? glue_dir : ".";
	snprintf(p, 512, "%s/%s", dir, name); return p;
}
static void note(const char *s) { strncat(log_, s, sizeof log_ - strlen(log_) - 1); }
static uint8_t ds_img[0x10000] __attribute__((aligned(2)));   /* the data segment's static part (glue_load_ds_tables) */
/* DS:0D06 / 0D08: the x of a column's left / right edge (130 + 32*col for cols -5..14; out-of-range columns read the
 * neighbouring data, as in the game) */
const uint8_t *ds_ptr(uint16_t a) { return ds_img + a; }   /* static DS tables in place */
const int16_t *col_x_left = (const int16_t *)(ds_img + 0x0D06), *col_x_right = (const int16_t *)(ds_img + 0x0D08);
int glue_open_seq(const char *seqpath) { return dat_open(&seqdat, seqpath); }
void glue_init(const char *seqpath, const char *levelbin)
{
	if (!dat_open(&seqdat, seqpath)) { fprintf(stderr, "cannot open %s\n", seqpath); exit(2); }
	FILE *f = levelbin ? fopen(levelbin, "rb") : NULL; if (f) { if (fread(&level, 1, sizeof level, f) != sizeof level) fprintf(stderr, "short level\n"); fclose(f); }
	level_roomlinks = (uint8_t *)&level + 0x17BC; level_number = ((uint8_t *)&level)[0x1847]; level_kind = ((uint8_t *)&level)[0x1845];
}
void missing_reset(void) { log_[0] = 0; }
const char *missing_log(void) { return log_; }
const uint16_t *get_seq_words(uint16_t id) { uint16_t n; return (const uint16_t *)dat_find(&seqdat, "SQES", id, &n); }
int get_seq_resource(uint16_t id) { return dat_find(&seqdat, "SQES", id, NULL) != NULL; }
void seq_reload_current(void) { note(" reload"); }
int seq_condition(uint16_t c) { char t[32]; snprintf(t, sizeof t, " COND(%u)?", c); note(t); return 0; }
void seq_jump_to(uint16_t id) { Char.seq_id = id; Char.seq_pos = 0; }

void seq_ctl_1954(void) { drink(); }   /* items.c */
void ovl_366c_1704(void) { note(" ovl1704"); }
void flash_on(uint16_t v) { (void)v; note(" flash_on"); } void flash_off(void) { note(" flash_off"); }
int ovl_366c_11f8(uint8_t r) { return room_draws_sword_pub(r); }   /* 366C:11F8 (OVL10, guard.c) */

void rtlink_fatal(int code) { char t[32]; snprintf(t, sizeof t, " FATAL(%x)", code); note(t); }
/* control.c externs not yet reconstructed */
uint8_t byte_2ab4, edge_type, start_room; int16_t word_3bf62; uint16_t word_8a84;

void ovl_2f86_0a5c(void) { turn_flash(); } 
void ovl_35a88(void) { ruins_open_tile7(); }   /* 347C:12C8 (ruins.c) */ int ovl_34350(void) { return (level_kind == 2 || level_kind == 4) ? blade_running_here() : 0; }   /* 33FD:0380 in OVL05 */ int ovl_35240(int a) { return wall_near(a); }   /* 347C:0A80 */ 
int control_sword_check_030e3c(void) { return try_pick_up(); }   /* items.c 2FDF:104C */  int ovl_32a0e(void) { return under_gate(); }   /* 3212:08EE (control.c) */ 
int gate_blocks_0329b6(void) { return can_bump_into_gate(); } void ovl_2f86_08d8(void) { turn_count(); }   /* 2F86:0078 (spirit.c) */
uint16_t word_922e, word_8604, word_927e;
int ovl_377c6(void) { return level_kind == 4 ? head_biting(Char.index, Char.room) : 0; }   /* 366C:1106 */ void ovl_3741a(void) {}   /* 366C:0D5A: a bare retf in OVL09 (the heads levels) */ 

void control_0d9_0e2(void) { note(" 0d9_0e2?"); }

static uint8_t kidtab[20736];
void glue_load_exe_tables(const char *exe)
{
	FILE *f = fopen(exe, "rb"); if (!f) { fprintf(stderr, "cannot open %s\n", exe); exit(2); }
	fseek(f, 0x3A500, SEEK_SET); if (fread(kidtab, 1, sizeof kidtab, f) < 12000) fprintf(stderr, "short data resource\n");
	fclose(f); frame_table_kid = kidtab;
	static dat_file princedat, guarddat; uint16_t n;
	/* sword frames: PRINCE.DAT FRAM 1000 (1286:0544); guard frames: the guard DAT's FRAM table (GUARD.DAT 750 on level 1, DS:0CB8) */
	if (dat_open(&princedat, game_path("PRINCE.DAT"))) { sword_tables[0] = dat_find(&princedat, "MARF", 1000, &n); sword_tables[1] = dat_find(&princedat, "MARF", 1200, &n); }
	(void)guarddat; frame_table_guard = kidtab;   /* set per level type by glue_select_guard_dat() */
}

/* collision / kid stubs */
uint8_t room_L, room_R, room_B, room_AL, room_AR, room_BL, room_BR;
const uint8_t *sword_tables[2];   /* PRINCE.DAT FRAM 1000, 1200 (sword type 2): DS:6110, 1286:0454/0544 */
void ovl_366c2(void) { head_attach(); }   /* 366C:0002 (heads.c) */ void ovl_37bca(void) { note(" 37bca"); } int ovl_34ce6(void) { return !GOD_KID && wall_find(Char.room, Char.curr_row) != NULL; }   /* 347C:0526 */
void ovl_34bd2(uint8_t *f, uint8_t *r, int8_t row) { wall_collision(row, r, f); }   /* 347C:0412 */ int ovl_343c2(void) { if (level_kind == 2 || level_kind == 4) { int m = (uint8_t)curr_modifier & 0x1F; return m >= 3 && m <= 0xF; } note(" 343c2?"); return 0; }   /* 33FD:03F2 (OVL05): tile 0xC blocks while its modifier is 3..15 */
int16_t ovl_34b28(int8_t row, uint8_t room, int8_t dir) { return wall_edge(dir, room, row); }   /* 347C:0368 */
int16_t ovl_352ca(void) { return wall_limit(Char.room, Char.curr_row); }   /* 1375:14FC -> 347C:0B0A */ void ovl_3211a(void) { seqtbl_offset_char(0x76); take_hp(100); }   /* 2FDF:232A: crushed by a caverns gate */
void ovl_348e6(void) { if (level_kind == 4) ruins_crumble(); else note(" CHOMPER"); }   /* 347C:0126 */ void ovl_3564e(void) { note(" 3564e"); }
void ovl_34724(void) { if (level_kind == 3) floor_collapse_pub(); else note(" 34724"); }   /* 33FD:0754 (OVL04, caverns.c) */ void ovl_37826(void) { skel_collapse(); }   /* 366C:1166 (skeleton.c) */

static uint16_t guard_bank2[8];
const uint16_t *refract_timer; static uint16_t refract_tbl[16];
static dat_file kiddat, envdat; static int kiddat_ok, envdat_kind = -1;
static int16_t env_bank2[8];   /* DS:05AC: per level kind, images at or above it come from the second bank (+200) */
/* 0993:0FE2 + 26BC:06B6: the SHAP resource header of the sprite (chtab 2 = KID.DAT, base id 25001: image+1, or image-399 above 221) */
/* DS:0672: the guard file of each level type (type 4 has none); opened on demand */
static const char *guard_names[10] = {"GUARD.DAT", "FLAME.DAT", "SKELETON.DAT", "GUARD.DAT", NULL, "HEAD.DAT", "HEAD.DAT", "BIRD.DAT", "HEAD.DAT", "JINNEE.DAT"};
static const dat_file *guard_file_of_type(uint8_t t)
{
	static dat_file f[10]; static int8_t ok[10];
	if (t > 9 || !guard_names[t]) return NULL;
	if (!ok[t]) ok[t] = dat_open(&f[t], game_path(guard_names[t])) ? 1 : -1;
	return ok[t] > 0 ? &f[t] : NULL;
}
/* DS:1BB4: resource 755 of the level type's guard file (HEAD.DAT for types 5 and 6, 1286:09C1): where a biting head
 * sits on each of the prince's images (2 bytes: dy, dx) */
/* 0CD6:02BE: the background id of a room's "CUST" description (resource (room + 159) * 25 of the scenery file), -1 none */
static const char *const kind_dat[7] = {NULL, "DESERT.DAT", "TEMPLE.DAT", "CAVERNS.DAT", "RUINS.DAT", "ROOFTOPS.DAT", "FINAL.DAT"};
const char *level_kind_dat(void) { return level_kind < 7 ? kind_dat[level_kind] : NULL; }   /* the level kind's scenery file */
/* the room's "CUST" description resource, NULL none */
const uint8_t *room_description_res(uint8_t room, uint16_t *size)
{
	static dat_file f[7]; static int8_t ok[7];
	if (level_kind >= 7 || !kind_dat[level_kind]) return NULL;
	if (!ok[level_kind]) ok[level_kind] = dat_open(&f[level_kind], game_path(kind_dat[level_kind])) ? 1 : -1;
	if (ok[level_kind] < 0) return NULL;
	return dat_find(&f[level_kind], "TSUC", (uint16_t)((room + 0x9F) * 0x19), size);
}
int16_t room_description_bg(uint8_t room)
{
	uint16_t n; const uint8_t *r = room_description_res(room, &n);
	return r && n > 1 ? r[1] : -1;
}
const uint8_t *head_attach_table(void) { const dat_file *gf = guard_file_of_type(level.type); uint16_t n; return gf ? dat_find(gf, NULL, 755, &n) : NULL; }
const uint8_t *guard_frame_table(uint8_t charid)
{
	uint8_t t = (charid == 10 || charid == 12) ? charid_to_type[charid] : level.type;
	const dat_file *gf = guard_file_of_type(t); uint16_t n; const uint8_t *f = gf ? dat_find(gf, "MARF", 750, &n) : NULL;
	return f ? f : frame_table_guard;
}
int res_image_size(uint8_t chtab, int16_t image, int16_t *height, int16_t *width_m1)
{
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
		if (envdat_kind != level_kind) { const char *path = game_path(level_kind < 7 && env[level_kind] ? env[level_kind] : "-"); envdat_kind = level_kind;
			if (!dat_open(&envdat, path)) envdat.data = NULL; }
		int id = 3501 + image + (level_kind < 8 && env_bank2[level_kind] <= image ? 200 : 0);
		uint16_t n; const uint8_t *r = envdat.data ? dat_find(&envdat, "PAHS", id, &n) : NULL;
		if (!r) { note(" NOESHAP"); return 0; }
		*height = r[0] | (r[1] << 8); *width_m1 = r[2] | (r[3] << 8); return 1;
	}
	if ((chtab == 0 || chtab == 1) && image >= 0) {   /* the sword sprites: PRINCE.DAT SHAP 1001 (1201 with sword type 2, levels 7/8) / 3001 + image (DS:60E6/60E8) */
		static dat_file pdat; static int pdat_ok;
		if (!pdat_ok) pdat_ok = dat_open(&pdat, game_path("PRINCE.DAT")) ? 1 : -1;
		uint16_t n; const uint8_t *r = pdat_ok > 0 ? dat_find(&pdat, "PAHS", (chtab ? 3001 : byte_5cba == 2 ? 1201 : 1001) + image, &n) : NULL;
		if (!r) { note(" NOSSHAP"); return 0; }
		*height = r[0] | (r[1] << 8); *width_m1 = r[2] | (r[3] << 8); return 1;
	}
	if (chtab != 2 || image < 0) { note(" IMGSIZE?"); return 0; }
	if (!kiddat_ok) { kiddat_ok = dat_open(&kiddat, game_path("KID.DAT")) ? 1 : -1; }
	if (kiddat_ok < 0) return 0;
	uint16_t n; int id = image <= 0xDD ? 25002 + image : 24602 + image; const uint8_t *r = dat_find(&kiddat, "PAHS", id, &n);
	if (!r) {   /* the resource search goes on through the other open files: the level kind's scenery file (level 14's fireballs) */
		static const char *env[7] = {NULL, "DESERT.DAT", "TEMPLE.DAT", "CAVERNS.DAT", "RUINS.DAT", "ROOFTOPS.DAT", "FINAL.DAT"};
		if (envdat_kind != level_kind) { const char *path = game_path(level_kind < 7 && env[level_kind] ? env[level_kind] : "-"); envdat_kind = level_kind;
			if (!dat_open(&envdat, path)) envdat.data = NULL; }
		r = envdat.data ? dat_find(&envdat, "PAHS", id, &n) : NULL;
	}
	if (!r) { note(" NOSHAP"); return 0; }
	if (n < 4) { *height = 0; *width_m1 = 5; return 1; }   /* a 1-byte placeholder (KID.DAT 25065): the original reads the heap
	                                                         * past it; h 0, w 5 as observed (F7_9) */
	*height = r[0] | (r[1] << 8); *width_m1 = r[2] | (r[3] << 8); return 1;
}

/* room.c stubs */
uint16_t word_68f0;
void ovl_37d2a(void) { note(" 37d2a"); } 
void ovl_34958(void) { level6_entrance(); }
void load_guard_sprites(uint8_t t) { (void)t; } void ovl_guard6_sprites(void) {}
level_char_init *ovl_36ada(level_char_init *r) { note(" 36ada?"); return r; }
void ovl_36712(void) { note(" 36712"); } void room_music_087e(void) {}

static uint8_t dstables[0x20];
void glue_load_ds_tables(const uint8_t *ram)   /* DS:0096 type->charid, DS:00A2 charid->type (static data) */
{ memcpy(ds_img, ram + 0x3B250, 0x10000); memcpy(dstables, ram + 0x3B250 + 0x96, 0x20); type_to_charid = dstables; charid_to_type = dstables + 0x0C; guard_set_prob_tables(ram + 0x3B250); mobs_set_tables(ram + 0x3B250); caverns_set_tables(ram + 0x3B250); heads_set_tables(ram + 0x3B250); blades_set_tables(ram + 0x3B250); byte_016a = (int8_t)ram[0x3B250 + 0x16A]; byte_14a0 = ram[0x3B250 + 0x14A0]; byte_0670 = ram[0x3B250 + 0x670]; cheat_mode = ram[0x3B250 + 0x10C2] | ram[0x3B250 + 0x10C3] << 8; word_0366 = ram[0x3B250 + 0x366] | ram[0x3B250 + 0x367] << 8;; fireball_width = (int16_t)(ram[0x3B250 + 0x842] | ram[0x3B250 + 0x843] << 8);   /* outside the snapshot window */ for (int i = 0; i < 16; i++) refract_tbl[i] = ram[0x3B250 + 0x13D0 + 2 * i] | ram[0x3B250 + 0x13D1 + 2 * i] << 8; refract_timer = refract_tbl;
  for (int i = 0; i < 8; i++) { uint16_t p = ram[0x3B250 + 0x6BC + 2 * i] | ram[0x3B250 + 0x6BD + 2 * i] << 8; guard_bank2[i] = p ? (ram[0x3B250 + p] | ram[0x3B250 + p + 1] << 8) : 0;
    env_bank2[i] = (int16_t)(ram[0x3B250 + 0x5AC + 2 * i] | ram[0x3B250 + 0x5AD + 2 * i] << 8); } }

/* guard.c / play_all_chars stubs */

void ovl_366c_e0a(void) { if (level_kind == 4) heads_ai(); else note(" e0a?"); } void ovl_366c_11(void) { note(" 11da?"); }
int ovl_36ed6(int16_t d) { if (level_kind == 4) return head_wall(d); note(" 36ed6?"); return -1; }   /* 366C:0816 */ void dead_char_sound_1611(void) { dead_char_music(); }   /* 1611:0068 (fight.c) */  void ovl_37d28(void) { note(" 37d28"); }

/* fight/tick stubs */
int ovl_366c_6ac(void) { if (level_kind == 4) return head_hit(); note(" 6ac?"); return -1; } int ovl_366c_1580(void) { note(" 1580?"); return -1; } 
void music_1286_07ce(uint8_t k) { (void)k; } void ovl_366c_f24(void) { if (level_kind == 4) head_knock_back(); else note(" f24?"); }
void anim_tile_other(uint8_t t) { char m[24]; snprintf(m, sizeof m, " ANIM%02X?", t); note(m); }
void level_kind_tick(void) { if (level_kind == 5) kind5_tick();   /* 33FD:0232 (kind5.c) */
  else if (level_kind == 2) temple_tick();   /* 347C:0FC4 */
  else if (level_kind == 3) { if (level_number == 5 && (drawn_room == 10 || drawn_room == 7 || drawn_room == 12)) bridge_tick(); }   /* 33FD:0BEA */
  else if (level_kind == 1) kind1_tick();   /* 33FD:0170 */
  else if (level_kind == 4) {}   /* DS:0654[4] is null: no kind tick */
  else if (level_kind == 6) kind6_tick();   /* 33FD:03C6 (final.c) */
  else note(" KINDTICK?"); }
void anim_start_other(uint8_t t, int8_t tp, uint8_t room, int si) { (void)tp; (void)room; (void)si; char m[24]; snprintf(m, sizeof m, " ASTART%02X?", t); note(m); }
/* mobs stubs */
void ovl_347c_b3e(uint8_t r, int8_t tp, int k) { wall_trigger(r, tp, k); }   /* walls.c */

void ovl_347c_e8e(void) { slab_shake(); } void ovl_347c_126(void) { if (level_kind == 4) ruins_crumble(); else note(" 126?"); }
void ovl_mob_other(uint8_t t) { if (t == 10 && level_kind == 2) { slab_mob(); return; } if (t == 6 && level_kind == 2) { wall_move(); wall_push_kid(); return; }   /* 347C:07D4, 027A */
 char m[24]; snprintf(m, sizeof m, " MOB%u?", t); note(m); } int ovl_torch_347c(int c) { if (level_kind == 2) return temple_torch(c); note(" torch347c?"); return c; }
int ovl_347c_a0e(void) { return wall_near_blade(); }   /* 347C:0A0E */
void note_missing(const char *what) { char m[40]; snprintf(m, sizeof m, " %s?", what); note(m); }
void ovl_347c_e48(void) { slab_step(); }

/* the guard sprite/frame file of a level type */
void glue_select_guard_dat(uint8_t type)
{
	/* DS:0672: the guard file of each level type (type 4 has none) */
	static const char *names[] = {"GUARD.DAT", "FLAME.DAT", "SKELETON.DAT", "GUARD.DAT", NULL, "HEAD.DAT", "HEAD.DAT", "BIRD.DAT", "HEAD.DAT", "JINNEE.DAT"};
	static int cur = -1; if (type == cur || type > 9 || !names[type]) return; cur = type;
	const dat_file *gf = guard_file_of_type(type);   /* opened once per file */
	uint16_t n; const uint8_t *f = gf ? dat_find(gf, "MARF", 750, &n) : NULL;
	frame_table_guard = f ? f : kidtab;
}

int pop2_keystrokes;   /* keystrokes waiting (core input) */
int (*bios_key_hook)(void);   /* tests: replace the keystroke source */
int bios_key(void) { if (bios_key_hook) return bios_key_hook(); if (pop2_keystrokes > 0) { pop2_keystrokes--; return 0x100; } return 0; }
void platform_wait_frame(void) {}
int (*frame_on_time_hook)(void);   /* the shell / tests: the frame timer DS:24DE still runs */
int frame_on_time(void) { return frame_on_time_hook ? frame_on_time_hook() : 1; }

const uint8_t *level_resource(uint16_t id, uint16_t *size) { static dat_file d; static int ok; if (!ok) ok = dat_open(&d, game_path("PRINCE.DAT")) ? 1 : -1; return ok > 0 ? dat_find(&d, NULL, id, size) : NULL; }
