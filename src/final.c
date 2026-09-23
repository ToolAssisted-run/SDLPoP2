/* Level kind 6 (level 14), OVL08 at 33FD: the kind tick (DS:0668 -> 33FD:03C6). Entering room 2 as the prince (not
 * the spirit) sends him back to room 4 after scene 5 and a reload of the level; the spirit standing at the left of
 * room 5 while his body lies in room 7 or 8 dies; guards appear in room 3; room sounds. Palettes, scenes and sounds
 * themselves are left out. Transcribed from the disassembly. */
#include "types.h"
#include "globals.h"

extern int last_scene; int load_level(int n);
#define word_2bb4 (*(uint16_t *)(tiles0 + 0x1A))   /* DS:2BB4: room 8 was reached with the sounds on */

/* platform: DS:2B98 sound on (the sound query on DS:0884, the music playing, goes through sound_playing(0xFFFF)) */
__attribute__((weak)) int sound_on(void) { return 1; }

/* 0823:0F38 (hp bars): Kid = Char, then the prince's opponent is loaded */
static void hp_bars_reload(void)
{
	loadkid(); Kid = Char;
	if (Kid.opp_index != 0xFF) load_char(Kid.opp_index);
}
/* 33FD:035A: the prince stands in room 4, column 5, row 1, facing right */
static void place_at_room4(void)
{
	Char.pal_slot = 2; Char.direction = 0;
	seqtbl_offset_char(0x37); Char.f10 = 1; play_sound(0x13);
	Char.room = next_room = 4; Char.curr_col = 5;
	Char.x = col_x_left[5] + 0xE; Char.curr_row = 1;
	char_y_to_floor(); Char.hp_delta = Char.f12;
	play_seq(); Kid = Char;
	control_rest(); ctrl1_shift = 0;
}
/* 33FD:045C: scene 5, the level again (1286:01F2), and the prince back in room 4 */
static void back_to_room4(void)
{
	/* 0AAC:0274 scene 5; 169B:018E: the four OVL01 initialisers clear DS:2B96 */
	word_2b96 = 0;
	last_scene = 5; load_level((int8_t)word_32d8);   /* (the full load, 1286:01F2) */
	/* 0FB3:2B1C palette; 1286:07CE music */
	place_at_room4();
}
/* 33FD:0570: the prince in room 2; the spirit near its body */
static void kind6_kid(void)
{
	loadkid();
	if (Char.room == 2) {
		if (char_x_right <= 0x131 && Char.action != 4) back_to_room4();
	} else if (Char.charid == 1 && Char.f19 != 0x47) {
		load_char(body_index());
		uint8_t r = room_of_char();
		if ((r == 7 || r == 8) && level_kind == 6 && Kid.room == 5 && Kid.curr_col <= 4 && (frame_table_kid[Kid.frame * 7 + 6] & 0x40)) {
			loadkid(); take_hp(100); seqtbl_offset_char(0x47); Kid = Char;
			apply_hp_deltas(); hp_bars_reload();
		}
		loadkid();
	}
	Kid = Char;
}
/* 33FD:01BA: a guard appears in room 3 at (row, col) */
static void guard_appears(int8_t row, int8_t col)
{
	int8_t idx = (int8_t)ROOM_REC(3)->nchars++;
	level_char_init *rec = room_char_record(idx, 3);
	rec->tilepos = (int8_t)(row_tilepos(row) + col);
	rec->x = col_x_left[col];
	rec->direction = col < 5 ? 0 : -1;
	rec->f04 = 5;
	if (pal_slots[0] == 0) { pick_pal_slot_pub(1); /* 2D3E:0F50 palette */ }
	rec->pal = pal_slots[0]; rec->index = (uint8_t)idx; rec->f10 = 0; rec->f38 = 0;
	((uint8_t *)rec)[0x15] = (uint8_t)row;   /* (low byte of w15) */
	rec->hp = rec->max_hp = (uint8_t)(random_2751(2) + 3);
	Char.index = (uint8_t)idx; Char.direction = rec->direction; Char.x = rec->x;
	Char.charid = type_to_charid[level.type];
	Char.curr_col = col; Char.curr_row = row; Char.f38 = 0; Char.room = 3;
	Char.f13 = Char.f12 = rec->hp; Char.hp_delta = (int8_t)rec->hp;
	Char.alive = -1; Char.pal_slot = 4;
	char_y_to_floor(); Char.fall_x = Char.fall_y = 0;   /* 0AFF:07B0, 0AFF:0952 */
	Char.f24 = 0; Char.f0f = 1; Char.f10 = 0;
	seqtbl_offset_char(0xEE); Char.f23 = 0; play_seq();
	play_sound(0xC5);
	if (Kid.opp_index == 0xFF) Kid.opp_index = Char.index;
	save_char();
}
/* 33FD:0126: with no guard (or one that has finished appearing) in room 3, one appears on the other row */
static void room3_guards(int8_t n)
{
	int8_t row = -1;
	if (n > 1) return;
	if (n == 1) {
		if (chars[0].f19 == 0xEE) return;
		row = chars[0].curr_row == 1 ? 2 : 1;
		if (row == Kid.curr_row) return;
	} else row = Kid.curr_row == 2 ? 1 : 2;
	if (n != 0 && random_2751(0x14)) return;
	int8_t col = row == 1 ? 4 : 2;
	if (random_2751(1)) col += 4;
	guard_appears(row, col);
}
/* 33FD:0640 */
static void kind6_chars(void)
{
	int8_t n = room_nchars(drawn_room);
	for (int8_t i = 0; i < n; i++) { load_char(i); save_char(); }
	if (drawn_room == 3 && level_kind == 6) room3_guards(n);
}
/* 33FD:03C6 (DS:0668) */
void kind6_tick(void)
{
	kind6_kid(); kind6_chars();
	if (!sound_on() || sound_playing(0xFFFF) || Kid.alive >= 0 || (int8_t)word_32d8 != (int16_t)counter_5cec) return;
	int si = -1;
	switch (drawn_room) {
	case 3: si = 0x10D; break;
	case 6: si = ROOM_REC(6)->nchars < 1 ? 0x10C : 0x10E; break;
	case 8: word_2bb4 = 1; /* fall through */
	case 7: si = word_2bb4 < 1 ? 0x10C : 0x107; break;
	}
	if (si != -1 && !sound_playing(si + 0x2710)) sound_1611_01a8(si);
}

/* Tile animations (1375:0096 calls 33FD directly: the loaded overlay's routine) and their starts on room entry
 * (0823:0B78). Each runs only while the room description (DS:01AC, byte 1 = background id) is the expected one;
 * the rest redraws parts of the description (1375:0F5A), left out. DS:610E (a full redraw in progress) is 0 here. */
uint8_t byte_2b74[2];   /* DS:2B74 / 2B75: the next start index of tiles 0x29 (room 6) and 0x2A (rooms 7, 8) */
static uint16_t mod16(void) { return (uint16_t)anim_mod; }
static void set_mod16(uint16_t v) { anim_mod = (anim_mod & 0xFFFF0000u) | v; }
/* 33FD:15A2: tile 0x1F counts up (background 0x1A: the drawing at 5..10, then stops at 11) */
void anim_tile1f(void)
{
	set_mod16(mod16() + 1);
	uint16_t dl = mod16() & 0xF;
	if (dl < 5 || room_background_id() != 0x1A) return;
	if (dl - 5 >= 6) { cur_trob.state = 0xFF; set_mod16(0); }
}
/* 33FD:162A: tile 0x28 (background 0x19): bits 3..10 a delay, bit 11 showing, bits 0..2 the frame */
void anim_tile28(void)
{
	if (room_background_id() != 0x19) { cur_trob.state = 0xFF; return; }
	uint16_t di = mod16(); int16_t si = (di & 0x7F8) >> 3;
	if (di & 0x800) {
		di = (uint16_t)random_2751(5);
		if (--si > 0) di |= 0x800;
		else si = random_2751(0x3C) + 0x1E;
	} else if (--si <= 0) {
		if (word_2ba4 == 0) { si = random_2751(2) + 1; di |= 0x800; }
		else si = random_2751(0x3C) + 0x1E;
	}
	set_mod16((uint16_t)((di & 0xF807) | (si << 3)));
}
/* 33FD:17AC / 19B4: tiles 0x29 and 0x2A: bits 4..10 a delay or step, bit 11 running, bit 12 kept set */
static void anim_steps(int last, int base)
{
	uint16_t si = mod16();
	if (si & 0x800) {
		uint16_t di = ((si & 0x7F0) >> 4) + 1;
		if (di < last) si = (si & 0xF80F) | (di << 4);
		else { si &= 0xF80F; si |= (uint16_t)((random_2751(0x28) + base) << 4); si &= 0xF7FF; }
	} else {
		int16_t dx = ((si & 0x7F0) >> 4) - 1; si &= 0xF80F;
		if (dx > 0) si |= dx << 4;
		else if (word_2ba4 != 0 && trob_count != 0) si |= (uint16_t)((random_2751(0x28) + base) << 4);
		else si |= 0x800;
	}
	set_mod16(si | 0x1000);
}
void anim_tile29(void) { if (room_background_id() != 0x1C) { cur_trob.state = 0xFF; return; } anim_steps(7, 0x28); }
void anim_tile2a(void)
{
	int bg = room_background_id(); uint8_t idx = mod16() & 0xF;
	if ((bg != 0x1D && bg != 0x1E) || !((idx < 6 && drawn_room == 7) || (idx >= 6 && drawn_room == 8))) { cur_trob.state = 0xFF; return; }
	anim_steps(9, 0x50);
}
/* 33FD:1962 / 1B94 (0823:0B78 on level 14, a tile with attribute bit 0x1000 in room 6 / rooms 7, 8) */
void anim_start_final(uint32_t *attrs, int8_t tp, uint8_t room)
{
	int k = room == 6 ? 0 : 1; uint16_t di = byte_2b74[k]++;
	uint16_t si = (uint16_t)random_2751(0x28);
	*(uint16_t *)&attrs[tp] = (uint16_t)(((si | 0x100) << 4) | di);
	add_trob(k ? 0x2A : 0x29, 1, tp, room);
}
/* 33FD:1708 (room hooks 0x1C..0x1E, level 14 rooms 6..8): palettes; the start indexes; sounds */
void final_room_enter(void)
{
	byte_2b74[0] = 0;
	if (drawn_room == 7) byte_2b74[1] = 0;
	else if (drawn_room == 8) byte_2b74[1] = 6;
}

/* The fight at the top (rooms 6..8). Charid 6 is Jaffar (type 3): in room 6 four of him stand still until the prince
 * comes close; in rooms 7/8 one walks a loop of four waypoints (DS:1A2E) and, when the prince's body is on his row
 * within reach, turns and casts (seq 0xF2: at frame 0x11A the prince dies). The spirit casts fireballs (falling
 * object type 0xC, from frame 0x119) that kill Jaffar. */
typedef struct __attribute__((packed)) waypoint { int8_t col, row, dir; int16_t x_min, x_max; int8_t row_by_dir[2], col_min[2], col_max[2]; } waypoint;
_Static_assert(sizeof(waypoint) == 13, "waypoint");
static const waypoint *wp(int k) { return (const waypoint *)ds_ptr(0x1A2E + 13 * k); }
/* a record's Jaffar state (rec+0x11): mode (1 patrol, 2 chase), the waypoint aimed at, the waypoint reached, y */
static uint8_t *jst(level_char_init *rec) { return (uint8_t *)rec + 0x11; }
static int8_t col20(int8_t col, uint8_t room) { return (int8_t)(col + (room == 8 ? 10 : 0)); }   /* rooms 7|8 as one row of 20 */
static int frame_flag40(uint16_t f) { return (frame_table_kid[f * 7 + 6] & 0x40) != 0; }

/* 33FD:0054 (2FDF:0687, a dead charid-2 guard in room 3 of level 14): the body turns into a crumbling tile */
void ovl_34024(void)
{
	if (Char.alive == 6) {
		uint8_t t = get_tile(Char.curr_row, Char.curr_col, Char.room);
		int si = random_2751(2);
		if (si != 0 && t == 1 && (int16_t)trob_count < 4) {
			int8_t tp = (int8_t)(row_tilepos(Char.curr_row) + Char.curr_col);
			if (tp < 0x16 || tp > 0x18) {
				ROOM_TILES(3)[tp] = 0xA; *(uint16_t *)&ROOM_ATTRS(3)[tp] = (uint16_t)((si | 0x200) << 5);
				get_room_address(3); start_0a_pub(tp, 3);   /* (1375:0E8C marks it for redrawing) */
			}
		}
		Char.alive++;
	} else if (Char.alive >= 0x60 && Char.f19 != 0xEF) { seqtbl_offset_char(0xEF); play_sound(0xC5); Char.alive++; }
	if (Char.f0f == 0 && Char.alive > 6) Char.alive++;
}
/* 33FD:02E4 (the prince leaving room 7/8 while a Jaffar is dying): the side the living one is on */
int ovl_342b4(void)
{
	int si = chars[0].f19 == 0xF3 ? 0 : 1;
	if (drawn_room == 7 && chars[si].curr_col >= 0xA) return 1;
	if (drawn_room == 8 && chars[si].curr_col < 0) return 0;
	return -1;
}
/* 33FD:12E4 (2D3E:07AA, a Jaffar's record when he stays behind): he jumps to the waypoint he was heading for */
void ovl_352b4(level_char_init *rec)
{
	uint8_t *st = jst(rec);
	if (!((next_room == 7 || next_room == 8) && level_kind == 6) && st[1] != st[2]) {
		st[2] = st[1]; const waypoint *e = wp(st[1]);
		Char.curr_row = e->row; Char.curr_col = e->col; Char.x = e->x_min + 6;
		if (Char.room == 8) { Char.x -= 0x140; Char.curr_col -= 10; }
		Char.direction = e->dir; char_y_to_floor();
		seqtbl_offset_char(2); play_seq();
		rec->tilepos = tilepos_or_30_pub(Char.curr_row, Char.curr_col);
		rec->x = Char.x; rec->direction = Char.direction; rec->seq_pos = Char.seq_pos; rec->seq_id = Char.seq_id;
	}
	*(int16_t *)(st + 4) = Char.y;
}
/* 33FD:1416 (clear_char of a Jaffar leaving room 6 with seq 0xF0): he waits above room 7's first waypoint */
void jaffar_leaves_room6(void)
{
	Char.curr_col = 0x11; Char.x = 0x2C8; Char.curr_row = -3; Char.y = -0x85; Char.direction = wp(0)->dir;
	room_char_record((int8_t)Char.index, Char.room)->w15 = 0;
	seqtbl_offset_char(2); play_seq(); save_to_record_pub();
}
/* 33FD:06AE (a hit by a Jaffar): the other Jaffars of his room standing still start (seq 0xF0) */
void ovl_33fd_6ae(void)
{
	char_type saved = Char; int8_t n = room_nchars(Opp.room);
	for (int8_t i = 0; i < n; i++) {
		load_char(i);
		if (Char.charid == 6 && Char.index != Opp.index && Char.frame == 0xF) { seqtbl_offset_char(0xF0); save_char(); }
	}
	Char = saved;
}
/* 33FD:101A: a Jaffar of room 6 */
static void jaffar_room6(void)
{
	if (Char.alive >= 0) return;
	int8_t n = ROOM_REC(Char.room)->nchars; if (Opp.charid == 1) n--;
	if (n == 1 && Char.f19 != 0xF0 && Kid.alive < 0) { seqtbl_offset_char(0xF0); return; }
	if (Char.frame == 0xF) {
		if (Kid.alive >= 0 || Kid.f19 == 0x2C || Kid.f19 == 6 || Kid.charid != 0 || !frame_flag40(Kid.frame) || Kid.action == 3 || Kid.action == 4) return;
		int go = 0; int16_t si = (int16_t)opp_distance();
		if (Char.direction == Opp.direction) { si -= 0xF; go = si > 0x12 && si < 0x30; }
		else if (si < 0) { si = -0xF - si; if (si > 0x12 && si < 0x30) { Char.direction = ~Char.direction; go = 1; } }
		else if (Kid.frame == 0x91) { si -= 0xF; go = si > 0x12 && si < 0x34; }
		if (go) { Char.x = char_dx_forward(0xF); Char.f10 = 1; seqtbl_offset_char(0x4B); }
	} else if (Char.frame == 0xAB) { Char.f10 = 0; seqtbl_offset_char(0x5C); }
	else if (Char.frame == 0x34 && Kid.alive > 0) { seqtbl_offset_char(0xC5); play_sound(0x106); }
	else if (Char.frame == 0x6D) seqtbl_offset_char(0x31);
}
/* 33FD:13A2: the prince stands in the waypoint's catch zone on its side dir (-1 / 1) */
static int in_zone(int8_t dir, int k)
{
	const waypoint *e = wp(k); int d = dir >= 0 ? 1 : 0;
	if (e->row_by_dir[d] != Opp.curr_row) return 0;
	int8_t c = col20(Opp.curr_col, Opp.room);
	return c >= e->col_min[d] && e->col_max[d] >= c;
}
static void wp_next(uint8_t *st) { st[1] = st[1] == 3 ? 0 : st[1] + 1; }   /* 33FD:098C */
static void wp_prev(uint8_t *st) { st[1] = st[1] == 0 ? 3 : st[1] - 1; }   /* 33FD:09A8 */
/* 33FD:08C0: the prince's body is on Jaffar's row and within his reach */
static int can_cast(void)
{
	if ((int8_t)Opp.f12 <= 0 || (Opp.room != 7 && Opp.room != 8) || level_kind != 6 || Opp.f19 == 0xF1 || Char.curr_row != Opp.curr_row) return 0;
	if (!frame_flag40(Char.frame) || Char.frame == 0x6D) return 0;
	int8_t c = col20(Char.curr_col, Char.room);
	if (Char.curr_row == 0) return !(c > 6 && c < 0xD);
	if (Char.curr_row == 1) { int8_t o = col20(Opp.curr_col, Opp.room); return c <= 0xC ? o <= 0xC : o >= 0xE; }
	return Char.curr_row == 2;
}
/* 33FD:0728: face the prince, then cast; or stop moving */
static void face_and_cast(void)
{
	int cx = -1;
	if (Char.frame == 0xF) cx = (Char.x > Opp.x ? Char.direction != -1 : Char.direction != 0) ? 5 : 0xF2;
	else if (((Char.frame >= 1 && Char.frame <= 0xE) || (Char.frame >= 0x31 && Char.frame <= 0x38)) && Char.f19 != 0xD && Char.f19 != 5) cx = 0xD;
	if (cx != -1) seqtbl_offset_char(cx);
}
/* 33FD:0A1E: walk towards the waypoint's x range; -1 or a sequence */
static int walk_to(uint8_t *st)
{
	if (Char.frame != 0xF) return -1;
	if (st[0] == 2 && can_cast()) { face_and_cast(); return -1; }
	const waypoint *e = wp(st[1]); int16_t cx = Char.x;
	if (e->dir != Char.direction) cx += (int8_t)ds_byte(0xCF9 + e->dir) * 6;
	if (e->col < 10) { if (Char.room == 8) cx += 0x140; } else if (Char.room == 7) cx -= 0x140;
	if (e->x_min > cx) { if (Char.direction == 0) { control_standing_step(e->x_min - cx + 2); return -1; } return 5; }
	if (e->x_max < cx) { if (Char.direction == -1) { control_standing_step(e->x_min - cx + 2); return -1; } return 5; }
	if (e->dir != Char.direction) return 5;
	st[2] = st[1]; return -1;
}
/* 33FD:0AF2 / 0C4A / 0D72 / 0E8E: the moves between the waypoints (runs, jumps, climbs) */
static int to_wp0(uint8_t *st)
{
	int8_t c = col20(Char.curr_col, Char.room); const waypoint *e = wp(st[1]);
	if (st[2] == 3) {
		if (Char.frame == 0xF) {
			if (c == 6) { control_jump_031062(); return -1; }
			if (c <= 7 || e->col - 1 <= c) return walk_to(st);
			control_standing_forward(); return -1;
		}
		if (Char.frame == 7 || Char.frame == 0xB) {
			if ((c >= 5 && c <= 6) || (c >= 0xA && c <= 0xB)) { control_runjump(4); return -1; }
			return e->col - 1 <= c ? 0xD : -1;
		}
		return -1;
	}
	if (Char.curr_row == 2) {
		if (Char.frame == 0xF) { if (Char.direction == 0) { control_jumpup_grab_031074(); return -1; } return 5; }
		if (Char.frame == 0x5B) control_hanging_climb();
		return -1;
	}
	if (Char.curr_row == 1) {
		if (Char.frame == 0xF) {
			if (Char.direction == 0) { if (c >= 0x11) return 5; control_standing_forward(); return -1; }
			if (c >= 0x11) { control_jumpup_grab_031074(); return -1; }
			return 5;
		}
		if (Char.frame == 7 && c >= 0x11) return 0xD;
		if (Char.frame == 0x5B) { control_hanging_climb(); return -1; }
		return Char.frame == 0x6D ? 0x31 : -1;
	}
	return walk_to(st);
}
static int to_wp1(uint8_t *st)
{
	const waypoint *e = wp(st[1]);
	if (st[2] < st[1]) {
		if (Char.curr_row == 0) {
			if (Char.direction == 0 && Char.f19 != 5) return 5;
			if (Char.frame == 0xF) control_standing_down();
			return -1;
		}
		if (Char.curr_row == 1) {
			if (Char.frame >= 0x57 && Char.frame <= 0x5B) { control_frame81_0313c6(); return -1; }
			if (Char.frame == 0xF) { if (Char.direction == -1) { control_standing_forward(); return -1; } return 5; }
			return Char.frame == 0x6D ? 0x31 : -1;
		}
		if (Char.curr_row == 2) return Char.frame == 0x6D ? 0x31 : walk_to(st);
		return -1;
	}
	int8_t c = col20(Char.curr_col, Char.room);
	if (c == wp(2)->col && Char.frame == 0xF) { control_standing_forward(); return -1; }
	if (c >= 6 && c <= 7) { control_runjump(4); return -1; }
	if (c <= 0xA) return -1;
	if (Char.frame == 7 && e->col - 1 <= c) return 0xD;
	return walk_to(st);
}
static int to_wp2(uint8_t *st)
{
	if (st[2] < st[1]) {
		int8_t c = col20(Char.curr_col, Char.room);
		if (c == wp(1)->col && Char.frame == 0xF) { control_standing_forward(); return -1; }
		if (c >= 0xB && col_x_left[2] + 0x24 > Char.x && Char.f19 != 4 && (Char.frame == 7 || Char.frame == 0xB)) {
			Char.x = char_dx_forward(2); control_runjump(4); return -1;
		}
		if (c <= 6 && Char.frame == 7) return 0xD;
		return walk_to(st);
	}
	if (Char.curr_row == 0) {
		if (Char.frame == 0xF) { if (Char.direction == 0) return 5; control_standing_down(); }
		return -1;
	}
	if (Char.curr_row == 1) {
		if (Char.frame >= 0x57 && Char.frame <= 0x5B) { control_frame81_0313c6(); return -1; }
		if (Char.frame == 0xF) { if (Char.direction == -1) { control_standing_forward(); return -1; } return 5; }
		return Char.frame == 0x6D ? 0x31 : -1;
	}
	if (Char.curr_row == 2) return Char.frame == 0x6D ? 0x31 : walk_to(st);
	return -1;
}
static int to_wp3(uint8_t *st)
{
	const waypoint *e = wp(st[1]); int8_t c = col20(Char.curr_col, Char.room);
	if (st[2] == 0) {
		if (Char.frame == 0xF) {
			if (c > 0xA) { control_standing_forward(); return -1; }
			return e->col >= c ? walk_to(st) : -1;
		}
		if (Char.frame == 7 || Char.frame == 0xB) {
			if (e->col >= c) return 0xD;
			if ((c >= 0xE && c <= 0xF) || (c >= 9 && c <= 0xA)) control_runjump(4);
		}
		return -1;
	}
	if (Char.curr_row == 2) {
		if (Char.frame == 0xF) control_jumpup_grab_031074();
		else if (Char.frame == 0x5B) control_hanging_climb();
		return -1;
	}
	if (Char.curr_row == 1) {
		if (Char.frame == 0xF) {
			if (Char.direction != 0) { control_jumpup_grab_031074(); return -1; }
			if (c == 6) { control_standing_step(0x20); return -1; }
			return 5;
		}
		if (Char.frame == 0x5B) { control_hanging_climb(); return -1; }
		return Char.frame == 0x6D ? 0x31 : -1;
	}
	return walk_to(st);
}
/* 33FD:09C4 (DS:1A62, never set, would stop it) */
static void jaffar_move(uint8_t *st)
{
	int r = -1;
	switch (st[1]) { case 0: r = to_wp0(st); break; case 1: r = to_wp1(st); break; case 2: r = to_wp2(st); break; case 3: r = to_wp3(st); break; }
	if (r != -1) seqtbl_offset_char(r);
}
/* 33FD:0FB8 (frame 0x11A of the cast): the prince dies */
static void cast_kills(void)
{
	seqtbl_offset_char(0xF4);
	int8_t idx = (int8_t)Char.index; save_char(); loadkid();
	seqtbl_offset_char(0x47); play_sound(0xE); take_hp(100); seq_set_85f8(0x11);
	Kid = Char; load_char_and_opp(idx);
}
/* 33FD:0860: patrol: at a waypoint, the next one away from the prince */
static void jaffar_patrol(uint8_t *st)
{
	if (st[0] != 1) st[0] = 1;
	if (st[2] == st[1]) { if (in_zone(-1, st[1])) wp_prev(st); else if (in_zone(1, st[1])) wp_next(st); }
	if (st[2] != st[1]) jaffar_move(st);
}
/* 33FD:0794: chase: cast when possible, else the next waypoint towards the prince */
static void jaffar_chase(uint8_t *st)
{
	int go = 1;
	if (st[0] != 2) st[0] = 2;
	if (Char.f19 == 0xF2) {
		if (Char.frame == 0x119) play_sound(0x3E);
		else if (Char.frame == 0x11A && can_cast()) cast_kills();
		go = 0;
	} else if (st[2] == st[1]) {
		if (Char.curr_row == Opp.curr_row && can_cast()) face_and_cast();
		else if (Opp.f19 != 0xF1) { if (in_zone(-1, st[1])) wp_next(st); else wp_prev(st); }
	}
	if (go && st[2] != st[1] && (int8_t)Opp.f12 > 0) {
		if (Char.curr_row == 1 && can_cast()) { face_and_cast(); return; }
		jaffar_move(st);
	}
}
/* 33FD:1186: a Jaffar outside room 6 */
static void jaffar(void)
{
	level_char_init *rec = room_char_record((int8_t)Char.index, Char.room); uint8_t *st = jst(rec);
	int moving = st[0] == 1 && st[2] != st[1], patrol, chase = 0;
	if (Opp.charid == 0) {
		if (Opp.f10 == 1 && !(Char.frame >= 0xCF && Char.frame <= 0xD2) && Char.frame != 0x9E) {
			int d = opp_distance(); if (d < 0) d = -d;
			if (d < 0x3E) {   /* the prince draws his sword: he is swept away */
				int8_t idx = (int8_t)Char.index; save_char(); loadkid();
				seqtbl_offset_char(0xF1); sound_1611_01a8(0x109); Char.f10 = 0xFF; Kid = Char; load_char(idx);
			}
			patrol = moving;
		} else if (Char.curr_row == Opp.curr_row && (int16_t)word_5cbe >= 4) patrol = (int8_t)Opp.f12 > 2 || Opp.f19 == 0xF2;
		else { chase = Opp.f10 == 0xFF; patrol = moving; }
	} else {
		patrol = (int8_t)Opp.f12 > 2 || Opp.f19 == 0xF2;
		chase = !patrol;
	}
	if (Char.f19 == 0xF3 && Char.frame == 0x15F) counter_5cec++;   /* the last Jaffar is dead: the level is won */
	if (patrol) jaffar_patrol(st); else if (chase) jaffar_chase(st);
}
/* 33FD:0694 (2D3E:1864, charid 6) */
void ovl_33fd_694(void) { if (Char.room == 6 && level_kind == 6) jaffar_room6(); else jaffar(); }
/* 33FD:1CEA (the kind-6 hook after a character moves): a fireball on Jaffar's row hits him */
void ovl_kind6_char(void)
{
	if (Char.charid != 6 || Char.alive >= 0 || Char.f19 == 0xF3) return;
	for (int n = 1; ; n++) {
		mob_type *m = find_mob_pub(n, 0xC);
		if (!m) break;
		if (m->row != (uint8_t)Char.curr_row || (m->wd & 8)) continue;
		int16_t dx = m->room == Char.room ? 0 : (Char.room == 8 && level_kind == 6) ? -0x140 : 0x140;
		dx += m->w7 + m->x + 0x71;
		if (dx + 0x22 <= char_x_left || dx >= char_x_right) continue;
		m->wd = 8; m->x = m->w7 > 0 ? char_x_left - 0x7D : char_x_right - 0x87;
		take_hp(100); seqtbl_offset_char(0xF3); play_seq(); play_sound(0x3E);
		next_room = Char.room;
	}
}
extern int16_t fireball_width;
/* 33FD:1DC0: a fireball meets a wall: it bursts there */
static void fireball_wall(void)
{
	int16_t bx = fireball_width + cur_mob.x - 0xE; if (cur_mob.w7 < 0) bx -= 2 * fireball_width;   /* DS:0842 */
	int16_t a = bx < 0 ? -bx : bx; int8_t col = (int8_t)(bx < 0 ? -(a >> 5) : a >> 5); if (bx < 0) col--;
	if (!tile_is_wall_kind(get_tile(cur_mob.row, col, cur_mob.room))) return;
	cur_mob.x = col * 32 + 0xE; if (cur_mob.w7 < 0) cur_mob.x += 0x20;
	cur_mob.wd = 8; play_sound(0x103);   /* (stops sound 0x281A) */
}
/* 33FD:1ED0 (1375:1B10, falling object type 0xC): a fireball flies, speeding up to 16, animating 0..3; burst 8..13 */
void fireball_update(void)
{
	if (cur_mob.speed < 0) return;
	int si = cur_mob.wd & 7;
	if (cur_mob.wd & 8) { if (si >= 5) cur_mob.speed = -1; else si++; }
	else {
		if (cur_mob.w7 > 0) { if (cur_mob.w7 < 0x10) cur_mob.w7 += 2; } else if (cur_mob.w7 > -0x10) cur_mob.w7 -= 2;
		cur_mob.x += cur_mob.w7;
		if (cur_mob.room == 0) cur_mob.speed = -1;   /* (stops sound 0x2812) */
		else { fireball_wall(); if (!(cur_mob.wd & 8)) { si = si >= 3 ? 0 : si + 1; play_sound(0x102); } }
	}
	if (cur_mob.speed == -1) return;   /* (0FB3:2B1C palette when the prince has <= 2 hp) */
	cur_mob.wd = (int16_t)((cur_mob.wd & 0xFFF8) | si);
}
/* 33FD:1F8A (2FDF:048C, the spirit's cast frames 0x110..0x119): the fireball leaves at 0x119 */
void ovl_35f5a(void)
{
	if (Char.frame == 0x119) {   /* 33FD:1FA0 */
		cur_mob.x = char_dx_forward(0x1B) - 0x82; cur_mob.y = Char.y - 0x1D; cur_mob.room = Char.room; cur_mob.speed = 0;
		cur_mob.w7 = (int8_t)ds_byte(0xCF9 + Char.direction) << 3; cur_mob.type = 0xC;
		cur_mob.row = (uint8_t)y_to_row(cur_mob.y); cur_mob.wd = 2;
		fireball_wall(); add_mob_pub(); play_sound(0x102);
	}
	play_sound(0x102);
}
/* 33FD:1E4A (2FDF:19D4, shift: the spirit in rooms 7/8 casts, costing 2 hp) */
int spirit_cast(void)
{
	if ((int8_t)Char.f12 <= 2) return -1;
	take_hp(2); sound_1611_01a8(0x10A); return 0xF2;
}
/* 33FD:1BE6 (1375:205C, drawing a falling object of type 0xC): the state it leaves: a fireball in the left or right
 * room that shows is moved into the drawn room's coordinates, and DS:0842 keeps the drawn image's width (the wall test
 * uses it; 17 at start) */
int16_t fireball_width = 17;   /* DS:0842 */
void fireball_draw_state(void)
{
	int vis = 1;
	if (cur_mob.room == drawn_room) vis = cur_mob.x <= 0x1C1 && cur_mob.x >= 0;
	else if (cur_mob.room == room_L) { if (fireball_width + cur_mob.x > 0x140) { cur_mob.x -= 0x140; cur_mob.room = drawn_room; } else vis = 0; }
	else if (cur_mob.room == room_R) { if (cur_mob.x - fireball_width < 0x140) { cur_mob.x += 0x140; cur_mob.room = drawn_room; } else vis = 0; }
	else vis = 0;
	if (!vis) return;
	int16_t h, w; int image = (cur_mob.wd & 8 ? 0x134 : 0x130) + (cur_mob.wd & 7);   /* 33FD:1EB0 */
	if (res_image_size(2, (int16_t)image, &h, &w)) fireball_width = w;   /* (DS:0828 = h) */
}
