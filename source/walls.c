/* Sliding walls (temple levels; OVL07 at 347C): a button on tile 0x19 starts a wall moving along its row as a
 * falling-object entry of type 6 (x = its left edge, wd = where it started, w7 = its state: -1 pulled back, 1..8
 * pushing out, 0 stopped, 0x64 crushed shut; speed 1 = against a wall). The wall pushes, stops or crushes the prince,
 * blocks him through the collision rows, and the gate and trap code ask where it is. Transcribed from the
 * disassembly. */
#include <stddef.h>
#include "types.h"
#include "globals.h"

static int8_t div32(int16_t v) { return (int8_t)(v < 0 ? -((-v) >> 5) : v >> 5); }   /* cwd; xor; sub; sar 5 */
static int16_t ds_tab(uint16_t a) { return (int16_t)ds_word(a); }

/* 347C:0526: the wall on a room's row */
mob_type *wall_find(uint8_t room, int8_t row)
{
	for (int n = 1; ; n++) {
		mob_type *m = find_mob_pub(n, 6);
		if (!m) return NULL;
		if (m->room == room && (int8_t)m->row == row) return m;
	}
}
/* 347C:0B3E (a button acting on tile 0x19; k 1 = push out, 2 = pull back) */
void wall_trigger(uint8_t room, int8_t tp, int k)
{
	int8_t row = tp / 10;
	mob_type *m = wall_find(room, row);
	if (m) {
		if (k == 1) { if (m->w7 >= 0 && m->w7 != 0x64) m->w7 = -1; }
		else if (k == 2 && m->w7 == -1) m->w7 = 1;
		return;
	}
	if (k != 1) return;
	get_tile(row, tp % 10, room);
	cur_mob.w7 = -1; cur_mob.speed = 0;
	int16_t x = ds_tab(0x0D26 + 2 * tile_col);   /* 32 * column */
	if (x > 0x140 && room_L == room) { room = drawn_room; x -= 0x140; }
	cur_mob.x = x; cur_mob.wd = x; cur_mob.room = room;
	cur_mob.y = ds_tab(0x0D40 + 2 * tile_row) - 5;
	cur_mob.row = row; cur_mob.type = 6;
	add_mob_pub();   /* 1375:19A2 */
}
/* 347C:0732: the tiles the wall's front covers (dist pixels ahead, drow rows off) are all solid */
static int wall_front_solid(int16_t dist, int8_t drow)
{
	int si = 1;
	int8_t c0 = div32(cur_mob.x + 6);
	uint16_t room_left = (uint16_t)(cur_mob.wd - cur_mob.x);
	int16_t d = room_left < (uint16_t)dist ? (int16_t)room_left : dist;
	int8_t c1 = div32(d + cur_mob.x), row = (int8_t)(cur_mob.row + drow);
	if (c1 < c0) return si;
	do {
		uint8_t t = get_tile(row, c0++, cur_mob.room);
		si = !tile_is_empty_kind(t) && t != 0x19;
	} while (si && c1 >= c0);
	return si;
}
/* 347C:0214: blocked above and at the wall's row */
static int wall_blocked(void) { return wall_front_solid(0x20, -1) ? wall_front_solid(0x20, 0) : 0; }
/* 347C:056C: the wall reaches the prince */
static void wall_hits_kid(void)
{
	if (Char.action == 5) return;
	int si = -1;
	if (Char.x - 0x90 > cur_mob.x) { Char.x = cur_mob.x + 0x8C; if (Char.direction == -1) Char.x -= 0xD; }
	int16_t cx = Char.x - 8; if (Char.direction == 0) cx -= 0xD;
	int8_t col = x_to_col(cx);
	uint8_t t = get_tile(Char.curr_row, col, Char.room);
	if (!tile_is_wall_kind(t) && t != 4) {   /* room behind him: pushed along */
		if (Char.alive >= 0) Char.x -= 8;
		else if (Char.f10 == 1) { si = 0x5D; Char.f10 = 0; }
		else si = Char.direction == 0 ? 0x2F : 0x30;
	} else {   /* against a wall: stopped, or crushed */
		Char.x = col_x_right[col] + 8; if (Char.direction == 0) Char.x += 0x11;
		if (t == 2) goto done;
		if (cur_mob.w7 == 0x64) { if (Char.alive < 0) { take_hp(Char.f12); Char.alive = 0; } Char.frame = 0xB9; goto done; }
		int16_t x = cur_mob.x - 0xA; int8_t c = div32(x);
		t = get_tile(Char.curr_row, c, Char.room);
		if (!tile_is_wall_kind(t) && t != 4) {
			if (Char.alive < 0) {
				x -= 5; int8_t c2 = div32(x);
				if (c2 != c) { t = get_tile(Char.curr_row, c2, Char.room); if (tile_is_wall_kind(t) || t == 4) play_sound(1); }
			}
		} else {
			if (Char.alive < 0) play_sound(0x37);   /* (after stopping sound 0x2711) */
			seq_set_85f8(0xD);
		}
		Char.f24 = 4; si = 0x47;
	}
done:
	if (si != -1) { char_y_to_floor(); Char.fall_y = 0; seqtbl_offset_char(si); play_seq(); }
}
/* 347C:027A (a type-6 entry, after 07D4): the wall's front touches the prince */
void wall_push_kid(void)
{
	loadkid();
	int16_t si = Char.direction == 0 ? Char.x : Char.x + 0xD;
	if (cur_mob.room == Char.room && cur_mob.row == (uint8_t)Char.curr_row) {
		int16_t dx = cur_mob.x + 0x8C;
		if (Char.f24 == 4 || (dx < si && dx + 0x10 > si)) { wall_hits_kid(); Kid = Char; }
	}
}
/* 347C:07D4 (a type-6 entry): the wall moves */
void wall_move(void)
{
	if (cur_mob.w7 == 0 || cur_mob.w7 == 0x64) return;
	int16_t si = cur_mob.x;
	if (cur_mob.w7 == -1) si--;
	else {
		if (wall_blocked() && cur_mob.w7 >= 8) { cur_mob.w7 = 0; if (word_087e == 0x36) word_087e = -1; }   /* (stops sound 0x2746) */
		else {
			cur_mob.x -= 8; if (wall_blocked()) play_sound(0x35);
			cur_mob.x = si; si--; if (cur_mob.w7 <= 8) cur_mob.w7++;
		}
	}
	if (si < 0) { cur_mob.room = level_links(cur_mob.room)[0]; si += 0x140; }
	int16_t newx = si;
	if (cur_mob.room == 0) { cur_mob.speed = -1; return; }
	if (cur_mob.w7 == 0 || cur_mob.w7 == 0x64) return;
	int16_t bx = si - 0x14; int8_t col = div32(bx); if (bx <= 0) col--;
	uint8_t t = get_tile((int8_t)cur_mob.row, col, cur_mob.room);
	if (cur_mob.speed == 1) {   /* against a wall last time: shut */
		cur_mob.speed = 0; cur_mob.w7 = 0x64; cur_mob.y++; cur_mob.x = newx + 2;
		if (t == 2) { loadkid(); trap_kill_pub(); Kid = Char; }
		return;   /* (stops sound 0x2746) */
	}
	int8_t c1 = div32(newx - 4), c0 = div32(cur_mob.x - 4);
	if (!(c0 == c1 && col == c0)) {
		uint8_t t2 = get_tile((int8_t)cur_mob.row, c1, cur_mob.room);
		if ((tile_is_wall_kind(t2) || t2 == 4 || t == 2) && cur_mob.speed == 0) {
			cur_mob.speed = 1; cur_mob.y--;
			if (!sound_playing(0x2747) && t != 2) play_sound(8);
		}
	}
	loadkid();
	if (Char.frame >= 0x57 && Char.frame <= 0x63) {   /* hanging below the wall's row */
		int8_t c8 = div32(newx - 8);
		if ((int8_t)cur_mob.row - Char.curr_row + 1 == 0 && Char.curr_col == c8) control_frame81_0313c6();
	}
	Kid = Char;
	if (cur_mob.w7 == 0x64) return;   /* (stops sound 0x2746) */
	cur_mob.x = newx;
	if (cur_mob.w7 != 0) play_sound(0x36);
}
/* 347C:0368: the x the wall leaves free on a room's row, from the right (dir 0) or for a left bump (dir -1) */
int16_t wall_edge(int8_t dir, uint8_t room, int8_t row)
{
	mob_type *m = wall_find(room, row);
	int16_t si;
	if (m) {
		si = m->x + 0x8C;
		if (dir == -1) {
			si -= 3;
			int8_t col = x_to_col(si - 0x20);
			uint8_t t = ROOM_TILES(m->room)[(int8_t)(row_tilepos((int8_t)m->row) + col)];
			if (tile_is_wall_kind(t) || t == 4) {
				int16_t dx = col_x_right[col] + 0x18; if (Char.direction == 0) dx += char_x_right - char_x_left;
				if (dx < si) dx = si;
				si = dx;
			}
		} else si += 0x23;
	} else si = dir == -1 ? ds_tab(0x0D22) : 0;
	return si;
}
/* 347C:0412 (check_collisions): the wall's faces in a row's collision flags */
void wall_collision(int8_t row, uint8_t *rooms, uint8_t *flags)
{
	mob_type *m = wall_find(Char.room, row);
	if (!m) return;
	int16_t di = m->x + 0x89;
	if (di < char_x_right) {
		int8_t c = x_to_col(di);
		if (c >= 0 && c < 10) { flags[c] |= 0x0F; rooms[c] = m->room; }
		if (m->w7 != 0x64 && Char.curr_row == row) {
			int8_t c2 = x_to_col(di + 1);
			if (c2 != c) { prev_coll_flags[c] |= prev_coll_flags[c2] & 0x0F; if (!(flags[c2] & 0x0F)) prev_coll_flags[c2] &= 0xF0; }
		}
	}
	int16_t r = di + 0x26;
	if (r > char_x_left) {
		int8_t c = x_to_col(r);
		if (c >= 0 && c < 10) { flags[c] |= 0xF0; rooms[c] = m->room; }
		if (m->w7 != 0x64 && Char.curr_row == row) {
			int8_t c2 = x_to_col(r + 1);
			if (c2 != c) { prev_coll_flags[c] |= prev_coll_flags[c2] & 0xF0; if (!(flags[c2] & 0xF0)) prev_coll_flags[c2] &= 0x0F; }
		}
	}
}
/* 347C:0A80: the wall stands within the current tile (the last one looked up), dist pixels in */
int wall_near(int16_t dist)
{
	int16_t x = wall_edge(0, curr_room, tile_row);
	if (!x) return 0;
	int16_t left = tile_col * 32, right = left + 0x20;
	x -= 0x8C;
	if (left - dist + 0x20 <= x && right - dist + 0x20 >= x) return 1;
	x = wall_edge(-1, curr_room, tile_row) - 0x8C;
	return left - dist <= x && right - dist >= x;
}
/* 347C:0A0E (the trap code): the wall is at one of the two tiles after the current one */
int wall_near_blade(void)
{
	uint8_t tile = curr_tile; uint16_t mod = curr_modifier; uint8_t tp = curr_tilepos, room = curr_room; int8_t col = tile_col, row = tile_row;
	int8_t cl = char_col_left, cr = char_col_right, tr = char_top_row, br = char_bottom_row;
	get_tile(row, col + 1, room); int r = wall_near(0x10);
	if (!r) { get_tile(row, col + 2, room); r = wall_near(0x10); }
	curr_tile = tile; curr_modifier = mod; curr_tilepos = tp; curr_room = room; tile_col = col; tile_row = row;
	char_col_left = cl; char_col_right = cr; char_top_row = tr; char_bottom_row = br;
	return r;
}
/* 347C:0B0A (1375:14FC with the prince's room and row): how far he may go */
int16_t wall_limit(uint8_t room, int8_t row)
{
	int16_t x = wall_edge(0, room, row);
	if (x && Char.x < x) x = wall_edge(-1, room, row);
	return x;
}
