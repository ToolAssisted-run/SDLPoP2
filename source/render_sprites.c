/* The frame's characters and falling objects (0FB3:12F4: 1375:1FBA the objects, 0993:029A the characters, 0993:05CA
 * the screens put back): the objects of the frame (DS:5D3A, drawn by render_frame.c) and the redraw requests of the
 * tiles around them. Transcribed from the disassembly (0993:02AE / 0510 / 05CA / 07F8 / 08D0 / 09B6 / 0A8E / 0B98 /
 * 0C04 / 0C3A / 0D40 / 108A, 0AFF:1846 / 15AE / 18C0 / 18F2, 169B:0C48 / 0C64, 1375:2062 / 219E / 2296 / 22DC).
 * The drawing variables are the game's: obj_x / obj_y / obj_id / obj_chtab (DS:60FC..6102) and DS:60FA / 60FB /
 * 6103 / 610B / 610D (spr_vars). Character state changed by the drawing (as in the game): Char, the +0x26 / +0x28 /
 * +0x36 fields saved by 0993:108A, the collision box (3212:09BE), char_top_y. */
#include <stdio.h>
#include <string.h>
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
#include "types.h"
#include "globals.h"
#include "render.h"
#include "render_tiles.h"
#include "render_frame.h"

/* the overlays' drawing hooks (not reconstructed: logged) */
void draw_hook_33fd_0190(void);   /* render_hooks.c */
void draw_hook_33fd_0012(void);   /* render_hooks.c */
void draw_hook_347c_0000(void);   /* render_hooks.c */
void draw_hook_33fd_0330(void);   /* render_hooks.c */
void draw_hook_33fd_0482(void);   /* render_hooks.c */
void draw_hook_33fd_0478(void);   /* render_hooks.c */

static void draw_hook_2f86_052e(void);
void draw_hook_366c_15f6(void);
static void draw_hook_366c_167c(void);
int is_char_dead(void);
static int rect_empty(const int16_t *r) __attribute__((unused)); static int rect_empty(const int16_t *r) { return r[0] >= r[2] || r[1] >= r[3]; }
static void set_rect(int16_t *r, uint16_t ds_addr) { for (int k = 0; k < 4; k++) r[k] = (int16_t)ds_word((uint16_t)(ds_addr + 2 * k)); }
static void char_box(int16_t *r) { r[0] = Char.bbox_top; r[1] = Char.bbox_left; r[2] = Char.bbox_bottom; r[3] = Char.bbox_right; }

/* 0993:02AE (al: the object's type): the drawing variables into the frame's objects (DS:5D3A) */
void add_obj(uint8_t type)
{
	if (obj_count >= OBJ_MAX) return;
	int k = obj_count++; frame_obj *o = &objs[k];
	o->type = type;
	o->x = obj_x < 0 ? (int16_t)(obj_x - 1) : obj_x;   /* 0AFF:08D0 */
	o->y = obj_y; o->chtab = obj_chtab; o->id = (uint16_t)obj_id; o->dir = sv.dir;
	memcpy(o->rect, sv.rect, sizeof o->rect); o->mask = sv.mask;
	o->key = sv.key; if ((int8_t)sv.key < 0x1E) ((uint8_t *)&redraw)[0x6374 - 0x61E4 + (int8_t)sv.key] = 1;   /* 0FB3:1EAA */
	o->frame = Char.frame; o->charidx = Char.index;
}
/* 0AFF:0390: x moved forward by dx (the drawing direction DS:60FB) */
static void obj_dx(int16_t dx) { obj_x = (int16_t)(obj_x + (sv.dir ? -dx : dx)); }
/* 0AFF:1846: the rect DS:6103 of the image at obj_x / obj_y (the bottom row included; one row more above) */
static void obj_rect(void)
{
	if (obj_id == -1) { set_rect(sv.rect, 0x1F12); return; }   /* 0AFF:18AE: no image: the empty rect DS:1F12 */
	render_guard_type = obj_chtab == 3 && (Char.charid == 10 || Char.charid == 12) ? charid_to_type[Char.charid] : 0xFF;   /* (0993:0F36: by the character's type) */
	const image_t *im = render_image(obj_chtab, obj_id + 1);   /* 0993:0FE2 */
	render_guard_type = 0xFF;
	if (!im) { set_rect(sv.rect, 0x1F12); return; }
	sv.rect[0] = (int16_t)(obj_y - im->height); sv.rect[2] = (int16_t)(obj_y + 1);
	if (sv.dir == 0) { sv.rect[3] = obj_x; sv.rect[1] = (int16_t)(obj_x - im->width); }
	else { sv.rect[1] = obj_x; sv.rect[3] = (int16_t)(obj_x + im->width); }
}
static void screen_clip(void) { set_rect(sv.rect, 0x097E); }   /* 0AFF:08BE */
/* 1286:0568: a sword frame's image, -1 none */
static int16_t sword_img(uint16_t sw) { return sw ? (int16_t)(sword_table[sw * 4] | sword_table[sw * 4 + 1] << 8) : -1; }
/* 0993:09B6: the character's image into the drawing variables (the direction, the palette mask; obj_* by the game's
 * load_frame_to_obj, with the heads' 366C:0002 and the riser's 366C:150A) */
static void char_to_obj(void)
{
	screen_clip();
	load_frame();
	int16_t si = sword_img(cur_frame.sword);
	if (Char.frame == 0) return;
	if (cur_frame.image == 0xFFFF && si == -1) return;
	sv.dir = (uint8_t)Char.direction; sv.mask = Char.pal_slot;
	load_frame_to_obj();
}
/* 0AFF:18C0: standing in a tile 6 with frame flag 0x40: on the row's floor line */
static void char_on_floor(void)
{
	if (Char.charid == 0xB || Char.charid == 7 || Char.charid == 8) return;
	if (get_tile_at_char() == 6 && (cur_frame.flags & 0x40)) Char.y = (int16_t)(0x3F * Char.curr_row + 0x39);
}
/* 0AFF:18F2: the tile the character's objects are drawn with (DS:60FA): the bottom row / left column while in
 * action 1, else its row / column; one column forward (DS:0CFA[direction]) in frames 0x87..0x94 and actions 2, 3, 4, 6 */
static void char_key(void)
{
	uint16_t f = Char.frame; int8_t a = (int8_t)Char.action;
	if (a == 1) { tile_row = char_bottom_row; tile_col = char_col_left; }
	else { tile_row = Char.curr_row; tile_col = Char.curr_col; }
	if ((f >= 0x87 && f < 0x95) || a == 2 || a == 3 || a == 4 || a == 6) tile_col = (int8_t)(tile_col + (int8_t)ds_byte((uint16_t)(0x0CFB + Char.direction)));
	int8_t t = tile_index_of(tile_row, tile_col); sv.key = (uint8_t)(t < 0 ? 0x1E : t);   /* 0AFF:026A */
}
/* 169B:0C48: the prince (index 10): the fore layer over his box */
static void kid_fore(void) { if (Char.index == 0xA) { int16_t r[4]; char_box(r); mark_tiles_under(mark_fore, r, 0xFF); } }
/* 169B:0C64: the fore layer (2) in front of the character: climbing (frames 0x87..0x90 facing right: the whole fore
 * of the top-right tile), hanging / jumping actions (2, 3, 4, 6, frames 0x4F / 0x51) and a collapsing skeleton */
static void char_fore(void)
{
	int8_t a = (int8_t)Char.action; uint16_t f = Char.frame; int16_t r[4]; char_box(r);
	if (f >= 0x87 && f < 0x91 && Char.direction == 0) { mark_fore_full(tile_index_of(char_top_row, char_col_right)); return; }
	int all = a == 2 || a == 3 || a == 4 || a == 6 || f == 0x4F || f == 0x51;
	if (!all) {
		if (Char.charid != 7 && Char.charid != 8) return;
		uint16_t g = Char.f19;
		if (!(g == 0x89 || g == 0x8A || g == 0x8F || (int16_t)g <= 0x90)) return;
		for (tile_col = char_col_right; tile_col >= char_col_left; tile_col--) mark_fore_part(tile_index_of(char_top_row, tile_col), r);
		return;
	}
	for (tile_col = char_col_right; tile_col >= char_col_left; tile_col--) {
		if (a != 2) mark_fore_part(tile_index_of(char_bottom_row, tile_col), r);
		if (char_top_row != char_bottom_row && !(Char.f19 == 0x1C && (Char.room == 7 || Char.room == 8) && level_kind == 6))
			mark_fore_part(tile_index_of(char_top_row, tile_col), r);
	}
}
int caverns_wall_open(uint16_t mod);   /* 34C1:136C */
static int empty_type(uint8_t t) { return t == 0 || t == 9 || t == 0x21 || t == 0x23 || t == 0x1B || t == 0x25; }   /* 0FB3:28D4 */
/* 0AFF:15AE: the sprite's clip: its image's box; nothing for the dead; climbing up (frames 0xD9..0xE2) cut to the
 * kind's box of the tile (DS:0806); under a wall above, from the ceiling of the row */
static void char_clip(void)
{
	int si = 1; uint16_t di = Char.frame; int8_t act = (int8_t)Char.action, row = Char.curr_row;
	screen_clip(); obj_rect();
	if (is_char_dead()) { set_rect(sv.rect, 0x1F12); return; }   /* 0AFF:1A64 */
	if (Char.charid == 0 && Char.frame >= 0xD9 && Char.frame <= 0xE2 && 0x3F * Char.curr_row + 0x29 > Char.y) {
		uint8_t t = get_tile(Char.curr_row, Char.curr_col, Char.room);
		int16_t src[4], dst[4]; set_rect(src, ds_word((uint16_t)(0x0806 + 2 * level_kind)));
		if (!render_rect_at_tile(Char.curr_row, (int8_t)(Char.curr_col + (t == 0x10)), src, dst)) return;   /* 17C1:016E */
		sv.rect[0] = dst[0]; sv.rect[3] = 0x140; return;
	}
	if (act == 9) { if (level_kind == 5) draw_hook_33fd_0190(); return; }
	int keep = 1, heads = Char.charid == 7 || Char.charid == 8;
	uint8_t t = get_tile(char_top_row, char_col_left, Char.room);
	if (empty_type(t)) keep = 0;
	else if (level_kind == 3 && t == 0x14 && caverns_wall_open(curr_modifier)) keep = 0;
	else if (curr_room == 1 && level_number == 14) keep = 0;
	else if ((curr_room == 7 || curr_room == 8) && level_kind == 6 && Char.f19 != 0xE) keep = 0;
	else if (!(act == 0 && (di == 0x4F || di == 0x51))) {
		t = get_tile(char_top_row, char_col_right, Char.room);
		if (empty_type(t)) keep = 0;
		else if (t == 0x14 && level_kind == 3 && caverns_wall_open(curr_modifier)) keep = 0;
	}
	if (keep && level_kind != 5 && level_kind != 1 && !(Char.room == 0x10 && level_number == 9) && !(Char.room == 0x1B && level_number == 6)
	    && !(Char.room == 3 && level_kind == 6) && !(Char.room == 6 && level_kind == 6)) {
		int8_t r1 = (int8_t)(row + 1);
		if (row == 0) si = keep;
		else {
			int16_t y = (int16_t)(0x3F * r1 - 0x3C);
			si = y < obj_y && y - 0xF < char_top_y ? keep : 0;
		}
		if (si && !heads) {
			int16_t y = (int16_t)(0x3F * r1 - 0x3C);
			if (y > sv.rect[0]) { char_top_y = y; sv.rect[0] = y; }
		}
	}
	switch (level_kind) {
	case 1: draw_hook_33fd_0012(); break;
	case 4: draw_hook_347c_0000(); break;
	case 5: draw_hook_33fd_0190(); break;
	case 6: draw_hook_33fd_0330(); break;
	}
}
/* 0AFF:1A64: dead (f24 4..7, 0xA, 0xC; on the rooftops out of the sea rooms in a dead frame) */
int is_char_dead(void)
{
	uint16_t s = Char.f24;
	if (s == 4 || s == 5 || s == 6 || s == 7 || s == 0xA || s == 0xC) return 1;
	return level_kind == 5 && Char.room != 0x13 && Char.room != 0x10 && Char.room != 0xF && is_dead_frame(Char.frame);
}
/* 0993:0C04: the prince's body */
static void kid_body(void)
{
	if (Char.frame == 0) return;
	load_fram_det_col(); char_to_obj(); char_on_floor(); set_char_collision(); char_key(); kid_fore(); char_fore(); char_clip();
	add_obj(Char.charid);
}
/* 0993:0A8E: a character's dead body lying (image 0xDA of KID.DAT, lower and moved by frame and kind) */
static void dead_body(void)
{
	uint16_t si = Char.frame; spr_vars save = sv; int16_t sx = obj_x, sy = obj_y, sid = obj_id; uint8_t sch = obj_chtab;   /* 0AFF:1C0A */
	sv.key = 0xFF; obj_chtab = 2; obj_id = 0xDA;
	int16_t dx;
	if (si == 0xB9 || (si >= 0x6A && si < 0x6F)) { obj_y += 4; dx = 0xC; }
	else if (Char.charid == 0xB) { obj_y += 5; obj_dx(-8); obj_chtab = 3; obj_id = 0x77; goto placed; }
	else if (Char.charid == 0xC) { obj_y -= 0xB; dx = -8; }
	else if (Char.charid == 0) {
		if (Opp.charid == 0xB || si == 0x109) dx = 0xA;
		else if (si == 0x8A || si == 0x8B || si == 0x8C) { obj_y -= 0xA; dx = -5; }
		else if (si == 0x152) { obj_y -= 0x1E; dx = 0x1D; }
		else if (si == 0x154) { obj_y -= 0x1E; dx = -0x11; }
		else { obj_y -= 0xF; dx = 0xA; }
	} else { obj_y -= 0xB; dx = 7; }
	obj_dx(dx);
placed:
	obj_rect();
	add_obj(5);
	mark_tiles_under(mark_fore, sv.rect, 0xFF);
	sv = save; obj_x = sx; obj_y = sy; obj_id = sid; obj_chtab = sch;   /* 0AFF:1C1C */
}
/* 0993:0B98 (frames 0x110..0x119): the sword frame's image as an object of type 3 */
static void kid_sword_frame(void)
{
	spr_vars save = sv; int16_t sx = obj_x, sy = obj_y, sid = obj_id; uint8_t sch = obj_chtab;
	if (cur_frame.sword) {
		const uint8_t *e = sword_table + cur_frame.sword * 4;
		obj_id = (int16_t)(e[0] | e[1] << 8); obj_dx((int8_t)e[2]); obj_y += (int8_t)e[3];
		obj_rect(); add_obj(3); mark_tiles_under(mark_fore, sv.rect, 0xFF);
	}
	sv = save; obj_x = sx; obj_y = sy; obj_id = sid; obj_chtab = sch;
}
/* 0993:0C3A (al: the type): the character's sword (image set 0) */
static void char_sword(uint8_t type)
{
	if (is_char_dead()) return;
	uint16_t sw = cur_frame.sword;
	if (sw == 0) return;
	if (!(sw <= 0x90 || (sw >= 0xC8 && sw < 0xFB) || (sw >= 0x124 && sw <= 0x130))) return;
	if (Char.charid == 0 && Char.f10 == 0) {
		uint16_t f = Char.frame;
		if (!(f == 0x85 || (f >= 0xE5 && f <= 0xF0) || f == 0x143 || f == 0x144)) return;
	}
	const uint8_t *e = sword_table + sw * 4;
	obj_id = (int16_t)(e[0] | e[1] << 8);
	if (obj_id == -1) return;
	obj_dx((int8_t)e[2]); obj_y += (int8_t)e[3]; obj_chtab = 0;
	obj_rect();
	if (level_kind == 5) draw_hook_33fd_0190();
	add_obj(type);
	if (Char.action != 9) mark_tiles_under(mark_fore, sv.rect, Char.index);
}
/* 0993:0D40: the sword frames 0x91..0xA8 (image set 1) */
static void char_sword2(void)
{
	uint16_t sw = cur_frame.sword;
	if (sw < 0x91 || sw > 0xA8) return;
	const uint8_t *e = sword_table + sw * 4;
	if ((int16_t)(e[0] | e[1] << 8) == -1) return;
	obj_id = (int16_t)(e[0] | e[1] << 8); obj_dx((int8_t)e[2]); obj_y += (int8_t)e[3]; obj_chtab = 1;
	obj_rect(); add_obj(3);
	mark_tiles_under(mark_fore, sv.rect, Char.index);
}
/* 33FD:1512 (the final level, after each character): a guard (charid 2) in frames 0xE5..0xFE: its sword frame's image
 * (image set 3) as an object of type 9 */
static void draw_hook_33fd_1512(void)
{
	if (Char.charid != 2 || Char.frame < 0xE5 || Char.frame > 0xFE) return;
	const uint8_t *e = sword_table + cur_frame.sword * 4;
	if ((int16_t)(e[0] | e[1] << 8) == -1) return;
	obj_id = (int16_t)(e[0] | e[1] << 8); obj_dx((int8_t)e[2]); obj_y += (int8_t)e[3]; obj_chtab = 3;
	obj_rect(); add_obj(9);
	mark_tiles_under(mark_fore, sv.rect, Char.index);
}
/* 2F86:04AE: the shadow's spirit shows: +0x24 is 0xD, or on the final level (kind 6) more than 2 hit points */
static int spirit_shows(void) { return Char.f24 == 0xD || (level_kind == 6 && (int8_t)Char.f12 > 2); }
/* 2F86:052E (OVL, the shadow, charid 1): its spirit, an object of type 0xE: image 0x122 + tick % 9 (2812:1DC4) of
 * the character's image set, moved forward by 13 - image width / 2 (DS:6114), the fore layer marked over it */
static void draw_hook_2f86_052e(void)
{
	if (!spirit_shows()) return;
	spr_vars save = sv; int16_t sx = obj_x, sy = obj_y, sid = obj_id; uint8_t sch = obj_chtab;   /* 0AFF:1C0A */
	obj_id = (int16_t)((int32_t)tick % 9 + 0x122);
	int16_t w = image_width; obj_dx((int16_t)(13 - w / 2));
	obj_rect();
	add_obj(0xE);
	mark_tiles_under(mark_fore, sv.rect, Char.index);
	sv = save; obj_x = sx; obj_y = sy; obj_id = sid; obj_chtab = sch;   /* 0AFF:1C1C */
}
/* 366C:167C (OVL10, charid 0xC): for the frames' sword entries 0xAB..0xC7 (DS:5CC8) the sword table's image (FRAM,
 * DS:6110: +0 id, +2 dx, +3 dy) of the guard's image set (chtab 3) as an object of type 9, the fore layer marked */
static void draw_hook_366c_167c(void)
{
	uint16_t sw = cur_frame.sword;
	if (sw < 0xAB || sw > 0xC7) return;
	const uint8_t *e = sword_table + sw * 4;
	if ((e[0] | e[1] << 8) == 0xFFFF) return;
	obj_id = (int16_t)(e[0] | e[1] << 8);
	obj_dx((int8_t)e[2]);
	obj_y = (int16_t)(obj_y + (int8_t)e[3]); obj_chtab = 3;
	obj_rect();
	add_obj(9);
	mark_tiles_under(mark_fore, sv.rect, Char.index);
}
/* 0993:07F8: the prince (Char = Kid, not saved back) */
static void draw_kid(void)
{
	Char = Kid;   /* 0AFF:1D60 */
	if (Char.room == 0 || Char.room != drawn_room) { if (level_kind == 5) draw_hook_33fd_0482(); return; }
	kid_body();
	if (Char.action == 0x63 || Char.f24 == 8) {
		dead_body();
		if (Char.f24 == 8) Char.f24 = 0; else Char.action = 5;
	}
	if (level_kind == 5) draw_hook_33fd_0482();
	else if (level_kind == 1) draw_hook_33fd_0478();
	else if (level_number == 5 && Char.room == 3 && Char.frame == 0x127) draw_hook_37f0_044a();
	else if (Char.frame >= 0x132 && Char.frame <= 0x13E) draw_hook_37f0_05fc();
	else if (Char.frame >= 0x110 && Char.frame <= 0x119) kid_sword_frame();
	if (Char.charid == 1 && level_kind == 6) draw_hook_2f86_052e();
	char_sword(3);
	char_sword2();
}
/* 0993:059E: the saved screen of the n-th slot of id `id` is kept (not put back) */
static void keep_saved(uint8_t id, int n)
{
	int k = 0;
	for (int i = 0; i < saved_count; i++) if (saved_bgs[i].id == id && ++k == n) { saved_bgs[i].flag = 1; return; }
}
/* 0993:108A: a character's body; 0 when it has not changed since the last drawing (same frame, x, y, not asked to be
 * redrawn: its saved screens are kept) */
static int char_body(void)
{
	load_fram_det_col(); char_to_obj(); char_on_floor(); set_char_collision(); char_key(); char_fore(); char_clip();
	uint8_t *c = (uint8_t *)&Char;
	if (Char.f23 == 0 && Char.frame == Char.f2a && Char.x == (int16_t)Char.f26 && Char.y == (int16_t)Char.f28 && (c[0x36] | c[0x37]) == 0
	    && Char.charid != 0xA && Char.charid != 0xB) {
		keep_saved(Char.index, 1); keep_saved(Char.index, 2);
		return 0;
	}
	uint8_t type = (Char.charid == 7 || Char.charid == 8 || Char.charid == 0xA || Char.charid == 6 || Char.charid == 1) ? Char.charid : 2;
	if (Char.charid == 0xA) draw_hook_366c_15f6();
	if (Char.action != 9 && !is_char_dead()) { int16_t r[4]; char_box(r); mark_tiles_under(mark_fore, r, Char.index); }
	add_obj(type);
	Char.f26 = (uint16_t)Char.x; Char.f28 = (uint16_t)Char.y; c[0x36] = (uint8_t)redraw_all_flag; c[0x37] = (uint8_t)(redraw_all_flag >> 8);
	save_char();
	return 1;
}
/* 0993:08D0: the other characters of the drawn room */
static void draw_chars(void)
{
	int n = room_nchars(drawn_room);
	for (int i = 0; i < n; i++) {
		load_char(i);
		if (Char.room != drawn_room) { if (level_kind == 5) draw_hook_33fd_0482(); continue; }
		if (!char_body()) continue;
		if (Char.action == 0x63 || Char.f24 == 8 || ((int8_t)Char.action == 9 && Char.frame == 0xD5)) {
			if (Char.charid != 7 && Char.charid != 8) {
				if (Char.action == 0x63) Char.action = 5;
				dead_body();
			}
		}
		if (Char.charid != 0xA && Char.charid != 1) char_sword(9);
		if (Char.charid == 0xC) draw_hook_366c_167c();
		else if (Char.charid == 1) draw_hook_2f86_052e();
		else if (level_kind == 5) draw_hook_33fd_0482();
		else if (level_kind == 6) draw_hook_33fd_1512();
	}
}
/* 0993:05CA: before the saved screens not kept are put back: the fore layer over them (not for a character that is
 * dead or in action 9) */
static void mark_restored(void)
{
	for (int i = saved_count - 1; i >= 0; i--) {
		saved_bg *b = &saved_bgs[i];
		if (b->flag) continue;
		int go = 1;
		if ((int8_t)b->id >= 0 && (int8_t)b->id < 5) {
			load_char(b->id);
			if (Char.action == 9 || is_char_dead()) go = 0;
		}
		if (go) mark_tiles_under(mark_fore, b->rect, b->id);
	}
}

/* ---- the falling objects (1375:1FBA, by type: DS:201E) ---- */
/* DS:0810 / 082A: the falling objects' heights / widths by type (the temple's drawing rewrites some: kept here) */
static int16_t box_over[0x1A]; static uint32_t box_set;
int16_t render_mob_box(uint16_t a) { int k = (a - 0x0810) / 2; return k >= 0 && k < 0x1A && (box_set & (1u << k)) ? box_over[k] : (int16_t)ds_word(a); }
void render_mob_box_set(uint16_t a, int16_t v) { int k = (a - 0x0810) / 2; if (k >= 0 && k < 0x1A) { box_over[k] = v; box_set |= 1u << k; } }
void render_mob_box_reset(void) { box_set = 0; }
static int16_t mob_height(uint8_t t) { return render_mob_box((uint16_t)(0x0810 + 2 * t)); }
static int16_t mob_width(uint8_t t) { return render_mob_box((uint16_t)(0x082A + 2 * t)); }
/* 1375:219E: a falling floor's box (height DS:0810[type], width DS:082A[type]), moved from a neighbour room's
 * coordinates to the drawn room's (not shown: the empty rect) */
void render_mob_rect(int16_t *r)
{
	r[0] = (int16_t)(cur_mob.y - mob_height(cur_mob.type)); r[1] = cur_mob.x; r[2] = (int16_t)(cur_mob.y + 1); r[3] = (int16_t)(mob_width(cur_mob.type) + r[1]);
	uint8_t m = cur_mob.room;
	if (m == drawn_room) return;
	int16_t dx = 0, dy = 0;
	if (m == room_L || m == room_AL || m == room_BL || m == room_R || m == room_AR || m == room_BR) dx = -0x140;   /* (the rooms right too) */
	if (m == room_A || m == room_AL || m == room_AR) dy = -0xC0;
	else if (m == room_B || m == room_BL || m == room_BR) dy = 0xC0;
	if (!dx && !dy) { set_rect(r, 0x1F12); return; }
	r[0] += dy; r[2] += dy; r[1] += dx; r[3] += dx;   /* 194C:50EC */
}
/* 1375:2296: the tiles under the falling floor: 1 the fore layer (1375:0F16), 2 the fore layer (2) (0E12), else the
 * back layers (0DC6) */
void render_mob_mark(int how)
{
	int16_t r[4]; render_mob_rect(r);
	mark_tiles_under(how == 1 ? mark_fore : how == 2 ? mark_fore_part : mark_back, r, 0xFF);
}
/* 1375:22DC: the falling floor as an object (type 0x80 + its type, image set 4, the image from +0xB) */
void render_mob_obj(int16_t y)
{
	if (obj_count >= OBJ_MAX) return;
	int k = obj_count++; frame_obj *o = &objs[k];
	o->type = (uint8_t)(cur_mob.type | 0x80); o->x = cur_mob.x; o->y = y; o->chtab = 4; o->id = (uint16_t)cur_mob.wd;
	render_mob_rect(o->rect);
	o->mask = 0; o->frame = 0; o->charidx = 0xFF;
	o->key = sv.key; if ((int8_t)sv.key < 0x1E) ((uint8_t *)&redraw)[0x6374 - 0x61E4 + (int8_t)sv.key] = 1;   /* 0FB3:1EAA */
}
/* 1375:2062: a falling floor (types 0, 1, 3) shown in the drawn room or reaching in from the room below, above or
 * left: the fore layer over it, the object, and the fore layer (2) of the tile right of it (and above, spanning two rows) */
static void draw_floor_mob(void)
{
	int vis = 1; int16_t di = cur_mob.y, h = mob_height(cur_mob.type);
	if (cur_mob.room == drawn_room) vis = h + 0xBF > di;
	else if (cur_mob.room == room_B) { if (h > di) di = (int16_t)(di + 0xBF); else vis = 0; }
	else if (cur_mob.room == room_A) { if (0xBF - h > di) vis = 0; else di = (int16_t)(di - 0xBF); }
	else if (cur_mob.room == room_L) {
		if (mob_width(cur_mob.type) + cur_mob.x > 0x140) { cur_mob.x = (int16_t)(cur_mob.x - 0x140); cur_mob.room = drawn_room; }
		else vis = 0;
	} else vis = 0;
	if (!vis) return;
	int8_t col = (int8_t)(cur_mob.x < 0 ? -((-cur_mob.x) >> 5) : cur_mob.x >> 5), row = y_to_row(di);
	int8_t t = tile_index_of(row, col); sv.key = (uint8_t)(t < 0 ? 0x1E : t);   /* 0AFF:026A */
	render_mob_mark(1);
	int16_t r[4]; render_mob_rect(r);
	render_mob_obj(di);
	col++;
	if (cur_mob.room != room_B) mark_fore_part(tile_index_of(row, col), r);
	int8_t row2 = y_to_row((int16_t)(di - h));
	if (row2 != row) mark_fore_part(tile_index_of(row2, col), r);
}
void draw_mob_186a_0008(void);   /* traps (4), render_hooks.c */
void draw_mob_33fd_01a2(void);   /* 2, 5 (render_hooks.c) */
void draw_mob_347c_0084(void);   /* 6..8 (render_hooks.c) */
void draw_mob_347c_0c22(void);   /* 9, 10 (render_hooks.c) */

void draw_mob_33fd_1be6(void);   /* 12 (render_hooks.c) */
/* 1375:200C: by type (the table at 1375:201E) */
static void draw_mob(void)
{
	switch (cur_mob.type) {
	case 0: case 1: case 3: draw_floor_mob(); break;
	case 2: case 5: draw_mob_33fd_01a2(); break;
	case 4: draw_mob_186a_0008(); break;
	case 6: case 7: case 8: draw_mob_347c_0084(); break;
	case 9: case 10: draw_mob_347c_0c22(); break;
	case 11: draw_mob_37f0_0782(); break;
	case 12: draw_mob_33fd_1be6(); break;
	}
}
/* 1375:1FBA: every falling object (a copy in cur_mob, written back) */
static void draw_mobs(void)
{
	for (int i = 0; i < (int16_t)mob_count; i++) { cur_mob = mobs[i]; draw_mob(); mobs[i] = cur_mob; }
}

/* 0FB3:12F4: the frame's tables: the falling objects, the characters (0993:029A), the screens to put back, the tile
 * pass (0FB3:1308) */
void render_frame_objects(void)
{
	memset(table_counts, 0, sizeof table_counts); obj_count = 0;   /* (169B:0A8A: DS:60F0..60F9) */
	draw_mobs();
	draw_kid(); draw_chars(); screen_clip(); render_status_hp();   /* 0993:029A: 07F8, 08D0, 0AFF:08BE, 0823:0F38 */
	mark_restored();
}
void render_frame_tables(void) { render_frame_objects(); redraw_requested(); }
