/* The floating heads (charids 7/8) of level kind 4, OVL09 at 366C. The record's bytes +0x11.. hold the state:
 * [0] 0 idle, 1 hunting, 2 retreating, 3 biting; [1]/[2] the column/row it heads for (state 3: bites so far);
 * [3..4] retreat timer; [5] the retreat sound was played. Sequences: 0x83..0x88 idle drifts, 0x89/0x8A up/down,
 * 0x8B home, 0x8C spots the prince, 0x8E/0x8F/0x90 fly across/up/down, 0x93 lunge, 0x96/0x97 bites, 0x98 recoil,
 * 0x99/0x9F/0xA0 knocked back, 0xA2/0xA3 back to hunting/home, 0xA4 smashed, 0xA5 gloats, 0xA6 hover, 0x9C/0x9D. */
#include <stddef.h>
#include "types.h"
#include "globals.h"

static uint8_t *head_state(void) { level_char_init *r = room_char_record(Char.index, Char.room); return r ? (uint8_t *)r + 0x11 : NULL; }
static int kid_reachable(void) { return Opp.f12 != 0 && !(Opp.f19 == 0x46 && (Opp.frame == 0 || Opp.frame >= 0xDF)); }   /* not dead, not leaving by the door */
static int frame_hovering(void) { uint16_t f = Char.frame; return f == 0x96 || f == 0xA5 || f == 0xA6 || f == 0x9E || f == 0xAD; }
static int iabs(int v) { return v < 0 ? -v : v; }

/* 366C:1136: flying up/down and not yet level with the row */
static int head_between_rows(void)
{
	int16_t y = 63 * Char.curr_row + 0x25;
	if (Char.f19 == 0x90) return Char.y > y;
	if (Char.f19 == 0x8F) return Char.y < y;
	return 0;
}
/* 366C:116C / 11A4: within tol of the middle of a column / row */
static int x_centred(int16_t x, int tol) { int r = (x - 0x90) % 32; return 0x10 - tol <= r && r <= tol + 0x10; }
static int y_centred(int16_t y, int tol) { int r = (y - 3) % 63; return 0x1F - tol <= r && r <= tol + 0x1F; }
static int gate_stops(uint8_t t) { return t == 4 && can_bump_into_gate(); }
/* 366C:06D4: may the head fly one tile that way (0 left, 1 right, 2 down, 3 up)? */
static int head_can_move(int dir)
{
	uint8_t t;
	switch (dir) {
	case 0:
		if (Char.curr_col < -1 || head_between_rows()) return 0;
		t = get_tile(Char.curr_row, Char.curr_col - 1, Char.room);
		return !tile_is_wall_kind(t) && !gate_stops(t);
	case 1:
		if (Char.curr_col > 10 || head_between_rows()) return 0;
		t = get_tile(Char.curr_row, Char.curr_col + 1, Char.room);
		if (tile_is_wall_kind(t) || gate_stops(t)) return 0;
		return !gate_stops(get_tile(Char.curr_row, Char.curr_col, Char.room));
	case 2:
		if (Char.curr_row == 2 || !x_centred(Char.x, 8)) return 0;
		return tile_is_empty_kind(get_tile(Char.curr_row, Char.curr_col, Char.room));
	case 3:
		if (Char.curr_row == 0 || !x_centred(Char.x, 8)) return 0;
		return tile_is_empty_kind(get_tile(Char.curr_row - 1, Char.curr_col, Char.room));
	}
	return 0;
}
/* 366C:00C4: a clear line from the head to the prince (half-tile steps: walls block across, floors block up/down)?
 * -1 blocked; else the way to go: 0 left, 1 right, 2 down, 3 up, 4 here (or not the prince's nearest foe) */
static int head_sight(void)
{
	if (Char.curr_col == Opp.curr_col && Char.curr_row == Opp.curr_row) return 4;
	int8_t y = Char.curr_row * 2, x = Char.curr_col * 2;
	if ((cur_frame.dx + Char.x - 0x7A) % 32 > 0x10) x++;
	int8_t ty = Opp.curr_row * 2 + 1, tx = Opp.curr_col * 2;
	if ((Opp.x - 0x82) % 32 > 0x10) tx++;
	int8_t dy = iabs(ty - y), sx = x < tx ? 1 : -1, sy = y < ty ? 1 : -1, dx = iabs(tx - x);
	int clear = 1;
	#define WALL_AT(r, c) tile_is_wall_kind(get_tile((r), (c), Char.room))
	#define CROSS_X() ((sx > 0 && x % 2 == 0) || (sx < 0 && x % 2 != 0))
	#define CROSS_Y() ((sy > 0 && y % 2 != 0) || (sy < 0 && y % 2 == 0))   /* leaving a row: down from its lower half, up from its upper */
	#define FLOOR_OPEN() tile_is_empty_kind(get_tile(sy < 0 ? (y - 1) / 2 : y / 2, x / 2, Char.room))
	if (dy < dx) {
		int8_t err = 2 * dy - dx, inc = 2 * dy, dec = (dy - dx) * 2;
		do {
			x += sx;
			if (CROSS_X()) clear = !WALL_AT(y / 2, x / 2);
			if (clear) {
				if (err < 0) err += inc;
				else { if (CROSS_Y()) clear = FLOOR_OPEN(); y += sy; err += dec; }
			}
		} while (--dx && clear);
	} else {
		int8_t err = 2 * dx - dy, inc = 2 * dx, dec = (dx - dy) * 2;
		do {
			if (CROSS_Y()) clear = FLOOR_OPEN();
			if (clear) {
				y += sy;
				if (err < 0) err += inc;
				else { x += sx; if (CROSS_X()) clear = !WALL_AT(y / 2, x / 2); err += dec; }
			}
		} while (--dy && clear);
	}
	if (!clear) return -1;
	if (!nearest_foe_2d3e_a26()) return 4;
	if (Opp.curr_row != Char.curr_row) return sy > 0 ? 2 : 3;
	return sx > 0 ? 1 : 0;
}
/* 366C:08C8 */
static void head_face_kid(void)
{
	if (Char.curr_col < Opp.curr_col) { if (Char.direction != 0) { Char.x = char_dx_forward(-14); Char.direction = 0; } }
	else if (Char.direction != -1) { Char.x = char_dx_forward(-14); Char.direction = -1; }
}
/* 366C:0B34: the head notices the prince */
static void head_spot(uint8_t *st)
{
	st[0] = 1; st[2] = Opp.curr_row; st[1] = Opp.curr_col; st[3] = 1;
	seqtbl_offset_char(0x8C); Char.fall_x = Char.fall_y = 0;
	head_face_kid();
	Char.f23 = 3; Char.f10 = 1;
	if (Opp.opp_index == 0xFF) { save_char(); hp_bar_draw(Char.index, Char.f12, Char.f13); load_char(Char.index); Opp.opp_index = Char.index; }
	play_sound(0x57);
}
/* 366C:0BA8 / 0800: back to idle where it is */
static void head_settle(uint8_t *st)
{
	st[0] = 0; Char.f10 = 0; Char.fall_x = Char.fall_y = 0;
	seqtbl_offset_char(0x8B);
	Char.curr_col = x_to_col(dx_weight()); Char.x = col_x_left[Char.curr_col] + 0x1E;
	char_y_to_floor(); Char.f23 = 0;
}
static void head_home(uint8_t *st) { head_settle(st); Char.f24 = 0; }
/* 366C:0D5C: idle drifting */
static void head_idle(void)
{
	if (Char.frame != 0x96) return;
	int si = random_2751(3);
	if (si == 3) si = (Char.f19 >= 0x83 && Char.f19 <= 0x85) ? 0 : (Char.f19 >= 0x86 && Char.f19 <= 0x88) ? 1 : -1;
	else if (si == 2) si += random_2751(1);
	if (si == -1 || !head_can_move(si)) return;
	seqtbl_offset_char(si == 3 ? 0x89 : si == 2 ? 0x8A : si == 0 ? random_2751(2) + 0x83 : random_2751(2) + 0x86);
}
/* 366C:0BF0: fly toward the remembered spot; settles when there or stuck */
static int head_follow(uint8_t *st)
{
	int ok = 0;
	if (st[1] == (uint8_t)Char.curr_col && st[2] == (uint8_t)Char.curr_row) { head_settle(st); return 0; }
	if (st[2] != (uint8_t)Char.curr_row) {
		int down = (int8_t)st[2] > Char.curr_row; uint16_t s = down ? 0x90 : 0x8F;
		ok = head_can_move(down ? 2 : 3);
		if (ok && Char.f19 != s) seqtbl_offset_char(s);
	}
	if (!ok && st[1] != (uint8_t)Char.curr_col) {
		ok = head_can_move((int8_t)st[1] > Char.curr_col ? 1 : 0);
		if (ok && Char.f19 != 0x8E) seqtbl_offset_char(0x8E);
	}
	if (!ok) head_settle(st);
	return ok;
}
/* 366C:0A60: lunging (seq 0x93) or flying up/down next to the prince */
static void head_lunge(uint8_t *st)
{
	if (Char.f19 != 0x93 && Char.f19 != 0x90 && Char.f19 != 0x8F) return;
	int near, far; if (Opp.f10 == 1) { near = -0x19; far = 0; } else { near = -3; far = 0x1E; }
	int d = opp_distance();
	if (Char.curr_row == Opp.curr_row && near <= d && d <= far) {
		Char.fall_x = Char.fall_y = 0; seqtbl_offset_char(random_2751(1) + 0x96); st[0] = 3; st[1] = 1; return;
	}
	int lim = Opp.f10 == 1 ? -0x23 : -0xD;
	if ((Char.curr_row != Opp.curr_row || lim > d) && Char.f19 == 0x93) { Char.fall_x = Char.fall_y = 0; seqtbl_offset_char(0x9C); }
	if (d - 0x40 <= far && Char.curr_row == Opp.curr_row && Char.f19 != 0x90) play_sound(0x5A);
}
/* 366C:04B4: hunting */
static void head_chase(uint8_t *st)
{
	uint16_t f = Char.f19;
	if (f == 0x8C || f == 0x98 || f == 0x9C || f == 0xA2) return;
	if (f >= 0x93 && f <= 0x97) { head_lunge(st); return; }
	if (!frame_hovering()) return;
	int si = head_sight();
	if (si == -1) { if (st[3]) { st[3] = 0; st[1] = Opp.curr_col; st[2] = Opp.curr_row; } head_follow(st); return; }
	st[3] = 1;
	int dx = iabs(Char.curr_col - Opp.curr_col), dy = iabs(Char.curr_row - Opp.curr_row);
	uint16_t s;
	if (dy == 0 && dx < 2) {
		if (dx == 0 || head_can_move(Char.x >= Opp.x ? 0 : 1)) { if (Char.f19 != 0x93) seqtbl_offset_char(0x93); return; }
		goto hover;
	}
	if (dx == 0 && dy == 1 && head_can_move(si)) { s = Char.curr_row > Opp.curr_row ? 0x8F : 0x90; if (Char.f19 != s) seqtbl_offset_char(s); return; }
	if (si == 4) { if (dx != 0 || dy != 0) goto hover; return; }
	st[2] = Opp.curr_row; st[1] = Opp.curr_col;
	if ((si == 3 || si == 2) && !head_can_move(si)) si = Char.x >= Opp.x ? 0 : 1;
	if (head_can_move(si)) {
		if (si == 3) s = 0x8F; else if (si == 2) s = 0x90; else { s = 0x8E; head_face_kid(); char_y_to_floor(); }
	} else { s = 0xA6; Char.fall_x = Char.fall_y = 0; }
	if (Char.f19 != s) seqtbl_offset_char(s);
	return;
hover:
	if (Char.f19 != 0xA6 && y_centred(Char.y, 0xF)) seqtbl_offset_char(0xA6);
}
/* 366C:03BE: does this bite land? (DS:1B84 / 1B90 by the record's skill byte +4: certain below, never from, else 50%) */
static int head_bite_lands(uint8_t *st)
{
	level_char_init *r = room_char_record(Char.index, Char.room); uint8_t k = r ? r->f04 : 0;
	if (ds_byte(0x1B84 + k) > st[1]) return 1;
	if (ds_byte(0x1B90 + k) <= st[1]) return 0;
	return random_2751(1);
}
/* 366C:0C86: biting */
static void head_bite(uint8_t *st)
{
	int back = 0;
	if ((Char.f19 == 0x96 && Char.frame == 0xA7 && Char.f24 != 1) || (Char.f19 == 0x97 && Char.frame == 0xA6)) {
		if (head_bite_lands(st)) {
			if (Opp.alive < 0) { st[1]++; seqtbl_offset_char((Char.f19 == 0x96) + 0x96); }
			else { level_char_init *r = room_char_record(Char.index, Char.room); if (r && r->f04 >= 0xB) seqtbl_offset_char(0xA5); }
		} else { seqtbl_offset_char(0x98); Char.x = char_dx_forward(-8); back = 1; }
	}
	if (back || Char.f19 == 0x9E || Char.f19 == 0x9D) { st[0] = 1; st[1] = Opp.curr_col; st[2] = Opp.curr_row; }
}
/* 366C:0904: knocked back, retreating */
static void head_retreat(uint8_t *st)
{
	level_char_init *r = room_char_record(Char.index, Char.room);
	if (Char.f19 == 0x99) { if (Char.f24 == 2) { Char.f23 = 0; seqtbl_offset_char(0x9F); } return; }
	if (Char.f19 == 0x9F) {
		if (st[1] == (uint8_t)Char.curr_col && x_centred(Char.x, 2)) {
			if (Char.curr_row == Opp.curr_row && iabs(Char.curr_col - Opp.curr_col) < 3) head_spot(st);
			else { seqtbl_offset_char(0xA0); uint16_t t = ds_word(0x1B9C + 2 * (r ? r->f04 : 0)); st[3] = t; st[4] = t >> 8; }
		}
	} else if (Char.curr_row == Opp.curr_row && iabs(Char.curr_col - Opp.curr_col) < 3 && kid_reachable()) head_spot(st);
	else {
		uint16_t t = (st[3] | st[4] << 8) - 1; st[3] = t; st[4] = t >> 8;
		if (t == 0) {
			if (kid_reachable() && head_sight() != -1) { head_spot(st); seqtbl_offset_char(0xA2); }
			else { head_settle(st); seqtbl_offset_char(0xA3); }
		}
	}
	if (st[5] == 0) { st[5] = 1; play_sound(0x92); }   /* (unless sound 0x2767 still plays) */
}
/* 366C:0E0A */
void heads_ai(void)
{
	if (Char.alive >= 0) return;
	uint8_t *st = head_state();
	if (!st) return;
	load_char(Char.index); Opp = Kid;   /* 0AFF:0878 */
	load_frame();
	if (cur_frame.image != 0xFFFF) { obj_id = cur_frame.image; set_char_collision(); }
	switch (st[0]) {
	case 0:
		if (kid_reachable() && frame_hovering() && head_sight() != -1) head_spot(st);
		else head_idle();
		break;
	case 1: if (kid_reachable()) head_chase(st); else head_home(st); break;
	case 2: head_retreat(st); break;
	case 3: if (kid_reachable()) head_bite(st); else head_home(st); break;
	}
	save_char_restore_kid_pub();   /* 0AFF:089A */
}
/* 366C:10A4: more than one character here and none retreating */
static int heads_none_retreating(uint8_t room)
{
	int8_t n = room_nchars(room);
	if (n <= 1) return 0;
	for (int8_t i = 0; i < n; i++) { level_char_init *r = room_char_record(i, room); if (((uint8_t *)r)[0x11] == 2) return 0; }
	return 1;
}
/* 366C:0F24: sometimes a hit head backs off to a free column behind it */
int head_knock_back(void)
{
	level_char_init *r = room_char_record(Char.index, Char.room); uint8_t k = r ? r->f04 : 0;
	if (!heads_none_retreating(Char.room) || random_2751(k / 3 + 1) != 0) return 0;
	if (!((Char.direction == 0 && Char.curr_col >= 3) || (Char.direction == -1 && Char.curr_col + 3 <= 10))) return 0;
	int ok = 1; int8_t c = Char.curr_col, end = Char.direction == 0 ? -1 : 10, behind = Char.direction == -1 ? 1 : -1, front = -behind;
	while (c != end && ok) {
		uint8_t t = get_tile(Char.curr_row, c, Char.room);
		if (wall_type(t) && !(t == 4 && !can_bump_into_gate())) ok = 0;
		c += behind;
	}
	if (!ok) { c += 2 * front; if (iabs(c - Char.curr_col) >= 3) ok = 1; }
	else c += front;
	if (!ok) return 0;
	Char.f10 = 0; Char.f23 = 1;
	uint8_t *st = (uint8_t *)r + 0x11;
	st[0] = 2; st[1] = c; st[2] = Char.curr_row; st[3] = st[4] = 0xFF; st[5] = 0;
	save_char(); hp_bar_draw(Char.index, 0, Char.f13); load_char(Char.index);
	if (Opp.opp_index == Char.index) Opp.opp_index = 0xFF;
	return 1;
}
/* 366C:06AC: the prince's sword meets a head: it dies (0x9A) or is knocked back (0x99) */
int head_hit(void) { if (take_hp(1)) return 0x9A; play_sound(0x57); head_knock_back(); return 0x99; }
/* 366C:1106: is character i of room biting? */
int head_biting(int i, uint8_t room) { level_char_init *r = room_char_record(i, room); return r && ((uint8_t *)r)[0x11] == 3; }
/* 366C:0816 (collision): a head flying into a wall within `near` is smashed */
int head_wall(int near)
{
	if (Char.f24 == 1) return -1;
	int si = wall_distance_pub(tile_col, curr_room, curr_tile);
	if (si >= 5 || near > si || -image_width > si) return -1;
	Char.x = char_dx_forward(-(si + 14));
	if (Char.alive < 0 && Char.f19 != 0x99) return 0x9D;
	if (Char.f19 == 0xA4) return -1;
	int di = -1;
	if (Char.frame <= 0xBF) { di = 0xA4; Char.x = col_x_left[tile_col_in_drawn_room()] + 0xE; if (Char.direction == -1) Char.x += 0x20; }
	else seqtbl_offset_char(0x91);
	take_hp(100);
	return di;
}
/* 2D3E:0A26: is Char the prince's nearest foe on its side? */
int nearest_foe_2d3e_a26(void) { return find_opponent(Char.x > Kid.x ? 0 : -1) == (int8_t)Char.index; }
static const uint8_t *hd_ds;
void heads_set_tables(const uint8_t *ds) { hd_ds = ds; }
uint8_t ds_byte(uint16_t a) { return hd_ds[a]; }
uint16_t ds_word(uint16_t a) { return hd_ds[a] | hd_ds[a + 1] << 8; }
