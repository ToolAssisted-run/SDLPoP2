/* The level kinds' overlay parts of the frame's drawing (the overlays at 33FD / 347C / 34C1 / 366C / 37F0 / 2F86, by
 * level kind): falling objects, sprite clips and extra sprites. Transcribed from the disassembly of each overlay. */
#include <stdio.h>
#include <string.h>
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
#include "types.h"
#include "globals.h"
#include "render.h"
#include "render_tiles.h"
#include "render_frame.h"

/* 34C1:136C (caverns): a wall modifier (low 4 bits 1, 4, 6, 9) the sprite is not cut under */
int caverns_wall_open(uint16_t mod) { int m = mod & 0xF; return m == 1 || m == 4 || m == 6 || m == 9; }

/* 33FD:01A2 (falling objects of types 2 and 5): caverns (OVL04): one in the drawn room still falling (speed not -1
 * / -2) is an object (type 0x80 + its type), the fore layer redrawn over it, and becomes type 2 */
void draw_mob_33fd_01a2(void)
{
	switch (level_kind) {
	case 3:
		if (cur_mob.speed == -1 || cur_mob.speed == -2) return;
		{ int8_t col = (int8_t)(cur_mob.x < 0 ? -((-cur_mob.x) >> 5) : cur_mob.x >> 5), row = y_to_row(cur_mob.y);
		  int8_t t = tile_index_of(row, col); sv.key = (uint8_t)(t < 0 ? 0x1E : t); }   /* 0AFF:026A */
		if (cur_mob.room != drawn_room) return;
		render_mob_mark(1); render_mob_obj(cur_mob.y);   /* 1375:2296, 22DC */
		cur_mob.type = 2;
		return;
	default: note_missing("33FD:01A2"); return;
	}
}

/* 366C:15F6 (OVL10, the riser, charid 10, in images 4, 0x13, 0x14): it bobs: random(4) - 2 (or random(1) + 1 when
 * at or above its row's y + 0x36, random(1) - 2 when below y + 0x3A), applied to its y, the sprite and its rect */
void draw_hook_366c_15f6(void)
{
	if (obj_id != 4 && obj_id != 0x13 && obj_id != 0x14) return;
	int si = 4, di; int16_t base = (int16_t)(0x3F * Char.curr_row);
	if (base + 0x36 >= Char.y) { di = 1; si = 1; }
	else { di = -2; if (base + 0x3A <= Char.y) si = 1; }
	int16_t d = (int16_t)(random_2751(si) + di);
	Char.y = (int16_t)(Char.y + d); obj_y = (int16_t)(obj_y + d);
	sv.rect[0] += d; sv.rect[2] += d;   /* 194C:50EC */
}

static int wall_290a(uint8_t t) { return t == 0x14 || t == 2 || t == 7 || t == 0x19 || t == 0x2B; }             /* 0FB3:290A */
static int empty_28d4(uint8_t t) { return t == 0 || t == 9 || t == 0x21 || t == 0x23 || t == 0x1B || t == 0x25; }   /* 0FB3:28D4 */
/* 347C:0000 (OVL06, ruins: 0AFF:15AE's kind 4 part): a wall at the column of the sprite's right edge (on its row and
 * its top row, not a gate 7) cuts the sprite at that column's left; for the heads (charid 7 / 8, f19 above 0x90) a
 * non-empty tile above cuts it at their bottom row's top */
void draw_hook_347c_0000(void)
{
	int heads = Char.charid == 7 || Char.charid == 8;
	int8_t c = col_from_x18(char_x_right);   /* 0AFF:1010 */
	if (wall_290a(get_tile(Char.curr_row, c, Char.room))) {
		uint8_t t2 = get_tile(char_top_row, c, Char.room);
		if (wall_290a(t2) && t2 != 7 && curr_room == Char.room) sv.rect[3] = (int16_t)(tile_col << 5);
	}
	if (!heads || (int16_t)Char.f19 <= 0x90) return;
	int8_t c1 = col_from_x18(char_x_left), c2 = col_from_x18(char_x_right);
	if (c1 != c2) {
		int16_t r1 = (int16_t)((char_x_right - 32) % 32), r2 = (int16_t)((char_x_left - 0x82) % 32);
		if (32 - r2 > r1) c--;
	}
	if (!empty_28d4(get_tile(char_top_row, c, Char.room))) sv.rect[0] = (int16_t)(0x3F * char_bottom_row + 3);
}

/* ---- rooftops (kind 5, OVL02 at 33FD) ---- */
extern uint8_t byte_9276, byte_6937; extern int16_t word_6938;   /* DS:6936 (the character being grabbed), 6937 (the grab's step), 6938 (its x) */
/* 33FD:0190 (0AFF:15AE's kind 5 part, and the swords): in the sea rooms 0x13 / 0x10 the grabbed character's sprite
 * is cut at the water (y 0xAC, 0xBA while hanging: action 9) */
void draw_hook_33fd_0190(void)
{
	if (Char.room != 0x13 && Char.room != 0x10) return;
	if (Char.index != byte_9276) return;
	int16_t dx = (int8_t)Char.action == 9 ? 0xBA : 0xAC;
	if (dx > sv.rect[2]) dx = sv.rect[2];
	sv.rect[2] = dx;
	if (dx < sv.rect[0]) sv.rect[0] = dx;
}
/* 33FD:0482 (after each character, kind 5): during the grab (DS:6937 steps 1..6) of the grabbed character, the
 * hand object (description background 0x12 / 0x13, object DS:13E8[step]) placed at the character (y from its rect +
 * 0xB0, 0xE lower while hanging; x centered on DS:6938) and put in front (layer 1, 0FB3:0712), the fore layer
 * over it redrawn */
void draw_hook_33fd_0482(void)
{
	if (Char.index != byte_9276 || byte_6937 == 0 || (int8_t)byte_6937 >= 7) return;
	if (render_desc_bg() != 0x12 && render_desc_bg() != 0x13) return;
	int16_t save[4]; memcpy(save, draw_clip, sizeof save);
	int k = ds_byte((uint16_t)(0x13E8 + 2 * (int8_t)byte_6937));
	uint8_t *o = desc_obj(k);
	int16_t r[4]; for (int q = 0; q < 4; q++) r[q] = (int16_t)(o[0xB + 2 * q] | o[0xC + 2 * q] << 8);
	int16_t y = (int16_t)(r[0] - r[2] + 0xB0); if ((int8_t)Char.action == 9) y += 0xE;
	int16_t w = (int16_t)(r[1] - r[3]); int16_t x = (int16_t)((w < 0 ? -((-w) >> 1) : w >> 1) + word_6938);
	o[1] = (uint8_t)y; o[2] = (uint8_t)(y >> 8); o[3] = (uint8_t)x; o[4] = (uint8_t)(x >> 8);
	desc_obj_image_rect(o);   /* 194C:13B3 */
	for (int q = 0; q < 4; q++) draw_clip[q] = (int16_t)(o[0xB + 2 * q] | o[0xC + 2 * q] << 8);
	o[5] = 1; draw_object(k, 1); o[5] = 0xB;   /* 0FB3:0712 */
	if ((int8_t)Char.action != 9) { int16_t rr[4]; for (int q = 0; q < 4; q++) rr[q] = (int16_t)(o[0xB + 2 * q] | o[0xC + 2 * q] << 8); mark_tiles_under(mark_fore, rr, 0xFF); }
	memcpy(draw_clip, save, sizeof save);
}

/* ---- desert (kind 1, OVL03 at 33FD) ---- */
/* 33FD:0012 (0AFF:15AE's kind 1 part): the sprite cut at y 0x7E */
void draw_hook_33fd_0012(void)
{
	int16_t b = sv.rect[2] > 0x7E ? 0x7E : sv.rect[2];
	sv.rect[2] = b;
	if (b < sv.rect[0]) sv.rect[0] = b;
}
/* 33FD:0478 (after the prince, kind 1): in f19 0x1B (the rope?) the description object 3 or 4 (frame parity) placed
 * at the prince's x (its left edge when facing left) and y 0x77, the back layers under it redrawn and drawn in front */
void draw_hook_33fd_0478(void)
{
	if (Char.f19 != 0x1B) return;
	int16_t save[4]; memcpy(save, draw_clip, sizeof save);
	int k = (int)(tick & 1) + 3;
	uint8_t *o = desc_obj(k);
	int16_t x = obj_x; if (sv.dir == 0) x = (int16_t)(x - image_width);
	o[1] = 0x77; o[2] = 0; o[3] = (uint8_t)x; o[4] = (uint8_t)(x >> 8);
	desc_obj_image_rect(o);   /* 194C:13B3 */
	int16_t r[4]; for (int q = 0; q < 4; q++) r[q] = (int16_t)(o[0xB + 2 * q] | o[0xC + 2 * q] << 8);
	mark_tiles_under(mark_back, r, 0xFF);
	memcpy(draw_clip, r, sizeof r);
	o[5] = 1; draw_object(k, 1); o[5] = 0xB;
	memcpy(draw_clip, save, sizeof save);
}

/* ---- level 13's room 4 (OVL13 at 37F0) ---- */
/* 37F0:05FC (0993:07F8, the prince on frames 0x132..0x13E; in room 4 of level 13): the sword frame's entry (the sword
 * table DS:[6110], cur_frame's sword) names a description object (image + 0x17) placed at its offsets from obj_x /
 * obj_y (0AFF:0390) and drawn in the background, clipped to its own rect; DS:60FA.. kept around it (0AFF:1C0A / 1C1C) */
void draw_hook_37f0_05fc(void)
{
	if (Char.room != 4 || level_number != 13) return;
	spr_vars sv0 = sv; int16_t ox = obj_x, oy = obj_y, oid = obj_id; uint8_t och = obj_chtab;
	int16_t save[4]; memcpy(save, draw_clip, sizeof save);
	const uint8_t *e = sword_table + cur_frame.sword * 4;
	int k = (uint8_t)(e[0] + 0x17);
	if (k < desc_count()) {
		uint8_t *o = desc_obj(k);
		int16_t dx = (int8_t)e[2]; if (sv.dir != 0) dx = (int16_t)-dx;
		obj_x = (int16_t)(obj_x + dx);
		int16_t x = obj_x, y = (int16_t)((int8_t)e[3] + obj_y);
		o[3] = (uint8_t)x; o[4] = (uint8_t)(x >> 8); o[1] = (uint8_t)y; o[2] = (uint8_t)(y >> 8);
		desc_obj_image_rect(o);   /* 194C:13B3 */
		for (int q = 0; q < 4; q++) draw_clip[q] = (int16_t)(o[0xB + 2 * q] | o[0xC + 2 * q] << 8);
		o[5] = 0; draw_object(k, 0); o[5] = 0xB;
	}
	memcpy(draw_clip, save, sizeof save);
	sv = sv0; obj_x = ox; obj_y = oy; obj_id = oid; obj_chtab = och;
}

/* ---- the final level (kind 6, OVL08 at 33FD) ---- */
/* 33FD:0330 (0AFF:15AE's kind 6 part): in room 3, row 1, a dead frame: the sprite cut at y 0x77 */
void draw_hook_33fd_0330(void)
{
	if (Char.room == 3 && level_kind == 6 && Char.curr_row == 1 && is_dead_frame(Char.frame)) sv.rect[2] = 0x77;
}

/* ---- traps (resident 186A) ---- */
/* 186A:0008 (falling objects of type 4, the blades' pieces): one in the left room's column 10 moves into the drawn
 * room, one elsewhere ends (speed -1); a live one is two objects (types 0x84 and 0x85, the second 4 left and 7 lower) */
void draw_mob_186a_0008(void)
{
	int8_t col = (int8_t)(cur_mob.x < 0 ? -((-cur_mob.x) >> 5) : cur_mob.x >> 5);
	if (cur_mob.room != drawn_room) {
		if (cur_mob.room == room_L && col == 10) { cur_mob.room = drawn_room; col = 0; cur_mob.x = (int16_t)(cur_mob.x - 0x140); }
		else cur_mob.speed = -1;
	}
	if (cur_mob.speed == -1) return;
	sv.key = (uint8_t)(row_tilepos((int8_t)cur_mob.row) + col);   /* 0AFF:07D4 + col */
	render_mob_obj(cur_mob.y);
	cur_mob.type = 5; cur_mob.x = (int16_t)(cur_mob.x - 4); cur_mob.y = (int16_t)(cur_mob.y + 7);
	render_mob_obj(cur_mob.y);
	cur_mob.type = 4; cur_mob.x = (int16_t)(cur_mob.x + 4); cur_mob.y = (int16_t)(cur_mob.y - 7);
}
/* 186A:038C (objects of types 0x84 / 0x85): the piece's image from DS:0CD4 / DS:0CE6 by its id (0xF0: 8, bit 7: 5,
 * else the low byte) */
void obj_hook_186a_038c(uint8_t type)
{
	uint16_t id = (uint16_t)obj_id;
	if (id == 0xF0) id = 8; else if (id & 0x80) id = 5; else id &= 0xFF;
	obj_id = (int16_t)id;
	uint16_t si = type == 0x84 ? ds_word((uint16_t)(0x0CD4 + 2 * id)) : type == 0x85 ? ds_word((uint16_t)(0x0CE6 + 2 * id)) : 0;
	if (si) add_sprite(4, si, sv.x0, 10, obj_y);
}

/* ---- the temple (kind 2, OVL07 at 347C): moving walls (falling objects 6..8) and slabs (9, 10) ---- */
static void ds_put(uint16_t a, int16_t v) { render_mob_box_set(a, v); }   /* (the falling objects' box sizes DS:0810 / 082A the drawing rewrites) */
static int8_t col_of(int16_t x) { return (int8_t)(x < 0 ? -((-x) >> 5) : x >> 5); }
static int8_t key_of(int8_t row, int8_t col) { int8_t t = tile_index_of(row, col); return t < 0 ? 0x1E : t; }   /* 0AFF:026A */
/* 1375:2620: tile `key` of `room` shows: the drawn room, the left room's column 9, the room below's row 0, the room
 * below-left's tile 9 */
static int mob_shows(uint8_t room, int8_t key)
{
	if (room == drawn_room) return 1;
	if (room == room_L && key % 10 == 9) return 1;
	if (room == room_B && key < 10) return 1;
	return room == room_BL && key == 9;
}
/* 347C:0732 (the wall's front, dist pixels ahead, drow rows off: its tiles all solid, not 0x19) (walls.c has it too) */
static int wall_front_solid(int16_t dist, int8_t drow)
{
	int si = 1; int8_t c = col_of((int16_t)(cur_mob.x + 6));
	int16_t d = cur_mob.wd - cur_mob.x; if (dist < d) d = dist;
	int8_t c2 = col_of((int16_t)(d + cur_mob.x)), r = (int8_t)(cur_mob.row + drow);
	while (c2 >= c && si) { uint8_t t = get_tile(r, c++, cur_mob.room); si = !empty_28d4(t) && t != 0x19; }
	return si;
}
/* 347C:0024: the wall's body pieces (the current type) every 0x18 from x up to its right end */
static void wall_pieces(void)
{
	int8_t row = (int8_t)cur_mob.row; int16_t si = cur_mob.wd;
	while (si > cur_mob.x) {
		sv.key = (uint8_t)(row_tilepos(row) + col_of(cur_mob.x)); if (cur_mob.type == 8) sv.key++;
		render_mob_obj(cur_mob.y);
		cur_mob.x = (int16_t)(cur_mob.x + 0x18);
	}
}
/* 347C:0084 (falling objects 6..8: a moving wall): its front (type 7, with the fore layer over its top part while
 * the tiles ahead are not solid), its edge (6) and body pieces (8, 9; offsets DS:18DC..18E6), the objects' box sizes
 * rewritten meanwhile */
static void temple_wall_mob(void)
{
	if (cur_mob.speed == -1) return;
	int8_t row = y_to_row(cur_mob.y), col = col_of(cur_mob.x);
	sv.key = (uint8_t)key_of(row, col);
	if (!mob_shows(cur_mob.room, (int8_t)sv.key)) return;
	int16_t h7 = render_mob_box(0x081E), ax;
	if (!wall_front_solid(0x34, -1)) {
		int16_t di = cur_mob.y; cur_mob.y = (int16_t)(cur_mob.y - 0x3B);
		int16_t si = render_mob_box(0x0836), w = (int16_t)(cur_mob.wd - cur_mob.x);
		ds_put(0x0836, w > si ? si : w);
		int16_t w8 = render_mob_box(0x0838); ds_put(0x0838, w > w8 ? w8 : w);
		cur_mob.x = (int16_t)(cur_mob.x + 5); ds_put(0x081C, 9); ds_put(0x081E, 9);
		render_mob_mark(2);
		cur_mob.y = di; cur_mob.x = (int16_t)(cur_mob.x - 5); ds_put(0x0836, si); ds_put(0x0838, si);
		ax = h7;
	} else ax = 0x3A;
	ds_put(0x081C, ax); ds_put(0x081E, ax);
	cur_mob.type = 7; render_mob_obj(cur_mob.y);
	int16_t si = cur_mob.x, di = cur_mob.y;
	cur_mob.x = (int16_t)(cur_mob.x + 0x20);
	if ((uint16_t)cur_mob.wd > (uint16_t)cur_mob.x) { sv.key = (uint8_t)(row_tilepos(row) + col_of(cur_mob.x)); cur_mob.type = 6; cur_mob.y--; render_mob_obj(cur_mob.y); }
	cur_mob.x = (int16_t)(ds_word(0x18E6) + si);
	if ((uint16_t)cur_mob.x < (uint16_t)cur_mob.wd) { cur_mob.y = (int16_t)(ds_word(0x18E4) + di); cur_mob.type = 8; wall_pieces(); }
	cur_mob.x = (int16_t)(ds_word(0x18DE) + si);
	if ((uint16_t)cur_mob.x < (uint16_t)cur_mob.wd) {
		cur_mob.y = (int16_t)(ds_word(0x18DC) + di); cur_mob.type = 9; wall_pieces();
		cur_mob.x = (int16_t)(ds_word(0x18E2) + si); cur_mob.y = (int16_t)(ds_word(0x18E0) + di); wall_pieces();
	}
	cur_mob.x = si; cur_mob.y = di; cur_mob.type = 6;
	ds_put(0x081C, h7); ds_put(0x081E, h7);
}
/* 347C:0238: the box of the moving wall on (room, row): 0x45 above its y to its y, 0x34 wide from its x; 0 none */
int temple_wall_rect(int8_t row, uint8_t room, int16_t *r)
{
	if (level_kind != 2) return 0;
	mob_type *w = wall_find(room, row); if (!w) return 0;
	r[0] = (int16_t)(w->y - 0x45); r[1] = w->x; r[2] = (int16_t)(w->y + 1); r[3] = (int16_t)(r[1] + 0x34);
	return 1;
}
/* 347C:02D6 (objects 0x86..0x89, the wall's pieces): cut at the wall's right end (DS:6109); images 0x4F (edge), 0x50
 * (front), 0x56 + / 0x51 + (the distance to the end) % 5 (body) */
static void temple_wall_obj(uint8_t type)
{
	mob_type *w = wall_find(drawn_room, (int8_t)(obj_y / 0x3F));
	int16_t di = w ? w->wd : 0x140;
	sv.rect[3] = di; di = (int16_t)(di - obj_x);
	int16_t si;
	switch (type) { case 0x86: si = 0x4F; break; case 0x87: si = 0x50; break; case 0x88: si = (int16_t)(di % 5 + 0x56); break; case 0x89: si = (int16_t)(di % 5 + 0x51); break; default: si = -1; }
	if (si != -1) add_sprite(4, (uint16_t)si, obj_x, 10, obj_y);
}
/* 347C:0C22 (falling objects 9 and 10, a slab): from a neighbour room in the drawn room's coordinates; settled (step
 * > 0x10) it becomes tile 0x1A again (speed -1; its sounds are left out); the back layers under it redrawn (its box
 * sizes from its image: DS:0824 / 083E), and while it moves the fore layer (2) over it and the object */
static void temple_slab_mob(void)
{
	int16_t di = cur_mob.x, si = cur_mob.y; uint8_t room = cur_mob.room;
	if (room != drawn_room) {
		if (room == room_B) { si = (int16_t)(si + 0xBF); room = drawn_room; }
		else if (room == room_A) { si = (int16_t)(si - 0xBF); room = drawn_room; }
		else if (room == room_L) { di = (int16_t)(di - 0x140); room = drawn_room; }
		else if (room == room_R) { di = (int16_t)(di + 0x140); room = drawn_room; }
	}
	int8_t col = col_of(di);
	int8_t k = key_of((int8_t)(si / 0x3F), col);
	if (k != 0x1E) { if (col != 0) k--; if (k < 0x14) k = (int8_t)(k + 10); }
	sv.key = (uint8_t)k;
	int8_t tp = (int8_t)(row_tilepos((int8_t)cur_mob.row) + col_of(cur_mob.x));
	int st = cur_mob.wd & 0x1F;
	if (st > 0x10) {
		cur_mob.speed = -1;
		*(uint8_t *)&ROOM_ATTRS(cur_mob.room)[tp] = 0; ROOM_TILES(cur_mob.room)[tp] = 0x1A;
		st = 0x11;   /* (sounds 0x276F stopped and 3 played: left out) */
	}
	if (!mob_shows(room, (int8_t)(row_tilepos((int8_t)cur_mob.row) + col))) return;
	int16_t sy = cur_mob.y, sx = cur_mob.x, dy = (int16_t)ds_word((uint16_t)(0x1908 + 2 * st));
	cur_mob.y = (int16_t)(cur_mob.y + dy); si = (int16_t)(si + dy);
	di = (int16_t)(di + (int16_t)ds_word((uint16_t)(0x18E6 + 2 * st))); cur_mob.x = di;
	const image_t *im = render_image(4, (int16_t)ds_word((uint16_t)(0x192A + 2 * st)));   /* 0993:0FE2 (image - 1, 0-based) */
	if (im) {
		ds_put(0x0824, (int16_t)(im->height + 1));
		int16_t w = (int16_t)(im->width + 1), dx = (int16_t)((col << 5) - cur_mob.x + 0x39);
		if (dx > w) w = dx;
		ds_put(0x083E, w);
	}
	render_mob_mark(0);
	if (cur_mob.speed != -1) {
		int16_t r[4]; render_mob_rect(r); r[1] = (int16_t)(r[1] + 0x10);
		mark_tiles_under(mark_fore_part, r, 0xFF);
		render_mob_obj(si);
	}
	cur_mob.x = sx; cur_mob.y = sy;
}
/* 347C:0F5E (object 0x8A, the slab): image DS:192A[step]; 0x15 (with 0x16 in front of it) */
static void temple_slab_obj(void)
{
	int16_t si = (int16_t)ds_word((uint16_t)(0x192A + 2 * (obj_id & 0x1F)));
	if (si == 0x15) { add_sprite(4, 0x15, (int16_t)(obj_x + 1), 10, (int16_t)(obj_y - 2)); add_sprite(4, 0x16, obj_x, 0, (int16_t)(obj_y - 1)); }
	else add_sprite(4, (uint16_t)si, obj_x, 10, (int16_t)(obj_y - 1));
}
void draw_mob_347c_0084(void) { if (level_kind == 2) temple_wall_mob(); else note_missing("347C:0084"); }
void draw_mob_347c_0c22(void) { if (level_kind == 2) temple_slab_mob(); else note_missing("347C:0C22"); }
void obj_hook_347c_02d6(uint8_t type) { if (level_kind == 2) temple_wall_obj(type); else note_missing("347C:02D6"); }
void obj_hook_347c_0f5e(uint8_t type) { (void)type; if (level_kind == 2) temple_slab_obj(); else note_missing("347C:0F5E"); }

/* ---- the final level (kind 6, OVL08 at 33FD) ---- */
static uint16_t fireball_image(uint16_t wd) { return (uint16_t)((wd & 8 ? 0x134 : 0x130) + (wd & 7)); }   /* 33FD:1EB0 */
/* 33FD:1BE6 (falling objects of type 12, the spirit's fireball): shown in the drawn room (x 0..0x1C1) or reaching in
 * from the left / right room (its width DS:0842), it becomes an object (0x8C) centred on its point, its size in
 * DS:0828 / 0842 (the fireball image of KID.DAT), the fore layer over it redrawn */
void draw_mob_33fd_1be6(void)
{
	if (level_kind != 6) { note_missing("33FD:1BE6"); return; }
	int vis = 1; int16_t y0 = cur_mob.y, w0 = render_mob_box(0x0842);
	if (cur_mob.room == drawn_room) vis = cur_mob.x <= 0x1C1 && cur_mob.x >= 0;
	else if (cur_mob.room == room_L) { if (w0 + cur_mob.x > 0x140) { cur_mob.x = (int16_t)(cur_mob.x - 0x140); cur_mob.room = drawn_room; } else vis = 0; }
	else if (cur_mob.room == room_R) { if (cur_mob.x - w0 < 0x140) { cur_mob.x = (int16_t)(cur_mob.x + 0x140); cur_mob.room = drawn_room; } else vis = 0; }
	else vis = 0;
	if (!vis) return;
	int8_t col = col_of(cur_mob.x);
	sv.key = (uint8_t)key_of(y_to_row(y0), col);
	const image_t *im = render_image(2, fireball_image((uint16_t)cur_mob.wd) + 1);   /* 0993:0FE2 on the prince's set */
	if (!im) return;
	int16_t si = cur_mob.x, h = (int16_t)im->height, w = (int16_t)im->width;
	cur_mob.x = (int16_t)(cur_mob.x - (w < 0 ? -((-w) >> 1) : w >> 1));
	cur_mob.y = (int16_t)((h < 0 ? -((-h) >> 1) : h >> 1) + y0);
	render_mob_box_set(0x0828, h); render_mob_box_set(0x0842, w);
	render_mob_obj(cur_mob.y);
	render_mob_mark(1);
	cur_mob.y = y0; cur_mob.x = si;
}
/* 33FD:1E72 (object 0x8C, the fireball): its KID.DAT image (palette mask 8), not mirrored */
void obj_hook_33fd_1e72(uint8_t type)
{
	(void)type;
	if (level_kind != 6) { note_missing("33FD:1E72"); return; }
	uint16_t img = fireball_image((uint16_t)obj_id);
	int n = table_counts[3];
	add_sprite(2, (uint16_t)(img + 1), obj_x, 10, obj_y);
	if (table_counts[3] > 0) { sprite_table[table_counts[3] - 1].mask = 8; sprite_table[table_counts[3] - 1].mirror = 0; }   /* (the entry at the count - 1, added or not) */
	(void)n;
}
