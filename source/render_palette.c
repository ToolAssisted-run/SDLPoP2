/* The palette (the VGA DAC, 6-bit components, render_palette): the game's palette routines (0FB3:2B1C loads a PALS
 * resource's sub-palette, 0FB3:29B8 / 294C black out and put back colors 0x10..0xFF around a room change, 0FB3:2A34
 * a one-color flash, 2699:0048 rotates colors) and the loads of a level start and of a whole redraw. Transcribed from
 * the disassembly (0FB3:2B1C .. 2C9C, 2699:0048, 2D3E:0F50, 1286:00A2 / 01DE / 07CE / 096D, 0AAC:0358). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "globals.h"
#include "dat.h"
#include "render.h"
#include "render_frame.h"
const char *game_path(const char *name);
const char *level_kind_dat(void);

static uint8_t saved_hi[0xF0 * 3]; static int saved_hi_on;   /* DS:27DA: colors 0x10..0xFF while blacked out */
static uint8_t saved_lo[0x10 * 3]; static int saved_lo_on;   /* DS:27F8: colors 0..0xF (0FB3:2A34) */

/* 194C:79A3: count colors from start (NULL: black) */
static void dac_set(const uint8_t *rgb, int count, int start)
{
	for (int i = 0; i < count && start + i < 256; i++) for (int k = 0; k < 3; k++) render_palette[(start + i) * 3 + k] = rgb ? (uint8_t)(rgb[i * 3 + k] & 0x3F) : 0;
}
/* the PALS / PALC resource id of the files open (the game searches every open file; the ids here are unique to their
 * files: PRINCE.DAT 10 / 750 / 1000 / 2000 / 3000, KID.DAT 25001, the level kind's file 3500, CAVERNS.DAT 25303,
 * TEMPLE.DAT 4075; 750 also in the guard files BIRD / FLAME / HEAD / JINNEE / SKELETON, searched first) */
static const uint8_t *pal_res(const char *tag, int id, uint16_t *n)
{
	static dat_file files[12]; static int8_t ok[12];
	static const char *names[12] = {NULL, "PRINCE.DAT", "KID.DAT", "DESERT.DAT", "TEMPLE.DAT", "CAVERNS.DAT", "RUINS.DAT", "ROOFTOPS.DAT", "FINAL.DAT", NULL, NULL, NULL};
	static const char *guards[10] = {NULL, "FLAME.DAT", "SKELETON.DAT", NULL, NULL, "HEAD.DAT", "HEAD.DAT", "BIRD.DAT", "HEAD.DAT", "JINNEE.DAT"};
	const char *order[4] = {NULL, NULL, NULL, NULL}; int k = 0;
	uint8_t gt = guard_type_loaded();
	if (id == 750 && gt < 10 && guards[gt]) order[k++] = guards[gt];   /* (the guard file loaded, 1286:087E) */
	if (level_kind_dat()) order[k++] = level_kind_dat();   /* (the ids are unique to their files: the scenery file's first) */
	order[k++] = id == 25001 ? "KID.DAT" : "PRINCE.DAT";
	for (int j = 0; j < k; j++) {
		int f = -1; for (int i = 1; i < 12; i++) if (names[i] && order[j] && !strcmp(names[i], order[j])) f = i;
		if (f < 0) { for (int i = 9; i < 12; i++) if (!names[i]) { names[i] = order[j]; f = i; break; } if (f < 0) continue; }
		if (!ok[f]) ok[f] = dat_open(&files[f], game_path(names[f])) ? 1 : -1;
		if (ok[f] < 0) continue;
		const uint8_t *r = dat_find(&files[f], tag, (uint16_t)id, n);
		if (r) { (*n)++; return r; }   /* (dat_find's size is one short: the checksum byte) */
	}
	return NULL;
}
/* 0FB3:2B1C (sub-palette `sub` of `count` colors of PALS `res` from color `start`): while blacked out, the colors
 * from 0x10 go to the saved copy; 0 if the resource has no such sub-palette (PALC [0]) */
int render_pal_load(int sub, int count, int start, int res)
{
	uint16_t n; const uint8_t *c = pal_res("CLAP", res, &n);   /* 0FB3:2C60 */
	int nsub = c ? (c[0] | (n > 1 ? c[1] << 8 : 0)) : 0;
	const uint8_t *p = pal_res("SLAP", res, &n);
	if (!p || (uint8_t)nsub <= (uint8_t)sub || sub * count * 3 >= n) return 0;
	const uint8_t *src = p + sub * count * 3;
	if ((sub + 1) * count * 3 > n) count = (n - (int)(src - p)) / 3;   /* (the game reads on past a short resource; here only what it has) */
	if (start < 0x10) {
		int direct = count;
		if (saved_hi_on) {
			direct = 0x10 - start < count ? 0x10 - start : count;
			if (count - direct > 0) memcpy(saved_hi, src + direct * 3, (size_t)(count - direct) * 3);
		}
		dac_set(src, direct, start);
	} else if (saved_hi_on) memcpy(saved_hi + (start - 0x10) * 3, src, (size_t)count * 3);
	else dac_set(src, count, start);
	return 1;
}
int render_pal_saved(void) { return saved_hi_on; }
/* 0FB3:294C: the saved colors put back */
void render_pal_restore(void)
{
	if (saved_hi_on) { dac_set(saved_hi, 0xF0, 0x10); saved_hi_on = 0; }
	if (saved_lo_on) { dac_set(saved_lo, 0x10, 0); saved_lo_on = 0; }
}
/* 0FB3:29B8 (0823:0E72, a room change): colors 0x10..0xFF saved and black (the prince's old box erased on the
 * screen: covered by the whole redraw that follows) */
void render_pal_blackout(void)
{
	if (saved_hi_on) return;
	memcpy(saved_hi, render_palette + 0x10 * 3, sizeof saved_hi); saved_hi_on = 1;
	dac_set(NULL, 0xF0, 0x10);
}
/* 0FB3:2A34: every color becomes color n (the others saved first) */
void render_pal_flash(int n)
{
	uint8_t c[3]; memcpy(c, render_palette + n * 3, 3);
	if (!saved_hi_on) { memcpy(saved_hi, render_palette + 0x10 * 3, sizeof saved_hi); saved_hi_on = 1; }
	if (!saved_lo_on) { memcpy(saved_lo, render_palette, sizeof saved_lo); saved_lo_on = 1; }
	for (int i = 1; i < 256; i++) memcpy(render_palette + i * 3, c, 3);
}
/* 2699:0048 (start, count): colors start .. start + count - 1 rotated down by one (the first becomes the last) */
void render_pal_rotate(int start, int count)
{
	uint8_t first[3]; memcpy(first, render_palette + start * 3, 3);
	memmove(render_palette + start * 3, render_palette + (start + 1) * 3, (size_t)(count - 1) * 3);
	memcpy(render_palette + (start + count - 1) * 3, first, 3);
}
/* 2D3E:0F50 (a whole redraw, level types 0, 5, 6): the guard palette slots DS:5D08 / 5D09 (PALS 750 sub-palette
 * slot - 1 at 0x20 / 0x30); type 2: sub-palette 0 at 0x30 */
void render_pal_guards(void)
{
	uint8_t t = level.type;
	if (t == 0 || t == 5 || t == 6) { for (int s = 0; s < 2; s++) if (pal_slots[s]) render_pal_load(pal_slots[s] - 1, 0x10, 0x20 + 0x10 * s, 750); }
	else if (t == 2) render_pal_load(0, 0x10, 0x30, 750);
}
/* a level's palette as its start sets it: PALS 10 (colors 0..0xF, 0FB3:293A), PRINCE.DAT 3000 at 0xE0 (not on the
 * final level, 1286:01DE), the level kind's 3500 at 0x40 (0xA0 colors, 1286:00EA), the guard file's 750 at 0x20
 * (1286:096D), the prince's KID.DAT 25001 sub-palette kind - 1 at 0x10 (1286:07CE); image set 0's list (SHPL 1000,
 * mask 0x8000, loaded by 26BC:0034 with its colors: those of PALS 1000) at 0xF0 */
void render_pal_level_reset(void);
void render_pal_level_start(void)
{
	saved_hi_on = saved_lo_on = 0; render_pal_level_reset();   /* (colors the level does not set keep what the last scene left: 0xF0..0xFF, the image set 0 sprites' colors) */
	render_pal_load(0, 0x10, 0, 10);
	if (level_kind != 6) render_pal_load(0, 0x10, 0xE0, 3000);
	render_pal_load(0, 0xA0, 0x40, 3500);
	if (level.type < 10 && level.type != 4) render_pal_load(0, 0x10, 0x20, 750);
	render_pal_load(level_kind - 1, 0x10, 0x10, 25001);
	render_pal_load(0, 0x10, 0xF0, 1000);
}

/* ---- the rooms with a description: the palette parts of their hooks (0CD6:02BE / 073A call entry 0 / 1 of the
 * background's record, DS:02FE[bg] -> DS:01AA..; the game-state parts are roomhooks.c's) ---- */
extern uint16_t word_2ba6, minutes_left;
const uint8_t *render_desc_raw(void);
static void fill_offscreen(uint8_t color)   /* 194C:6632 / 4D72 on the offscreen port: DS:097E */
{
	int16_t r[4]; for (int k = 0; k < 4; k++) r[k] = (int16_t)ds_word((uint16_t)(0x097E + 2 * k));
	for (int y = r[0] < 0 ? 0 : r[0]; y < r[2] && y < 192; y++) for (int x = r[1] < 0 ? 0 : r[1]; x < r[3] && x < SCREEN_W; x++) offscreen[y * SCREEN_W + x] = color;
}
/* entry 0 (the room loaded), by background id */
void render_room_enter_palette(int bg)
{
	render_check_level();   /* (the level's own palette first) */
	switch (bg) {
	case 0x16: render_pal_load(1, 0xA0, 0x40, 3500); break;   /* 347C:01EE (OVL06, ruins) */
	case 0x22: render_pal_load(2, 0xA0, 0x40, 3500); break;   /* 37F0:001C (OVL14, level 8 room 9; then its music) */
	case 0x17:                                               /* 33FD:145E (OVL08, level 14 room 1) */
		if (!word_2ba6) render_pal_load(0, 0xC0, 0x40, 3500);
		fill_offscreen(0x8B);   /* (DS:2450 = DS:5CC2: the offscreen port stays the current one) */
		break;
	case 0x19: case 0x1A: case 0x1B:                         /* 33FD:1494 (OVL08) */
		render_pal_load(1, 0xC0, 0x40, 3500); render_pal_load(0, 0x10, 0xF0, 1000);
		if (drawn_room == 4 && level_kind == 6) fill_offscreen(0);   /* (then sound 0x10C) */
		else if (drawn_room == 3 && level_kind == 6) render_pal_load(0, 0x10, 0xE0, 3000);   /* (then sound 0x10D) */
		break;
	case 0x1C: case 0x1D: case 0x1E:                         /* 33FD:1708 (OVL08, level 14 rooms 6..8) */
		render_pal_load(2, 0xC0, 0x40, 3500); render_pal_load(0, 0x10, 0xF0, 1000); render_pal_load(7, 0x10, 0x20, 25001);
		if (drawn_room == 7 || drawn_room == 8) render_pal_load(0, 0x10, 0xE0, 3000);
		break;
	case 0: render_pal_load(0, 0x20, 0x10, 25303); render_lever5_set_saved(0); break;   /* 37F0:0000 -> 0406 (the block, zeroed) / 0436 (OVL11, level 5's lever room): 0x20 colors at 0x10 */
	case 0x21: if (level_number == 5 && drawn_room == 0xA) render_pal_load(1, 0xA0, 0x40, 3500); break;   /* 37F0:0426 (OVL12) */
	case 0x20: {                                             /* 37F0:0510 (OVL13): after its images, 5DD / 5F0 */
		const uint8_t *d = render_desc_raw();
		if (d) render_pal_load(0, 0x10, 0xE0, d[2] | d[3] << 8);
		render_pal_load(0, 0x10, 0x30, 2000);
		break; }
	default: break;
	}
}
/* entry 1 (the room left) */
void render_room_leave_palette(int bg)
{
	switch (bg) {
	case 0x22: if (!word_2ba6) render_pal_load(0, 0xA0, 0x40, 3500); break;   /* 37F0:0060 */
	case 0x16: if (Kid.alive < 0 && minutes_left != 0) render_pal_load(0, 0xA0, 0x40, 3500); break;   /* 347C:0202 (DS:5B47, 5CD2) */
	case 0: render_pal_load(0, 0x10, 0x20, 750); break;      /* 37F0:0012 (after 0622 frees its images) */
	case 0x20: render_pal_load(0, 0x10, 0xE0, 3000); break;  /* 37F0:06E0 */
	case 0x21: render_pal_load(0, 0xA0, 0x40, 3500); break;  /* 37F0:0574 */
	default: break;
	}
}

