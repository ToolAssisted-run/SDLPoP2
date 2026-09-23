/* Sprite box and wall collisions: 0993:09B6 load_frame_to_obj, OVL01 segment 3212 (check_collisions and friends).
 * Reconstructed from the Ghidra decompilation of the DOS binary; names follow SDLPoP where the logic matches. */
#include "types.h"
#include "globals.h"
#include <stdio.h>
#include <stdlib.h>
int coll_debug;

int16_t obj_x, obj_y, obj_id; uint8_t obj_chtab;                             /* DS:60FC / 60FE / 6100 / 6102 */
int16_t image_height, image_width, char_x_left, char_x_right, char_x_left_coll, char_x_right_coll, char_top_y;   /* DS:6112.. */
int8_t char_col_left, char_col_right, char_top_row, char_bottom_row;         /* DS:6135..6138 */
coll_state coll;                                                             /* DS:2B24..2B67 */
uint8_t prev_coll_flags[10], curr_row_coll_flags[10];                        /* DS:6948 / 6952 */

static const int8_t wall_left_tbl[10]  = {0, 25, 0, -14, 2, 12, 6, 0, 10, 0};   /* DS:0D48, indexed by wall_type() */
static const int8_t wall_right_tbl[10] = {0, 0, 31, -14, 0, 0, 10, 0, 15, 0};   /* DS:0D52 */

/* 0AFF:1050 */
int8_t y_to_row(int16_t y) { int8_t r = (int8_t)((y - 3) / 63); if (y - 3 < 1) r--; return r; }

/* 1286:0568: image id of a sword frame (FRAM resource, 4-byte entries) */
static int16_t sword_image(uint16_t sword) { return sword ? (int16_t)(sword_table[sword * 4] | (sword_table[sword * 4 + 1] << 8)) : -1; }

/* 366C:150A (OVL10, charid 10): the sprite sits at the sword table's offset (FRAM 1000, bytes +2/+3); images 4, 0x13
 * and 0x14 shake while Char+0x3A counts (image 0x12 - count, stopping at -4) */
static void riser_offset(void)
{
	const uint8_t *e = sword_table + cur_frame.sword * 4;
	int8_t dx = (int8_t)e[2]; obj_x += Char.direction ? -dx : dx;   /* 0AFF:0390 */
	obj_y += (int8_t)e[3];
	if (obj_id != 4 && obj_id != 0x13 && obj_id != 0x14) return;
	if (Char.f3a != 0) obj_id = 0x12 - (int8_t)Char.f3a;
	if ((int8_t)Char.f3a <= -4) Char.f3a = 0; else Char.f3a--;
}
/* 0993:09B6 */
void load_frame_to_obj(void)
{
	load_frame();
	int16_t sw = sword_image(cur_frame.sword);
	if (Char.frame == 0) return;
	if (cur_frame.image == 0xFFFF && sw == -1) return;
	if (Char.charid == 0 || Char.charid == 6 || Char.charid == 1) obj_chtab = 2;
	else { obj_chtab = 3; obj_id = sw; if (Char.charid == 10) goto placed; }
	obj_id = (int16_t)cur_frame.image;
placed:
	if ((Char.charid == 7 || Char.charid == 8) && Char.f24 == 1) { ovl_366c2(); return; }
	obj_y = cur_frame.dy + Char.y;
	obj_x = char_dx_forward(cur_frame.dx) - 130;
	if (Char.charid != 10) {
		if (Char.charid == 4 && Char.frame > 0xCD && Char.frame < 0xD2) { obj_chtab = 4; obj_id += 0x68; }
		return;
	}
	riser_offset();
}

/* OVL01 032C36: extra width when the sword is drawn (FRAM entry byte +2 of the sword frame, minus 2) */
int16_t sword_extra_width(void)
{
	int16_t w = 0;
	if (Char.f10 == 1 && Char.charid != 7 && Char.charid != 8) {
		load_frame();
		if (cur_frame.sword != 0) { w = (int8_t)sword_table[cur_frame.sword * 4 + 2] - 2; if (w < 0) w = 0; }
	}
	return w;
}

/* OVL01 032ADE */
void set_char_collision(void)
{
	int16_t h, w1;
	Char.bbox_top = Char.bbox_left = Char.bbox_bottom = Char.bbox_right = 0;   /* DS:1F12 defaults */
	if (obj_id == -1 || !res_image_size(obj_chtab, obj_id, &h, &w1)) return;
	image_width = w1 + 1; image_height = h;
	int16_t extra = sword_extra_width();
	char_x_left = obj_x + 130 - (Char.direction == 0 ? image_width : extra);
	image_width += extra;
	char_x_right = char_x_left + image_width;
	char_top_y = obj_y - image_height + 1; if (char_top_y > 0xBF) char_top_y = 0;
	Char.bbox_bottom = obj_y + 1;
	if (Char.direction == -1) { Char.bbox_left = obj_x; Char.bbox_right = obj_x + w1; }
	else { Char.bbox_left = obj_x - w1; Char.bbox_right = obj_x; }
	Char.bbox_top = char_top_y;
	if (coll_debug) printf("   setcoll idx %u frame %u obj %d,%d id %d h %d w1 %d extra %d -> box %d %d %d %d\n", Char.index, Char.frame, obj_x, obj_y, obj_id, h, w1, extra, Char.bbox_top, Char.bbox_left, Char.bbox_bottom, Char.bbox_right);
	char_x_left_coll = char_x_left; char_x_right_coll = char_x_right;
	char_top_row = y_to_row(char_top_y);
	char_bottom_row = y_to_row(obj_y); if (char_bottom_row == -1) char_bottom_row = 3;
	char_col_left = col_from_x18(char_x_left_coll) < 0 ? 0 : col_from_x18(char_x_left_coll);
	char_col_right = col_from_x18(char_x_right_coll) < 10 ? col_from_x18(char_x_right_coll) : 9;
	if (frame_flags & 0x20) { char_x_left += 9; char_x_right -= 9; }
}

/* OVL01 032378: 0 = passable, else an index into the wall tables (1 gate, 2 wall/pillar-ish, 3 thin wall, 4 ..., 5 level-3 gate, 6, 8) */
int wall_type(uint8_t t)
{
	if (t == 0x19 || t == 0x14) return 4;
	if (t > 0x19) return 0;
	if (t == 7) return image_height < 0x13 ? 2 : 4;
	if (t < 8) {
		if (t == 2) return ((uint8_t)curr_modifier < 5 && byte_2ab4 != 0) ? 3 : 4;
		if (t != 4) return 0;
		if (level_kind == 3) return 5;
		return (drawn_room == 9 && level_number == 8) ? 8 : 1;
	}
	return t == 0xC ? 6 : 0;
}

/* OVL01 0326A2 (3212:0582): x in the drawn room's coordinate space for a tile of a neighbouring room (left / below-left, right / below-right) */
static int16_t x_in_drawn_room(int16_t x, uint8_t room)
{
	if (room != drawn_room) {
		if (room == room_L || room == room_BL) x -= 320;
		else if (room == room_R || room == room_BR) x += 320;
	}
	return x;
}

/* OVL01 032494 / 0324D0 */
static int16_t get_left_wall_xpos(int8_t row, int8_t col, uint8_t room)
{
	int w = wall_type(get_tile(row, col, room));
	return w ? wall_left_tbl[w] + coll.tile_left_xpos : 578;   /* DS:0D22 */
}
static int16_t get_right_wall_xpos(int8_t row, int8_t col, uint8_t room)
{
	int w = wall_type(get_tile(row, col, room));
	return w ? coll.tile_left_xpos - wall_right_tbl[w] + 31 : 0;
}

/* OVL01 0323FC */
static void get_row_collision_data(uint8_t *flags, uint8_t *rooms, int8_t row)
{
	coll.tile_left_xpos = col_x_left[coll.left_checked_col] + 14;
	for (int8_t c = coll.left_checked_col; c <= coll.right_checked_col; c++) {
		int16_t l = get_left_wall_xpos(row, c, Char.room), r = get_right_wall_xpos(row, c, Char.room);
		uint8_t f = l < char_x_right ? 0x0F : 0;
		if (char_x_left < r) f |= 0xF0;
		if (coll_debug) printf("   rowcoll row %d col %d -> room %u tcol %d tile %02X mod %04X wt %d tlx %d l %d r %d | xl %d xr %d f %02X\n", row, c, curr_room, tile_col, curr_tile, curr_modifier, wall_type(curr_tile), coll.tile_left_xpos, l, r, char_x_left, char_x_right, f);
		flags[tile_col] = f; rooms[tile_col] = curr_room;
		coll.tile_left_xpos += 32;
	}
}

/* OVL01 0322EE */
static void move_coll_to_prev(void)
{
	const uint8_t *rooms = coll.curr_room, *flags = curr_row_coll_flags;
	if (coll.prev_collision_row != coll.collision_row) {
		int d = coll.collision_row - coll.prev_collision_row;
		if (d != -3 && d != 3) {
			if (d == -1 || d == 2) { rooms = coll.above_room; flags = coll.above_flags; }
			else { rooms = coll.below_room; flags = coll.below_flags; }
		}
	}
	for (int i = 0; i < 10; i++) { coll.prev_room[i] = rooms[i]; prev_coll_flags[i] = flags[i]; }
	for (int i = 0; i < 10; i++) coll.curr_room[i] = coll.above_room[i] = coll.below_room[i] = 0xFF;
}

/* OVL01 0322A0 (feather fall against a wall while falling) */
static void feather_wall_check(void)
{
	int16_t floor = Char.curr_row * 63 - 4;
	if (Char.y - image_height < floor && !tile_is_empty_kind(get_tile(Char.curr_row - 1, Char.curr_col, Char.room))) {
		Char.x = char_dx_forward(-4); Char.fall_x = 0; seqtbl_offset_char(0xE5);
	}
}

/* OVL01 03212C */
void check_collisions(void)
{
	coll.bump_col_right_of_wall = coll.bump_col_left_of_wall = -1;
	if (Char.action != 7 && Char.f19 != 0x4E && Char.f19 != 0x46 && (word_440a == 0 || !ovl_34ce6() || Char.f24 != 4)) {
		coll.collision_row = Char.curr_row;
		move_coll_to_prev();
		coll.prev_collision_row = coll.collision_row;
		int8_t c = x_to_col(char_x_right);
		coll.right_checked_col = c + 4 < 12 ? c + 4 : 11;
		coll.left_checked_col = x_to_col(char_x_left) - 2;
		get_row_collision_data(curr_row_coll_flags, coll.curr_room, coll.collision_row);
		get_row_collision_data(coll.below_flags, coll.below_room, coll.collision_row + 1);
		get_row_collision_data(coll.above_flags, coll.above_room, coll.collision_row - 1);
		if (word_440a != 0) { ovl_34bd2(curr_row_coll_flags, coll.curr_room, coll.collision_row); ovl_34bd2(coll.below_flags, coll.below_room, coll.collision_row + 1); ovl_34bd2(coll.above_flags, coll.above_room, coll.collision_row - 1); }
		for (int8_t i = 9; i >= 0; i--) {
			if (coll.curr_room[i] != 0xFF && coll.prev_room[i] == coll.curr_room[i]) {
				if ((prev_coll_flags[i] & 0x0F) == 0 && (curr_row_coll_flags[i] & 0x0F) != 0) coll.bump_col_left_of_wall = i;
				if ((prev_coll_flags[i] & 0xF0) == 0 && (curr_row_coll_flags[i] & 0xF0) != 0) coll.bump_col_right_of_wall = i;
			}
		}
	}
	if (Char.charid == 0 && coll.bump_col_left_of_wall == -1 && coll.bump_col_right_of_wall == -1 && is_feather_fall != 0 && Char.action == 4) feather_wall_check();
}

/* OVL01 0329B6: may this character bump into the gate at curr_tile? (needs the gate to be low enough) */
int can_bump_into_gate(void)
{
	if (level_kind == 1) return ((uint8_t)curr_modifier & 0x1F) < 0x13;
	if (Char.charid == 1) return 0;
	int h = Char.bbox_bottom - Char.bbox_top;
	if (Char.charid == 7) h += 0x1A; else if (Char.charid == 8) h += 0x13;
	return (int)(((uint8_t)curr_modifier >> 2) + 6) < h + 7;
}
/* OVL01 032990 (tile 7) / 032AA2 (tile 0xC) */
static int blocks_tile7(void)  { return !(Char.charid != 7 && Char.charid != 8 && image_height < 0x13 && ((uint8_t)curr_modifier & 3) != 3); }
static int blocks_tile12(void) { return Char.charid != 1 && Char.charid != 7 && Char.charid != 8 && Char.charid != 0xB && image_height > 0x19 && ovl_343c2(); }

/* OVL01 032636: does curr_tile (already looked up) block the character? Sets coll.tile_left_xpos when it does. */
int tile_blocks(uint8_t t)
{
	int r;
	if (t == 0xC) r = blocks_tile12();
	else if (t == 4) r = can_bump_into_gate();
	else if (t == 7) r = blocks_tile7();
	else if (t == 10) r = 0;
	else r = 1;
	if (r && Char.charid == 0xB) r = 0;
	if (r) coll.tile_left_xpos = x_in_drawn_room(col_x_left[tile_col], curr_room) + 14;
	return r;
}
/* OVL01 0325F4 */
static int bump_tile_blocks(int8_t col)
{
	int8_t row = Char.curr_row; if (row < 0) row += 3; if (row > 2) row -= 3;
	return tile_blocks(get_tile(row, col, coll.curr_room[col]));
}

/* OVL01 0327A8: pushed back and knocked over */
static void bump_fall(void)
{
	Char.x = char_dx_forward(-8);
	if (Char.action == 4) Char.fall_x = 0;
	else { seqtbl_offset_char(0x2D); play_seq(); }
	if (Char.charid != 1) word_6140 = 1;   /* 03294E */
	load_fram_det_col();
}
/* OVL01 0327DA: landed against the wall */
static void bump_stand(int8_t dir)
{
	if (Char.f10 != 1 && (Char.curr_row * 63 - Char.y) + 56 >= 15) { bump_fall(); return; }
	char_y_to_floor();
	if (Char.fall_y >= 0x16) { Char.x = char_dx_forward(-10); load_fram_det_col(); return; }
	Char.fall_y = 0;
	if (Char.alive == 0) return;
	int16_t id;
	if (Char.f10 == 1) {
		int16_t lim = level_kind == 2 ? ovl_352ca() : 0;   /* 1375:14FC */
		if (Char.direction == dir) {
			if (Char.seq_id == 0x37 && Char.frame == 0xCF) Char.x = char_dx_forward(10);
			Char.x = char_dx_forward(2); id = 0x41;
		} else {
			id = 0x40;
			if (frame_is_strike_02f712(Char.frame, Char.charid)) {
				if (curr_tile == 4 && curr_modifier > 0x40) id = -1;
				else Char.x = char_dx_forward(16);
			}
		}
		if (lim != 0 && Char.x < lim) { id = 0x5D; Char.f10 = 0; }
	} else if (Char.frame < 0xF6 || Char.frame > 0x105) {
		if (Char.frame == 0x18 || Char.frame == 0x19 || (Char.frame > 0x27 && Char.frame < 0x2B) || (Char.frame > 0x65 && Char.frame < 0x6B)) id = 0x2E; else id = 0x2F;
	} else { Char.x = char_dx_forward(-3); id = 0x79; Char.f0f = 0; }
	if (id != -1) { seqtbl_offset_char(id); play_seq(); load_fram_det_col(); }
	if (id == 0x40) { if (Char.f19 != 0x38) play_sound(0xC); }
	else if (Char.charid != 1) word_6140 = 1;
}
/* OVL01 0326E2: apply a bump of dx against the wall on side dir (-1 = wall on the left) */
static void bump_apply(int8_t dir, int16_t dx)
{
	if (Char.alive >= 0 || Char.frame == 0xF2 || Char.frame == 0xF3) return;
	Char.x += dx;
	uint8_t t = curr_tile;
	if (tile_is_wall_kind(curr_tile)) {
		if (dir == -1) tile_col--;
		else { tile_col++; if (curr_room == 0 && tile_col == 10) { curr_room = Char.room; tile_col = 0; } }
		t = get_tile(tile_row, tile_col, curr_room);
	}
	if (tile_is_empty_kind(t) && (level_kind != 2 || !ovl_35240(8))) bump_fall();
	else bump_stand(dir);
}
/* OVL01 032422 / 03247A */
static void check_bumped_look_left(void)
{
	if (Char.f10 != 1 && Char.direction == 0) return;
	if (!bump_tile_blocks(coll.bump_col_right_of_wall)) return;
	int16_t x = get_right_wall_xpos(tile_row, tile_col, curr_room);
	if (x <= 0 && word_440a != 0) x = ovl_34b28(tile_row, curr_room, 0);
	bump_apply(0, x - char_x_left);
}
static void check_bumped_look_right(void)
{
	if (Char.f10 != 1 && Char.direction == -1) return;
	if (!bump_tile_blocks(coll.bump_col_left_of_wall)) return;
	int16_t x = get_left_wall_xpos(tile_row, tile_col, curr_room);
	if (x >= 578 && word_440a != 0) x = ovl_34b28(tile_row, curr_room, -1);
	bump_apply(-1, x - char_x_right);
}
/* OVL01 03250C */
void check_bumped(void)
{
	if (Char.action == 2 || Char.action == 6 || (Char.frame >= 0x87 && Char.frame <= 0x90)) return;
	if (coll.bump_col_left_of_wall >= 0) check_bumped_look_right();
	else if (coll.bump_col_right_of_wall >= 0) check_bumped_look_left();
}

/* OVL01 03317A: distance from the character's front edge to the wall of tile t at (col, room); -1 = no wall there */
static int16_t wall_distance(int8_t col, uint8_t room, uint8_t t)
{
	if (t == 4 && !can_bump_into_gate()) return -1;
	coll.tile_left_xpos = x_in_drawn_room(col_x_left[col] + 14, room);
	int w = wall_type(t);
	if (w == 0) return -1;
	if (Char.direction != 0) return wall_right_tbl[w] - coll.tile_left_xpos + char_x_left - 31;
	return wall_left_tbl[w] - char_x_right + coll.tile_left_xpos;
}
/* OVL01 032C84: distance to the edge in front (edge_type 0 = drop, 1 = wall, 2 = none within a tile) */
int get_edge_distance(void)
{
	determine_col(); load_frame_to_obj(); set_char_collision();
	uint8_t t = get_tile_at_char(); int16_t d;
	if (wall_type(t) != 0 && (d = wall_distance(Char.curr_col, Char.room, t)) >= 0) goto wall;
	if (t != 0x18) {
		t = get_tile_infrontof(1);
		if (wall_type(t) != 0 && (d = wall_distance(dir_front[Char.direction + 1] + Char.curr_col, Char.room, t)) >= 0) goto wall;
		if ((tile_is_empty_kind(t) || tile_is_loose_kind(t) || t == 0x1E) && t != 0x18) { edge_type = 0; return distance_to_edge_weight(); }
		if (t == 6 || t == 0x16 || t == 2 || t == 0x22) {
			d = distance_to_edge_weight();
			if (d == 0) { edge_type = 2; return 0x1C; }
			edge_type = 0; return d - 1;
		}
		if (t != 0x18) { edge_type = 2; return 0x1C; }
	}
	if (Char.direction == -1) { d = distance_to_edge_weight(); return Char.curr_col != tile_col ? d + 0x11 : d - 0xF; }
	edge_type = 2; return 0x1C;
wall:
	if (d < 0x20) { edge_type = 1; return d; }
	edge_type = 2; return 0x1C;
}

/* OVL01 03305A / 0330CE: signed distances to the gate at (room, col) on the near and far side */
static int16_t gate_near_distance(uint8_t t, uint8_t room, int8_t col)
{
	if (t == 4 && !can_bump_into_gate()) return -1;
	coll.tile_left_xpos = x_in_drawn_room(col_x_left[col] + 14, room);
	int w = wall_type(t); if (w == 0) return -1;
	if (Char.direction == 0) return wall_left_tbl[w] - char_x_right + coll.tile_left_xpos;
	return wall_right_tbl[w] - coll.tile_left_xpos + char_x_left - 31;
}
static int16_t gate_far_distance(uint8_t t, uint8_t room, int8_t col)
{
	coll.tile_left_xpos = x_in_drawn_room(col_x_left[col] + 14, room);
	int w = wall_type(t); if (w == 0) return 0x63;
	if (Char.direction < 0) return wall_left_tbl[w] - char_x_right + coll.tile_left_xpos;
	return wall_right_tbl[w] - coll.tile_left_xpos + char_x_left - 31;
}
/* OVL01 032F20: push the character out of a closing gate */
static void gate_push_out(void)
{
	int16_t a = gate_near_distance(4, curr_room, tile_col); if (a < 0) a = -a;
	int16_t b = gate_far_distance(4, curr_room, tile_col); if (b < 0) b = -b;
	Char.x = char_dx_forward(b > a ? -a : b);
}
/* OVL01 032E78 */
void check_gate_push(void)
{
	uint8_t t = get_tile_at_char();
	if (Char.alive >= 0 || Char.f19 == 0x76) return;
	if (t != 4 && get_tile_behind_char() != 4 && get_tile_infrontof(1) != 4) return;
	if (curr_room == 9 && level_number == 8) return;
	if ((curr_row_coll_flags[tile_col] & prev_coll_flags[tile_col]) != 0xFF || !can_bump_into_gate()) return;
	if (Char.frame == 0x6D || curr_modifier == 0 || Char.f10 == 1) {
		if (level_kind == 3 && curr_modifier != 0 && Char.f10 != 1) { ovl_3211a(); return; }
		if (Char.charid != 1) word_6140 = 1;
		gate_push_out();
	} else if (Char.f19 != 0x32) seqtbl_offset_char(0x32);
}

/* OVL01 3212:0E8E (032FAE): a guard with the sword drawn walking into a wall or a gate */
void check_guard_bumped(void)
{
	int is78 = Char.charid == 7 || Char.charid == 8; uint8_t act = is78 ? 0 : 1;
	if (!((Char.action == act || (is78 && Char.f19 != 0xA4)) && (Char.alive < 0 || is78) && Char.f10 == 1 && Char.f19 != 0x46)) return;
	uint8_t t = get_tile_at_char();
	if (!tile_is_wall_kind(t) && (t != 4 || !can_bump_into_gate())) {
		t = get_tile_infrontof(1);
		if (!tile_is_wall_kind(t) && (t != 4 || !can_bump_into_gate())) t = 0;
	}
	if (t == 0) return;
	load_frame_to_obj(); set_char_collision();
	if (!tile_blocks(t)) return;
	int id = -1;
	int16_t far = gate_far_distance(curr_tile, curr_room, tile_col), near = wall_distance(tile_col, curr_room, curr_tile);
	int16_t m = abs(far) <= abs(near) ? far : near;
	if (is78) id = ovl_36ed6(near);
	if (id == -1 && m < 5 && m >= -image_width) {
		if (t == 4 && m > 0) m = -m;
		Char.x = char_dx_forward(-m);
		if (m < 0 || t != 4 || is78) { if (!is78 || (Char.f19 != 0x91 && Char.f19 != 0xA4 && Char.f24 != 1)) id = is78 ? 0x9E : 0x41; }
		else id = 0x40;
	}
	if (id != -1) { seqtbl_offset_char(id); play_seq(); load_fram_det_col(); }
}
/* OVL01 3212:0C88 (032DA8): a guard overlapping a closing gate is pushed out */
void check_gate_guard(void)
{
	uint8_t t = get_tile_at_char();
	if (Char.alive >= 0) return;
	if (t != 4 && get_tile_behind_char() != 4 && get_tile_infrontof(1) != 4) return;
	if ((curr_room == 9 && level_number == 8) || Char.charid == 0xC || !can_bump_into_gate()) return;
	coll.tile_left_xpos = col_x_left[tile_col] + 14;
	int16_t l = get_left_wall_xpos(tile_row, tile_col, curr_room), r = get_right_wall_xpos(tile_row, tile_col, curr_room);
	if (curr_room != drawn_room) {
		if (curr_room == room_L) { l -= 320; r -= 320; } else if (curr_room == room_R) { l += 320; r += 320; } else { l = 999; r = 0; }
	}
	if (l < char_x_right && char_x_left < r) gate_push_out();
}
int16_t wall_distance_pub(int8_t col, uint8_t room, uint8_t t) { return wall_distance(col, room, t); }
