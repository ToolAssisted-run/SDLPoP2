/* The rooms' descriptions and the guard palettes through a whole game in one run. Every level in turn, every room
 * with a description loaded as a whole redraw loads it: each of its objects' images must be its own resource (the
 * description's first image + the object's, in the level kind's file). Then PALS 750 as the level's guard type
 * loads it: its guard file's when that file has one, else PRINCE.DAT's.
 * (The list of a description's images was kept from room to room and held 128 entries: by the eighth level of a
 * game played from the start an id not met before was no longer taken, and those rooms drew the set's own images
 * under their descriptions' ids. The palette search had places for three guard files of the five: the fourth of a
 * game, level 13's FLAME.DAT, was not looked in.)
 * usage: desctest GAME_DIR */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../source/core.h"
#include "../source/types.h"
#include "../source/globals.h"
#include "../source/dat.h"
#include "../source/render.h"

const char *level_kind_dat(void);
void render_desc_load(uint8_t room);
uint8_t guard_type_loaded(void);
int render_pal_load(int sub, int count, int start, int res);
extern uint8_t byte_0670;   /* DS:0670: the guard type loaded */

static int rd(const uint8_t *p) { return (int16_t)(p[0] | p[1] << 8); }
/* DS:0672, the guard types whose file has a PALS 750 (GUARD.DAT has none: PRINCE.DAT's) */
static const char *const pal_file[10] = {NULL, "FLAME.DAT", "SKELETON.DAT", NULL, NULL, "HEAD.DAT", "HEAD.DAT", "BIRD.DAT", "HEAD.DAT", "JINNEE.DAT"};

/* PALS 750's first sub-palette as the game loads it for guard type gt against the file's own */
static int pal_is_own(const char *dir, uint8_t gt, const char **name)
{
	*name = gt < 10 && pal_file[gt] ? pal_file[gt] : "PRINCE.DAT";
	char path[1024]; snprintf(path, sizeof path, "%s/%s", dir, *name);
	dat_file d; uint16_t pn = 0; const uint8_t *want = dat_open(&d, path) ? dat_find(&d, "SLAP", 750, &pn) : NULL;
	memset(render_palette + 0x20 * 3, 0, 0x30);
	int ok = render_pal_load(0, 0x10, 0x20, 750) && want != NULL;
	for (int i = 0; ok && i < 0x30; i++) if (render_palette[0x20 * 3 + i] != (want[i] & 0x3F)) ok = 0;
	return ok;
}
int main(int argc, char **argv)
{
	if (argc < 2) { fprintf(stderr, "usage: desctest GAME_DIR\n"); return 2; }
	if (!pop2_init(argv[1])) { fprintf(stderr, "pop2_init failed\n"); return 2; }
	int bad = 0, rooms = 0, images = 0, pals = 0;
	for (int lv = 1; lv <= 14; lv++) {
		pop2_new_game(lv, 0x1234567u * lv);
		int level_bad = 0, level_rooms = 0;
		for (int room = 1; room <= 28; room++) {
			drawn_room = (uint8_t)room;
			byte_5ce7 = (uint8_t)room_has_description(drawn_room); room_bg = 0;
			if (byte_5ce7) { int16_t bg = room_description_bg(drawn_room); if (bg >= 0) room_bg = bg + 1; }
			render_desc_load(drawn_room);
			uint16_t n; const uint8_t *r = room_bg ? room_description_res(drawn_room, &n) : NULL;
			if (!r || n < 0x1C) continue;
			rooms++; level_rooms++;
			int first = rd(r + 2);
			for (int i = 0; i < (int8_t)r[0] && 0x1C + 0x19 * (i + 1) <= n; i++) {
				const uint8_t *o = r + 0x1C + 0x19 * i;
				if (o[0] == 0xFF) continue;
				if (level_kind == 1 && r[1] == 6 && (i == 15 || i == 16)) continue;   /* (level 2's clue: the puzzle's images) */
				const char *dat = NULL; int res = -1; images++;
				int ok = render_image_res(4, first + i, &dat, &res) && res == first + (int8_t)o[0] && dat && level_kind_dat() && !strcmp(dat, level_kind_dat());
				if (!ok) { if (level_bad++ < 3) printf("  level %d room %d object %d: image %d is %s %d, not %s %d\n", lv, room, i, first + i, dat ? dat : "none", res, level_kind_dat(), first + (int8_t)o[0]); }
			}
		}
		/* the guard palette of the level's own type */
		uint8_t gt = guard_type_loaded(); const char *name; int pal_ok = pal_is_own(argv[1], gt, &name);
		pals++;
		printf("level %2d: %2d rooms with a description, %d images not their own; guard type %d: PALS 750 %s %s\n", lv, level_rooms, level_bad, gt, pal_ok ? "is" : "IS NOT", name);
		bad += level_bad + !pal_ok;
	}
	/* then every guard type's, as a game meets them one after another (1286:087E sets DS:0670: the flame of level
	 * 13, Jaffar) */
	for (int t = 0; t < 10; t++) {
		if (!pal_file[t]) continue;
		byte_0670 = (uint8_t)t;
		const char *name; int pal_ok = pal_is_own(argv[1], guard_type_loaded(), &name);
		pals++; bad += !pal_ok;
		if (!pal_ok) printf("guard type %d: PALS 750 IS NOT %s\n", t, name);
	}
	printf("desctest: %d rooms, %d images, %d palettes; %d wrong\n", rooms, images, pals, bad);
	return bad != 0;
}
