/* Level start and checkpoints: 1286:01F2/02EE/05E2 (load), 169B:0070 (level loop setup), 169B:01A4/02B8 (the
 * prince), 169B:034A (the entrance door), 169B:0F72/0FB4 (resets), 169B:0DB4 + 0D5E:0FBA..122A (checkpoints). */
#include <string.h>
#include "types.h"
#include "globals.h"

uint8_t start_hp = 3;          /* DS:6B71: the prince's hp at level start (3; a LEVEL switch gives 3..12; carried between levels) */
uint16_t word_0996, word_0880; /* DS:0996, DS:0880 */
uint8_t byte_5cbb; uint16_t word_5cbe, word_5d36;   /* DS:5CBB / 5CBE / 5D36 */

/* checkpoint copy (0D5E:0FBA: a 0x2597-byte far block) */
static struct {
	int used; uint16_t index; uint8_t level, hp; int8_t dir; uint8_t sword; uint16_t updown;   /* +0, +8, +9, +A, +C, +F */
	uint8_t tiles[0x3C0], attrs[0xF00], records[0xE80], spawns[0x440];   /* level +0, +0x3C0, +0x1867, +0x26F9 (rooms 1..32) */
} cp;
#define LV ((uint8_t *)&level)
static const uint8_t *checkpoint_table(void) { return LV + 0x26E7; }   /* DS:529F: two (room, tile) pairs */

/* 0D5E:10B4: gates in the copy become open (or shut when animating), exit doors open */
static void checkpoint_tidy(void)
{
	for (int r = 0; r < level.nrooms; r++)
		for (int tp = 0; tp < 30; tp++) {
			uint8_t t = cp.tiles[r * 30 + tp]; uint8_t *a = cp.attrs + (r * 30 + tp) * 4;
			if (t == 4) {
				trob_type *tr = get_trob(tp, r + 1); int si = tr ? (tr->state == 3 ? 0xC8 : 0) : a[0];
				a[0] = si;
			} else if (t == 0x11 && a[0]) a[0] = 0x2A;
		}
}
/* 0D5E:0FBA */
void checkpoint_save(int n)
{
	cp.used = 1; cp.index = n;
	memcpy(cp.tiles, LV, 0x3C0); memcpy(cp.attrs, LV + 0x3C0, 0xF00); memcpy(cp.records, LV + 0x1867, 0xE80); memcpy(cp.spawns, LV + 0x26F9, 0x440);
	cp.level = level_number; cp.hp = Kid.f13; cp.dir = Kid.direction; cp.updown = word_5d38; cp.sword = byte_5cba;
	checkpoint_tidy();
}
/* 0D5E:11A0 (after a level load) */
void checkpoint_restore(void)
{
	if (!cp.used) return;
	memcpy(LV, cp.tiles, 0x3C0); memcpy(LV + 0x3C0, cp.attrs, 0xF00); memcpy(LV + 0x1867, cp.records, 0xE80); memcpy(LV + 0x26F9, cp.spawns, 0x440);
	word_5d38 = cp.updown;
}
/* 0D5E:1094 (new level) */
void checkpoint_free(void) { cp.used = 0; }
/* 0D5E:122A: where a restarted prince appears; 2 = the standing sequence, -1 = no checkpoint */
static int checkpoint_start(void)
{
	if (!cp.used) return -1;
	const uint8_t *e = checkpoint_table() + (cp.index - 1) * 2;
	Char.room = next_room = e[0]; Char.curr_col = (int8_t)e[1] % 10; Char.curr_row = (int8_t)e[1] / 10;
	start_hp = cp.hp; Char.direction = cp.dir; byte_5cba = cp.sword;
	return 2;
}
/* 169B:0DB4 (every tick): a living prince standing on a checkpoint tile saves it */
void checkpoints_0db4(void)
{
	if (Kid.room == 0 || Kid.action == 4 || Kid.alive >= 0) return;
	if ((Kid.hp_delta < 0 ? -Kid.hp_delta : Kid.hp_delta) >= (int8_t)Kid.f12 || Kid.charid == 1) return;
	if (Kid.room == 9 && level_number == 8 && byte_5cba != 1) return;
	const uint8_t *e = checkpoint_table();
	for (int i = 0; i < 2; i++, e += 2)
		if (e[0] == Kid.room && (Kid.curr_row >= 0 ? Kid.curr_row * 10 : Kid.curr_row * 10 + 9) + Kid.curr_col == (int8_t)e[1] && (!cp.used || cp.index != i + 1))
			checkpoint_save(i + 1);
}

/* 1286:05E2: after loading, tile 7 loses attribute bit 7 */
void level_postprocess(void)
{
	for (int r = 1; r <= level.nrooms; r++) {
		get_room_address(r); room_L = level_links(r)[0]; room_R = level_links(r)[1];
		for (int tp = 0; tp < 30; tp++) if (curr_room_tiles[tp] == 7) curr_room_attrs[tp] &= ~0x80u;
	}
}
/* 169B:0F72: every character record starts its sequence afresh */
static void reset_record_seqs(void)
{
	for (int r = 1; r <= level.nrooms; r++) { level_char_init *rec; for (int i = 0; (rec = room_char_record(i, r)) != NULL; i++) rec->seq_pos = 0; }
}
/* 169B:0FB4 */
static void level_kind_reset(void)
{
	if (level_kind == 3) { floor_free_all(); if (level_number == 5) note_missing("33FD_0C1A"); }
	else if (level_kind == 4) note_missing("DS2BAE_RESET");   /* DS:2BAE/2BB0/2BB2 = 0 */
	else if (level_kind == 6) note_missing("DS2BB4_RESET");   /* DS:2BB4 = 0 */
}
/* 3212:083C: collision history rows unknown */
static void reset_collisions(void) { uint8_t *c = (uint8_t *)&coll; memset(c + 0x1A, 0xFF, 10); memset(c + 0x24, 0xFF, 10); memset(c + 0x2E, 0xFF, 10); memset(c + 0x38, 0xFF, 10); c[0xA] = 0xFF; }

/* 169B:02B8: the rest of the prince's record, then Kid = Char */
static void init_kid_record(void)
{
	Char.index = 10; Char.charid = 0; Char.f23 = 3; Char.f24 = 0; Char.pal_slot = 0; Char.alive = -1; Char.opp_index = 0xFF;
	char_y_to_floor(); Char.fall_x = Char.fall_y = 0;
	knock = 0; word_5d36 = 0; word_6142 = 0; word_6146 = 0;
	Char.f10 = byte_5cba == 0xFF ? 0xFF : 0;
	play_seq(); Kid = Char;
}
/* 169B:031A (level 7): the start depends on DS:5CB9 */
static void level7_start(void)
{
	if (flag_5cb9 == 2) { level.start_room = 4; level.start_tile = 0x19; LV[0x3E60 - 0x2BB8] = 0; }
	else { level.start_room = 0x20; level.start_tile = 0x18; LV[0x3144 - 0x2BB8] = 0; }
}
/* 169B:01A4 */
static void init_kid(void)
{
	Char.charid = 0;
	if (level_number == 7) level7_start();
	Char.room = next_room = level.start_room;
	Char.curr_col = level.start_tile % 10; Char.curr_row = level.start_tile / 10;
	Char.direction = ~level.start_dir;
	int si = checkpoint_start(), steps = 0;
	Char.x = col_x_left[Char.curr_col] + 0xE;
	Char.f12 = Char.f13 = start_hp;
	if (si == -1) {
		if (level_kind == 5 && level.start_room == 4) { si = 4; steps = 9; }
		else if (level_kind == 1) si = 0x7C;
		else if (level.start_room == 0x1B && level_number == 6) { si = 2; Char.x = 0xF0; }
		else if (level.start_room == 0x16 && level_number == 10) { si = 2; Char.x = 0x136; }
		else if (level_number == 14 && Char.room == 1) { si = 2; Char.x = 0x132; }
		else si = 5;
	}
	seqtbl_offset_char(si);
	while (steps--) play_seq();
	init_kid_record();
	byte_5cbb = 0xFF; word_5cbe = 0; word_8604 = 1;
}
/* 169B:034A: the entrance door behind the prince closes */
static void close_entrance(void)
{
	if (level.start_room != Kid.room && level_number != 7 && level_number != 6) return;
	get_room_address(Kid.room);
	for (int tp = 0; tp < 30; tp++)
		if (curr_room_tiles[tp] == 0x11) {
			if (level_kind != 1) { curr_room_attrs[tp] = (curr_room_attrs[tp] & ~0xFFu) | 0x2A; add_trob(0x11, 4, tp, Kid.room); }   /* 1375:170A */
			/* 194C:805C(0x2719): the door sound */
			return;
		}
}
/* 169B:00F5..0135: after the level is loaded, before its first tick */
void level_begin(void)
{
	reset_record_seqs(); reset_collisions();
	memset(kid_ctrl1_saved, 0, sizeof kid_ctrl1_saved);   /* 0AFF:13A8 */
	level_kind_reset();
	drawn_room = 0; mob_count = 0; trob_count = 0; word_8a84 = 0; word_0996 = 0; word_087e = -1; word_0880 = -1;
	Kid.hp_delta = 0; chars[0].direction = 0x56;
	init_kid(); close_entrance();
}

/* 1286:01F2 / 02EE: load level n (resource 0x7CF + n, +0x14 with the GAMEPLAY switch), then the checkpoint copy.
 * A different level than the current one drops the checkpoint. */
int load_level(int n)
{
	if (n != (int8_t)word_32d8) checkpoint_free();   /* (unless DS:5CB6) */
	word_32d8 = n; counter_5cec = n;
	uint16_t size; const uint8_t *p = level_resource(0x7CF + n, &size);
	if (!p) return 0;
	memcpy(&level, p, size < sizeof level ? size : sizeof level);
	level_kind = level.hdr_pad2[4]; level_number = level.number;
	level_postprocess(); checkpoint_restore();
	return 1;
}
uint16_t word_0366;   /* DS:0366 */
/* 0AAC:0120 (the state part of 0AAC:000E, before each level): the story scene after level `prev`, and DS:016A, the
 * story/timer stage (-1 until the first scene after level 3; the clock runs from 0 on). Returns the scene (0 none). */
int story_scene(int prev, int n)
{
	int si = 0;
	if ((int8_t)byte_6b6c > 2 && word_0366 == 0) return 0x64;
	if (byte_6b6c == 0) return 0;
	if (!(prev != 0 && (n == prev || n == -1)))
		switch (prev) { case 1: si = 9; break; case 2: si = 0x64; break; case 3: si = 0xA; break; case 5: si = 1; break; case 8: si = 2; break; case 13: si = 3; break; }
	if (si == 0 && prev >= 4) {
		if (byte_016a == -1) { si = 0x14; byte_016a = 0; }
		else { int8_t st = (int8_t)((1 - (int16_t)minutes_left) / 9 + 7); if (st > byte_016a) { si = st + 0x14; byte_016a = st; } }
	}
	/* with the cheat word, NISn / TREEn on the command line choose the scene or the stage */
	return si;
}
/* 169B:0070 (after 169B:0006): play levels from n until the player quits; returns 0 or -1 */
int play_level(int n)
{
	while (n > 0 && n <= 14) {
		if (!word_5cb6) story_scene((int8_t)word_32d8, n);   /* 0AAC:000E: scene (0AAC:0274 shows it) */
		if (!load_level(n)) return -1;
		level_begin();
		int r = level_first_room();
		if (r == -1) return -1;
		do r = play_frame(); while (r == -2);
		if (r == -1) return -1;
		n = r;
	}
	return n;
}
void close_entrance_pub(void) { close_entrance(); }
