/* The drawing parts of level 5's room overlays at 37F0 (the caverns' tile drawers DS:[6188] point there for tiles
 * 0x12, 0x1B and 0x2C, and the resident drawing calls 37F0 directly): OVL11, room 3 (description 0: the pit trap and
 * the mouth in the ceiling), OVL12, rooms 7 / 10 / 12 (description 0x21: the water, the bubbles). Transcribed from
 * the relocated overlays (~/pop2dec/image/ovl11_37F0.bin, ovl12_37F0.bin). */
#include <string.h>
#include "types.h"
#include "globals.h"
#include "render.h"
#include "render_tiles.h"
#include "render_frame.h"

static int16_t rd(const uint8_t *p) { return (int16_t)(p[0] | p[1] << 8); }
static void wr(uint8_t *p, int16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void obj_rect_get(const uint8_t *o, int16_t *r) { for (int k = 0; k < 4; k++) r[k] = rd(o + 0xB + 2 * k); }
void add_obj(uint8_t type);   /* 0993:02AE (render_sprites.c) */

/* ---- OVL11: level 5, room 3 ---- */
/* The room's heap block DS:2B76 (allocated by the enter hook 37F0:0000 -> 0406, freed by the leave hook 0622): ten
 * image handles (six bytes each), +0x3C the lifted / caught counter (the core's lever5_flag3c: the drawing counts it
 * up, game.c), +0x3E caught (lever5_flag3e), +0x40 the screen under image 0 saved (the drawing's own). The images
 * 0x62D7 + n (n 0..9) are built by 37F0:002A from a copy of image set 2's shape list header (KID.DAT, first 25002)
 * with its mask | 4: resource 25002 + 0x12E + n - 1 = 25303 + n (CAVERNS.DAT), mask 6 (colors 0x10 / 0x20: the
 * palette 25303 at 0x10 of the enter hook). */
static int lever_block(void) { return level_number == 5 && render_desc_bg() == 0; }   /* DS:2B76 != 0 (description 0 loaded) */
static int lever_saved;   /* DS:2B76 +0x40 */
void render_lever5_set_saved(int v) { lever_saved = v != 0; }   /* (the tests: from the game's block) */
void render_lever5_images(void)   /* (with the description, render_desc.c) */
{
	for (int n = 0; n < 10; n++) { render_register_image(4, 0x62D7 + n, "CAVERNS.DAT", 25303 + n); render_image_set_mask(4, 0x62D7 + n, 6); }
}
/* 37F0:0360: the frame of the lifting / catching animation */
static int lever_frame(void)
{
	int16_t c = (int16_t)lever5_flag3c;
	if (!lever5_flag3e) return (c / 2) % 9;
	return c + 8 > 9 ? 9 : c + 8;
}
static const image_t *lever_image(int n) { return render_image(4, 0x62D7 + n); }   /* 37F0:03AC */
/* 37F0:012A (tile 0x12, the trap, in a whole redraw, layer 0, before the lift): image 0 at DS:1C30 / DS:1C2E as a
 * back entry, its rect the image's box */
void render_lever5_tile12(tile_args *a)
{
	if (!lever_block() || lever5_flag3c != 0 || a->layer != 0 || !redraw_all_flag || table_counts[0] >= BACK_MAX) return;
	draw_entry *e = &back_table[table_counts[0]++];
	e->y = (int16_t)ds_word(0x1C2E); e->x = (int16_t)ds_word(0x1C30); e->id = 0x62D7; e->chtab = 4;
	e->col = (uint8_t)draw_col; e->row = (uint8_t)draw_row;
	const image_t *im = lever_image(0);
	e->top = e->y; e->left = e->x; e->bottom = (int16_t)(e->y + (im ? im->height : 0)); e->right = (int16_t)(e->x + (im ? im->width : 0));   /* 194C:13B3 */
	e->mode = 0xA; e->piece = cur_tile.tile; e->mirror = 0;   /* (DS:6B72) */
}
/* 37F0:06EE (tile 0x1B, the mouth, layer 0xB, drawn from its own column): object (modifier & 7) + 2, outside a whole
 * redraw with the whole screen as the clip */
void render_lever5_tile1b(tile_args *a)
{
	if (a->layer != 0xB || a->col != draw_col) return;
	int16_t save[4]; memcpy(save, draw_clip, sizeof save);
	if (!redraw_all_flag) memcpy(draw_clip, screen_rect, sizeof save);
	draw_object((a->mod & 7) + 2, 0xB);
	if (!redraw_all_flag) memcpy(draw_clip, save, sizeof save);
}
/* 37F0:01E6 (0FB3:0D3B, drawing an entry of image 0x62D7..0x62E2): once (+0x40), the screen under image 0 at
 * DS:1C30 / 1C2E is saved (0993:04F0, id 0x65, kind 2) */
void render_lever5_entry(void)
{
	if (!lever_block() || lever_saved) return;
	const image_t *im = lever_image(0); if (!im) return;
	int16_t x = (int16_t)ds_word(0x1C30), y = (int16_t)ds_word(0x1C2E);
	render_save_under(x, (int16_t)(x + im->width), y, (int16_t)im->height, 0x65, 2);
	lever_saved = 1;
}
/* 37F0:044A (0993:07F8: the prince on frame 0x127 in room 3, lifted): his object (type 0xD) is the animation's image
 * DS:1C32[k] + 1 at DS:1C52 / 1C50[k] from his position (20 more to the left when mirrored), its tiles' foreground
 * redrawn; DS:60FA.. kept around it (0AFF:1C0A / 1C1C) */
void draw_hook_37f0_044a(void)
{
	if (!lever_block() || lever5_flag3c == 0) return;
	spr_vars sv0 = sv; int16_t ox = obj_x, oy = obj_y, oid = obj_id; uint8_t och = obj_chtab;
	int k = lever_frame();
	obj_id = (int16_t)(ds_word((uint16_t)(0x1C32 + 2 * k)) + 0x62D8);
	obj_y = (int16_t)(obj_y + (int16_t)ds_word((uint16_t)(0x1C50 + 4 * k)));
	obj_x = (int16_t)(obj_x + (int16_t)ds_word((uint16_t)(0x1C52 + 4 * k)));
	if (sv.dir == 0) obj_x = (int16_t)(obj_x - 0x14);
	for (int q = 0; q < 4; q++) sv.rect[q] = (int16_t)ds_word((uint16_t)(0x097E + 2 * q));   /* 0AFF:08BE */
	const image_t *im = lever_image(obj_id - 0x62D7);
	if (im) { sv.rect[0] = (int16_t)(obj_y - im->height); sv.rect[1] = obj_x; sv.rect[2] = obj_y; sv.rect[3] = (int16_t)(obj_x + im->width); }   /* 194C:13B3 */
	add_obj(0xD);
	mark_tiles_under(mark_fore, sv.rect, 0xFF);
	sv = sv0; obj_x = ox; obj_y = oy; obj_id = oid; obj_chtab = och;
}
/* 37F0:023A (0FB3:1C32, an object of type 0xD): the image as a table-3 sprite (its bottom at obj_y), the first
 * object moved down DS:1C46[k]; caught: the palette 25303 sub-palette +0x3C - 4 (below 8) at 0x10; the counter
 * (core); the screen under image 1's description object put back next frame once image 0's was saved */
void obj_hook_37f0_023a(uint8_t type)
{
	(void)type;
	if (!lever_block() || lever5_flag3c == 0 || table_counts[3] >= SPRITE_MAX) return;
	int k = lever_frame();
	int n = (int16_t)ds_word((uint16_t)(0x1C32 + 2 * k)) + 1;
	const image_t *im = lever_image(n);
	sprite_entry *s = &sprite_table[table_counts[3]];
	s->x = obj_x; s->y = (int16_t)(obj_y - (im ? im->height : 0)); s->chtab = 4; s->id = (uint16_t)(0x62D7 + n);
	memcpy(s->rect, sv.rect, sizeof s->rect); s->mode = 0xA; s->mask = 0; s->mirror = 0; s->layer = 0xFF;
	sprite_guard[table_counts[3]] = 0xFF;
	int8_t dy = (int8_t)ds_byte((uint16_t)(0x1C46 + k));
	if (dy) { objs[0].y = (int16_t)(objs[0].y + dy); objs[0].rect[0] = (int16_t)(objs[0].rect[0] + dy); objs[0].rect[2] = (int16_t)(objs[0].rect[2] + dy); }   /* DS:5D3C, 194C:50EC on DS:5D45 */
	if (lever5_flag3e && (uint8_t)(lever5_flag3c - 4) < 8) render_pal_load((uint8_t)(lever5_flag3c - 4), 0x20, 0x10, 25303);   /* 0FB3:2B1C */
	table_counts[3]++;
	if (lever_saved) { render_desc_restore_obj(1); lever_saved = 0; }   /* 0CD6:0684(1) */
}
/* 37F0:0756 (the mouth's animation step, tick time): the tiles under DS:1C78 at the animated tile */
void render_lever5_mouth_tick(void) { int16_t r[4]; if (render_trob_rect(0x1C78, r)) mark_tiles_under(mark_back, r, 0xFF); }
/* 37F0:053B (the trap's tile removed at frame 0x127, tick time): the tiles under image 0's box */
void render_lever5_trap_tick(void)
{
	if (!lever_block()) return;
	const image_t *im = lever_image(0); if (!im) return;
	int16_t y = (int16_t)ds_word(0x1C2E), x = (int16_t)ds_word(0x1C30), r[4] = {y, x, (int16_t)(y + im->height), (int16_t)(x + im->width)};
	mark_tiles_under(mark_back, r, 0xFF);
}

/* ---- OVL12: level 5's water (rooms 7, 10, 12; description 0x21 in room 10) ---- */
/* 37F0:0654 / 0698: description object i lowered by the wave height of column col (DS:1C87) / put back */
static int16_t wave_lower(int i, int col)
{
	uint8_t *o = desc_obj(i); int16_t y = rd(o + 1), dy = (int8_t)ds_byte((uint16_t)(0x1C87 + col));
	wr(o + 1, (int16_t)(y + dy)); desc_obj_offset(o, 0, dy);
	return y;
}
static void wave_restore(int i, int16_t y)
{
	uint8_t *o = desc_obj(i); int16_t dy = (int16_t)(y - rd(o + 1));
	wr(o + 1, y); desc_obj_offset(o, 0, dy);
}
/* 37F0:0610 (tile 0x2C, the water, layers 5 and 2): object (modifier & 0xF) + 3 at the tile, lowered by the column's
 * wave height */
void render_water_tile2c(tile_args *a)
{
	if (a->layer != 5 && a->layer != 2) return;
	int i = (a->mod & 0xF) + 3;
	if (i >= desc_count()) return;
	int16_t y = wave_lower(i, a->col);
	desc_draw_obj_at(a, i);   /* 0CD6:007A */
	wave_restore(i, y);
}
/* 37F0:06CE (tick time: the water's animation 0588 and its start 0742, frame v at tile tp): object v + 3's rect (lowered
 * by the column's wave) at the tile's column and one row lower (17C1:016E), one pixel higher: the tiles under it */
void render_water_tick(int8_t tp, uint8_t v)
{
	int i = (uint8_t)(v + 3); int col = tp % 10;
	if (i >= desc_count()) return;
	int16_t y = wave_lower(i, col), src[4], r[4];
	obj_rect_get(desc_obj(i), src);
	if (render_rect_at_tile(tp / 10 + 1, col, src, r)) { r[0]--; mark_tiles_under(mark_back, r, 0xFF); }
	wave_restore(i, y);
}
/* 37F0:08D2 (0FB3:1C32, an object of type 0x8B: a bubble): the description object DS:1C90[obj_id & 0xF] + 1 as a
 * sprite one row up */
void obj_hook_37f0_08d2(uint8_t type)
{
	(void)type;
	add_sprite(4, (uint16_t)(ds_byte((uint16_t)(0x1C90 + (obj_id & 0xF))) + 1), obj_x, 0xA, (int16_t)(obj_y - 1));
}
/* 37F0:0782 (1375:200C, falling object type 0xB: a bubble): its position as the drawn room sees it (the room below,
 * above, left or right), its tile key (0AFF:026A; one less off column 0, +10 above row 2); in room 10 of level 5 only:
 * moved by the step's offsets DS:1C9A / 1CAE (step = +B & 0xF, the object DS:1C90[step]), the floor-depth table's type
 * 0xB entries DS:0826 / 0840 set to the object's image size + 1 (0CD6:0224), the tiles under it requested
 * (1375:2296(0)); a live one (speed not -1): the fore layer (2) of the tiles under its rect (16 in from the left) and
 * the object (1375:22DC); its position put back */
void draw_mob_37f0_0782(void)
{
	int16_t x = cur_mob.x, y = cur_mob.y; uint8_t room = cur_mob.room;
	if (room != drawn_room) {
		int moved = 1;
		if (room == room_B) y = (int16_t)(y + 0xBF);
		else if (room == room_A) y = (int16_t)(y - 0xBF);
		else if (room == room_L) x = (int16_t)(x - 0x140);
		else if (room == room_R) x = (int16_t)(x + 0x140);
		else moved = 0;
		if (moved) room = drawn_room;
	}
	int8_t col = (int8_t)(x < 0 ? -((-x) >> 5) : x >> 5);
	sv.key = (uint8_t)tile_index_of((int8_t)(y / 0x3F), col);   /* 0AFF:026A */
	if (sv.key != 0x1E) { if (col != 0) sv.key--; if ((int8_t)sv.key < 0x14) sv.key += 0xA; }
	int step = cur_mob.wd & 0xF; uint8_t img = ds_byte((uint16_t)(0x1C90 + step));
	if (level_number != 5 || room != 0xA) return;
	int16_t sx = cur_mob.x, sy = cur_mob.y, dy = (int16_t)ds_word((uint16_t)(0x1C9A + 2 * step));
	cur_mob.y = (int16_t)(cur_mob.y + dy); y = (int16_t)(y + dy);
	cur_mob.x = (int16_t)(cur_mob.x + (int16_t)ds_word((uint16_t)(0x1CAE + 2 * step)));
	if (render_desc_loaded() && img < desc_count()) {   /* 0CD6:0224 */
		const image_t *im = render_image(4, render_desc_image_id(img));
		if (im) { render_mob_box_set(0x0826, (int16_t)(im->height + 1)); render_mob_box_set(0x0840, (int16_t)(im->width + 1)); }
	}
	render_mob_mark(0);   /* 1375:2296 */
	if (cur_mob.speed != -1) {
		int16_t r[4]; render_mob_rect(r); r[1] = (int16_t)(r[1] + 0x10);
		mark_tiles_under(mark_fore_part, r, 0xFF);
		render_mob_obj(y);   /* 1375:22DC */
	}
	cur_mob.x = sx; cur_mob.y = sy;
}
