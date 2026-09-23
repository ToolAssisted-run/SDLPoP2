/* Loose floors and falling floors (1375:184E..1FBA, PoP1's mobs), buttons and door links (1375:10E0..16BC),
 * gates (1375:0910). Transcribed from the disassembly. */
#include <stdlib.h>
#include "types.h"
#include "globals.h"

mob_type mobs[30]; uint16_t mob_count;    /* DS:293E (13 bytes each), DS:6186 */
mob_type cur_mob;                          /* DS:6662 */
int16_t cur_mob_index;                     /* DS:66C6 */
static const uint8_t *door_speeds_open, *door_speeds_close;   /* DS:0776 (by state 0..3, signed) / DS:076C (state 4..8) */
static const uint8_t *exit_door_speeds;   /* DS:0764 */
static const int16_t *mob_floor_depth;    /* DS:0810: how far below room 0 a falling tile of each type lands */
void mobs_set_tables(const uint8_t *ds) { door_speeds_close = ds + 0x76C; door_speeds_open = ds + 0x776; exit_door_speeds = ds + 0x764; mob_floor_depth = (const int16_t *)(ds + 0x810); }

static uint8_t *room_tiles(uint8_t room) { return room ? level.tiles[room - 1] : tiles0; }   /* DS:2B9A + room*30 */
static uint32_t *room_attrs(uint8_t room) { return (uint32_t *)((uint8_t *)&level + 0x348) + room * 30; }   /* DS:2F00 + room*0x78 */
static uint16_t *attr_lo(uint8_t room, int8_t tp) { return (uint16_t *)&room_attrs(room)[tp]; }

/* door links: level+0x12C0 (DS:3E78), 5 bytes: room, tile position, timer, flag, next (0xFD ends) */
static uint8_t *link_entry(int i) { return (uint8_t *)&level + 0x12C0 + i * 5; }
static uint8_t link_room(int i) { return link_entry(i)[0]; }              /* 1375:15BE */
static int8_t link_tilepos(int i) { return (int8_t)link_entry(i)[1]; }   /* 1375:1562 */
static uint8_t link_timer(int i) { return link_entry(i)[2]; }            /* 1375:152E */
static void set_link_timer(int i, uint8_t v) { link_entry(i)[2] = v; }   /* 1375:1546 */
static uint8_t link_next(int i) { return link_entry(i)[4]; }             /* 1375:1516 */

/* 1375:2598 */
trob_type *get_trob(int8_t tp, uint8_t room) { for (int i = 0; i < (int16_t)trob_count; i++) if (trobs[i].tilepos == tp && trobs[i].room == room) return &trobs[i]; return NULL; }

/* 1375:11A6: a button acts on a gate (4 opener/raise, 5 raise, 6 closer, 0xE stuck-open plate); returns the gate's new animation state or -1 */
static int gate_trigger(uint8_t button, int8_t tp, uint8_t room)
{
	uint16_t *a = (uint16_t *)&curr_room_attrs[tp]; uint16_t pos = *a & 0xFF, hi = *a & 0xFF00;   /* pointers from the chain's get_room_address */
	switch (button) {
	case 0x22: return ovl_button22(room, tp);
	case 5:
		if (pos == 0xFF) return -1;
		if (pos >= 0xC8) {
			*a = hi + 0xFA;
			if (Char.alive < 0) { if (next_room == 0 || Char.room == next_room) return -1; }
			*a = hi + 0xFF; return 3;
		}
		*a = ((pos + 3) & 0xFC) + hi;
		if (Char.alive >= 0) return 3;
		if (next_room != 0 && Char.room != next_room) return 3;
		return 2;
	case 6: {
		trob_type *t = get_trob(tp, room);
		if (is_dead_frame(Char.frame) && Char.alive >= 2) return -1;
		if ((t == NULL && pos != 0) || (t && t->state != 4)) return 4;
		return -1;
	}
	case 0xE:
		if (pos < 0xC8) return 3;
		*a = hi + 0xFF; return -1;
	}
	return -1;
}
/* 1375:12CC: what a button does to the tile a door link points at */
static int trigger_target(uint8_t button, int8_t tp, uint8_t room, uint8_t t)
{
	switch (t) {
	case 4: return gate_trigger(button, tp, room);
	case 0x11:   /* exit door */
		if (room == level.start_room || level_number != 8 || byte_5cba == 1) {
			int r = (*attr_lo(room, tp) & 0xFF) == 0 ? 1 : -1;
			if (r == 1 && !sound_playing_8426() && level_number != 13) play_sound(0x1B);
			return r;
		}
		return -1;
	case 0x19: ovl_347c_b3e(room, tp, button == 5 ? 2 : 1); return -1;
	case 0x1B: return ovl_2a31_dad(room, tp);
	case 0x24: if (!(curr_modifier & 0x800)) rock_drop(room, tp); return -1;   /* DS:6130 & 8: still the pressing button's modifier */
	}
	return -1;
}
/* 1375:10E0: follow a door-link chain from a pressed button */
void trigger_links(int link, uint8_t button)
{
	if (link == 0xFD) return;
	do {
		uint8_t room = link_room(link); int8_t tp = link_tilepos(link);
		get_room_address(room); uint8_t t = curr_room_tiles[tp];
		int r = trigger_target(button, tp, room, t);
		if ((int8_t)r >= 0) add_trob(t, r, tp, room);
		link = link_next(link);
	} while (link != 0xFD);
}
/* 1375:15D4: a button is pressed (link -1 = the tile's own link) */
void press_button(int link, uint8_t tile)
{
	if (tile == 0) tile = curr_tile;
	if (link == -1) link = (uint8_t)curr_modifier;
	uint8_t timer = link_timer(link); int8_t tp = curr_tilepos; uint16_t mod = curr_modifier | 0x800; uint8_t room = curr_room;
	if (timer != 0x1F) {
		set_link_timer(link, 5);
		if (timer < 2) {
			add_trob(tile, 1, curr_tilepos, curr_room);
			if (Char.frame != 0xB9 && !(curr_room == 4 && level_number == 13)) play_sound(3);
			trigger_links(link, tile); word_6140 = 1;
			*attr_lo(room, tp) = mod; return;
		}
	}
	trigger_links(link, tile);
	*attr_lo(room, tp) = mod;
}
/* 1375:16A0: held down for good (a character stands on it) */
void press_button_hold(void) { uint8_t link = (uint8_t)curr_modifier; set_link_timer(link, 0x1F); press_button(link, curr_tile); }
/* 1375:16BC: button animation: count the link timer down, release at 1 */
void anim_button(void)
{
	if ((int8_t)cur_trob.state < 0) return;
	uint8_t link = (uint8_t)anim_mod, timer = link_timer(link);
	if (timer >= 0x1F) { cur_trob.state = 0xFF; return; }
	set_link_timer(link, timer - 1);
	if (timer < 2) { cur_trob.state = 0xFF; anim_mod &= ~0x800u; }
}

/* 1375:0910: gate animation. Position (modifier low byte) 0 closed .. 200 open, 0xFF stuck open.
 * States 0/1 close slowly, 2/3 open (+4), 4..8 slam shut faster each tick. */
static void gate_done(int arg)   /* 1375:0AE8 */
{
	cur_trob.state = 0xFF; play_sound(level_kind == 3 ? 8 : 6);
	loadkid();
	if (cur_trob.tilepos == ((Char.curr_row - 1) >= 0 ? (Char.curr_row - 1) * 10 : (Char.curr_row - 1) * 10 + 9) + Char.curr_col && Char.direction == -1 && Char.action == 2 && arg == 0) {
		seqtbl_offset_char(0x2F); Kid = Char;   /* the gate closes on the prince hanging below it */
	}
}
void anim_gate(void)
{
	int si = (uint8_t)anim_mod; int8_t st = cur_trob.state;
	if (st < 0) return;
	if (st >= 4) {
		if (st < 8) { st++; cur_trob.state = st; }
		si -= door_speeds_close[st];
		if (si > 0) goto write;
		si = 0;
		if (level_kind == 3) {
			int8_t row = cur_trob.tilepos / 10, col = cur_trob.tilepos % 10;
			if (cur_trob.room != Kid.room) { if (cur_trob.room == room_L) col -= 10; else if (cur_trob.room == room_R) col += 10; }
			if (Kid.f19 == 0x76 && Kid.curr_row == row && (Kid.curr_col == col || Kid.curr_col - col == 1)) si = 0x24;
		}
		cur_trob.state = 0xFF; goto write;
	}
	if (si == 0xFF) { gate_done(si); goto write; }
	si += (int8_t)door_speeds_open[st];
	if (st == 0 || st == 1) {
		if (si <= 0) { si = 0; gate_done(0); }
		else if (si < 0xC8) { cur_trob.state = 1; if (level_kind == 3) si = gate_squeeze(si); }
		else if (level_number == 5 && cur_trob.room == 7) si -= (int8_t)door_speeds_open[st];   /* 1375:0ACA: AX still holds the speed: no movement */
		goto write;
	}
	if (si >= 0xC8) {
		if (st < 3) { si = 0xFA; cur_trob.state = 0; }
		else { si = 0xFF; gate_done(0xFF); }
	}
write:
	anim_mod = (anim_mod & 0xFFFF0000u) | (uint16_t)(((uint16_t)anim_mod & 0xFF00) + si);
}

/* 1375:184E: something heavy lands on / steps on a loose floor (arg = fall speed, 0 = just stepped on) */
void loose_floor_touch(int8_t arg)
{
	if (Char.charid == 1 && arg == 0) { loose_floor_shake(); return; }
	uint16_t mod = curr_modifier & 0xFFBF;
	if (arg) mod = (mod & 0xFFFB) | 0xB;
	*attr_lo(curr_room, curr_tilepos) = mod;
	add_trob(0xB, arg, curr_tilepos, curr_room); play_sound(0x14);
}
/* 1375:18B0: shake a loose floor */
void loose_floor_shake(void)
{
	if ((uint8_t)curr_modifier & 0xF) return;
	int u = (((uint8_t)curr_modifier & 0x30) >> 4) + 1;
	if ((Char.curr_col != tile_col && u != 3) || Char.charid == 1) {
		curr_modifier = (curr_modifier & 0xFFCF) | ((u | 4) << 4);
		*attr_lo(curr_room, curr_tilepos) = curr_modifier | 0x40;
	}
	add_trob(0xB, 1, curr_tilepos, curr_room); play_sound(0x14);
}
/* 1375:191C: a heavy landing shakes the loose floors of a row */
void shake_loose_row(int8_t row, uint8_t room)
{
	mob_type saved = cur_mob;
	for (int8_t col = 0; col < 10; col++) {
		uint8_t t = get_tile(row, col, room);
		if (t == 0x1A) ovl_347c_e8e(); else if (t == 0xB) loose_floor_shake(); else if (t == 0xF) ovl_347c_126();
	}
	if (level.type == 2) skel_row_shake(row, room);
	cur_mob = saved;
}
/* 1375:17FC: the loose floor falls away */
static void remove_loose(int8_t tp, uint8_t room)
{
	room_tiles(room)[tp] = 0;
	*attr_lo(room, tp) = (level_number == 5 && room == 3) ? 0xC000 : (((uint8_t)*attr_lo(room, tp) & 0x80) + 3);
}
/* 1375:19A2 */
void add_mob(void) { if (mob_count < 30) mobs[mob_count++] = cur_mob; }
/* 1375:1744: loose floor animation; at 12 the tile becomes a falling floor */
void anim_loose(void)
{
	anim_mod = (anim_mod & 0xFFFF0000u) | (uint16_t)((uint16_t)anim_mod + 1);
	int dl = (uint8_t)anim_mod & 0xF;   /* shake counter */
	if (cur_trob.state == 0xFF) return;
	if (anim_mod & 0x40) { if (dl >= 4) { cur_trob.state = 0xFF; anim_mod = (anim_mod & 0xFFFFFF00u) | ((uint8_t)anim_mod & 0xB0); } return; }
	if (dl < 0xC) return;
	int si = level_kind == 3 ? ((uint16_t)anim_mod & 0x80) >> 7 : 3;
	remove_loose(cur_trob.tilepos, cur_trob.room);
	anim_mod = (anim_mod & 0xFFFF0000u) | (uint16_t)(si + 3);
	cur_mob.x = (cur_trob.tilepos % 10) * 32; cur_mob.y = (cur_trob.tilepos / 10) * 63 + 0x42; cur_mob.room = cur_trob.room;
	cur_mob.speed = (int8_t)cur_trob.state; cur_mob.w7 = 0; cur_mob.type = si; cur_mob.row = cur_trob.tilepos / 10; cur_mob.wd = 0;   /* DL holds the row from the division at 1375:17BD */
	add_mob();
	cur_trob.state = 0xFF;
}

static int8_t mob_col(void) { int16_t x = cur_mob.x; return (int8_t)(x < 0 ? -((-x) >> 5) : x >> 5); }
/* 1375:1F1A */
static void mob_next_row(void)
{
	if (++cur_mob.row >= 3) { cur_mob.y -= 0xC0; cur_mob.row = 0; cur_mob.room = level_links(cur_mob.room)[3]; }
}
/* 1375:1E86: a falling floor knocks out a loose floor below it */
static void mob_hits_loose(void)
{
	remove_loose(curr_tilepos, curr_room);
	cur_mob.speed >>= 1;
	mobs[cur_mob_index] = cur_mob;
	cur_mob.y = cur_mob.row * 63 + 0x42;
	cur_mob.y = mobs[cur_mob_index].y + 6;
	mob_next_row();
	add_mob();
	cur_mob = mobs[cur_mob_index];
}
/* 1375:1C7E: the falling floor lands on the tile below */
static void mob_land(void)
{
	uint8_t t = get_tile(cur_mob.row, mob_col(), cur_mob.room), btn = 0;
	uint8_t r = curr_room; int8_t c = tile_col, rw = tile_row, tp = curr_tilepos;
	uint8_t *tiles = room_tiles(r);
	switch (t) {
	case 5: tiles[tp] = 0xE; btn = 0xE; /* fall through */
	case 6: case 0x22:
		press_button(-1, btn);
		if (t == 0x22) *attr_lo(r, tp) = curr_modifier | 0x1800;
		get_tile(cur_mob.row, mob_col(), cur_mob.room);
		curr_modifier &= 0xFF00;
		if (t == 5) curr_modifier |= 4; else if (t == 6) curr_modifier |= 8;
		t = curr_tile;
		/* fall through */
	case 1: case 2: case 0xA: case 0x24:
		if (t == 0xA) { trob_type *tr = get_trob(tp, r); if (tr) cur_trob = *tr; }
		/* fall through */
	case 0x13: case 0x20: {
		uint8_t t2 = get_tile(rw, c, r);
		if (t2 == 0x13 || t2 == 0x20) { tiles[tp] = 0x20; break; }
		if (t2 == 0xA || t2 == 1) curr_modifier = 0;
		tiles[tp] = 0xE;   /* rubble */
		if (level_kind == 4) *attr_lo(r, tp) = curr_modifier | 0x40;
		else if ((level_kind == 3 || level_kind == 2) && t2 != 1) *attr_lo(r, tp) = 0;
		break;
	}
	case 0xC: case 0xD: *attr_lo(r, tp) = curr_modifier | 0x20; break;
	default: return;
	}
}
/* 1375:1B7C */
static void mob_fall(void)
{
	if (cur_mob.speed < 0) return;
	if (cur_mob.speed < 0x1D) cur_mob.speed += 3;
	cur_mob.y += cur_mob.speed;
	if (cur_mob.room == 0) { if (mob_floor_depth[cur_mob.type] + 0xC0 <= cur_mob.y) cur_mob.speed = -2; return; }
	if ((cur_mob.row + 1) * 63 > cur_mob.y) return;
	uint8_t t = get_tile(cur_mob.row, mob_col(), cur_mob.room);
	if (t == 0xB) mob_hits_loose(); else if (t == 0xF) ovl_347c_126();
	if (t == 0xB || t == 0xF || tile_is_empty_kind(t)) { mob_next_row(); return; }
	play_sound(2);
	shake_loose_row(cur_mob.row, cur_mob.room);
	cur_mob.y = (cur_mob.row + 1) * 63; cur_mob.speed = -2; cur_mob.wd = 1; mobs[cur_mob_index].wd = 1;
	mob_land();
}
/* 1375:23E8: does the falling floor hit Char? */
static int mob_hits_char(void)
{
	int16_t top = Char.charid == 0xB ? Char.y : (Char.charid == 7 || Char.charid == 8) ? Char.y - 0x14 : Char.y - 0x1E;
	if (Char.alive >= 0 || Char.room != cur_mob.room || top >= cur_mob.y || Char.curr_row != cur_mob.row) return 0;
	int8_t col = mob_col();
	if (col == Char.curr_col) return 1;
	if (Char.charid != 0xB) return 0;
	int16_t d = distance_to_edge_weight();
	if (Char.curr_col + dir_behind[Char.direction + 1] == col) return d > 8;
	return Char.curr_col + dir_front[Char.direction + 1] == col && d < 4;
}
/* 1375:24CE: does it hit a character that is only a room record? */
static int mob_hits_record(const level_char_init *rec)
{
	int16_t top = (rec->tilepos / 10) * 63 + 0x38;
	if (!(level.type == 5 && rec->type == 8)) top -= 0x1E;
	if (top >= cur_mob.y) return 0;
	int16_t x = rec->x - 0x90; int8_t c = (int8_t)(x < 0 ? -((-x) >> 5) : x >> 5);
	return c == mob_col();
}
/* 2FDF:2200: Char is hit by a falling floor */
static void char_hit_by_floor(void)
{
	uint8_t act = Char.action; uint16_t f = Char.frame;
	if ((f >= 5 && f < 0xF) || f == 0x6D) return;
	if (act >= 2 && act != 7) return;
	if (Char.charid == 1) return;
	char_y_to_floor();
	if (take_hp(Char.charid == 0 ? 1 : 100)) {
		if (Char.charid == 0xB) seqtbl_offset_char(0xA9);
		else if (Char.charid == 7 || Char.charid == 8) seqtbl_offset_char(0x92);
		else if (Char.charid == 4) skel_collapse();
		else seqtbl_offset_char(0x16);
		return;
	}
	if (tile_is_empty_kind(get_tile_behind_char())) Char.x = char_dx_forward(-4);
	Char.f24 = 8; play_sound(0xA); seqtbl_offset_char(0x34);
}
/* 2FDF:22CE: a record-only character is hit */
static void record_hit_by_floor(uint8_t room, level_char_init *rec)
{
	if (level.type != 2) rec->hp = 0;
	if (level.type == 5) { if (rec->type == 8) { rec->seq_id = 0xA9; rec->seq_pos = 0; } else remove_record_pub(rec->index, room); }
	else if (level.type == 2) { if (rec->y == 1) rec->y = 0; }
	else { rec->seq_id = 0x16; rec->seq_pos = 0; }
}
/* 1375:2342 */
static void mob_hits_chars(void)
{
	if (cur_mob.room == 0) return;
	loadkid();
	if (mob_hits_char()) { char_hit_by_floor(); Kid = Char; }
	int8_t n = room_nchars(cur_mob.room);
	for (int8_t i = 0; i < n; i++) {
		if (cur_mob.room == Kid.room) { load_char(i); if (mob_hits_char()) { char_hit_by_floor(); save_char(); } }
		else { level_char_init *rec = room_char_record(i, cur_mob.room); if (rec && mob_hits_record(rec)) record_hit_by_floor(cur_mob.room, rec); }
	}
}
/* 1375:1B10 */
static void mob_update(void)
{
	switch (cur_mob.type) {
	case 0: case 1: case 3: mob_fall(); if (cur_mob.speed <= 0) cur_mob.speed++; mob_hits_chars(); break;
	case 4: trap_update(); break;
	case 2: if (level_kind == 3) { rock_fly(); break; }   /* 33FD:02AC */
		/* fall through */
	default: ovl_mob_other(cur_mob.type); break;
	}
}
/* 1375:1A52 */
void falling_floors(void)
{
	int16_t n = mob_count;
	for (cur_mob_index = 0; cur_mob_index < n; cur_mob_index++) { cur_mob = mobs[cur_mob_index]; mob_update(); mobs[cur_mob_index] = cur_mob; }
	int k = 0; for (int i = 0; i < (int16_t)mob_count; i++) if (mobs[i].speed != -1) mobs[k++] = mobs[i];
	mob_count = k;
}
int exit_door_speed(int state) { return exit_door_speeds[state]; }

/* 1375:19DE: the n-th (1-based) live falling object of a type */
static mob_type *find_mob(int n, uint8_t type) { int k = 0; for (int i = 0; i < (int16_t)mob_count; i++) if (mobs[i].type == type && mobs[i].speed != -1 && ++k == n) return &mobs[i]; return NULL; }
/* 1375:1F42: is a landed falling floor lying on the current tile? */
static int mob_on_tile(void)
{
	uint8_t type = level_kind == 3 ? 1 : 3; mob_type *m; int n = 0;
	while ((m = find_mob(++n, type)) != NULL) {
		int16_t x = m->x; int8_t c = (int8_t)(x < 0 ? -((-x) >> 5) : x >> 5);
		if (m->room == curr_room && m->row == tile_row && c == tile_col && m->wd != 0) return 1;
	}
	return 0;
}
/* 33FD:0000 (kind 3): pressure plate 0x22 holding a gate: returns the gate's new state or -1 */
int ovl_button22(uint8_t room, int8_t tp)
{
	uint16_t *a = attr_lo(room, tp); uint16_t pos = *a & 0xFF, hi = *a & 0xFF00;
	int weighted = mob_on_tile();
	trob_type *t = get_trob(tp, room);
	if (t) {
		if (t->state == 0 || t->state == 1) {
			uint16_t mod;
			if (weighted) mod = curr_modifier;
			else {   /* is Char standing on a plate? (curr_* are saved and restored around the lookup) */
				uint8_t s_tile = curr_tile, s_room = curr_room, s_tp = curr_tilepos; int8_t s_col = tile_col, s_row = tile_row; uint16_t s_mod = curr_modifier;
				mod = get_tile(Char.curr_row, Char.curr_col, Char.room) == 0x22 ? curr_modifier : 0;
				curr_tile = s_tile; curr_room = s_room; curr_tilepos = s_tp; tile_col = s_col; tile_row = s_row; curr_modifier = s_mod;
			}
			if (mod & 0x800) { *a = hi + 0xFA; return -1; }
			if (pos > 0xC8) *a = hi + 0xC8;
			t->state = 4; return -1;
		}
		if (t->state == 2 && weighted) t->state = 3;
		return -1;
	}
	if (pos == 0xFF) return -1;
	if (pos == 0) {
		if (!(Char.charid == 4 && Char.curr_row != 0 && level_number == 5 && (Char.room == 10 || Char.room == 7 || Char.room == 12))) {
			if (weighted) return 3;
			return (curr_modifier & 0x800) ? -1 : 2;
		}
		return -1;
	}
	if (pos == 0x24) return -1;
	return Kid.alive < 0 ? 4 : -1;
}
mob_type *find_mob_pub(int n, uint8_t type) { return find_mob(n, type); }
void add_mob_pub(void) { add_mob(); }
int8_t mob_col_pub(void) { return mob_col(); }
int door_speed_0776(int st) { return (int8_t)door_speeds_open[st]; }   /* DS:0776 */
