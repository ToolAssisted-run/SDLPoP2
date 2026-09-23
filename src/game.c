/* The per-frame loop around the tick (169B:0505 / 0BA6 / 0E7C / 0A30) and the game clock (0823:0D5A). */
#include <string.h>
#include "types.h"
#include "globals.h"

uint16_t minutes_left = 75, clock_ticks = 0x2CF;   /* DS:5CD2 (0x4B at the start), DS:5CEA (719 ticks a minute) */
uint16_t word_5cb6, word_5cc0; int8_t byte_016a;   /* DS:5CB6 / 5CC0; DS:016A (negative: clock stopped) */
uint16_t frame_delay;                               /* DS:24DE */

/* 0823:0D5A: the clock runs while the prince lives; messages at every 5th minute, the last five and the last seconds */
void game_clock(void)
{
	if (byte_016a >= 0 && Kid.alive < 0 && minutes_left != 0) {
		if (--clock_ticks == 0) {
			clock_ticks = 0x2CF;
			if (--minutes_left != 0 && (minutes_left < 5 || minutes_left % 5 == 0)) word_5cd0 = 1;
		} else if (minutes_left == 1 && clock_ticks % 12 == 0) { word_5cd0 = 1; word_5cdc = word_5cda = 0; }
	}
	if (word_5cda != 0) return;
	if (word_5cb6) { word_5cc0 = word_5cb6; word_5cb6 = 0; return; }
	if (!word_5cd0) return;
	/* 0FB3:204C shows "N MINUTES LEFT" / "N SECONDS LEFT" / "TIME HAS EXPIRED" (DS:09BA / 09AA / 099C / 09CA) */
	word_5cdc = word_5cda = 0x18; word_5cd0 = 0;
}
/* 169B:0E7C */
static void frame_timers(void) { if ((int16_t)word_5ce8 > 0) word_5ce8--; if (word_5d36) word_5d36--; }
/* 169B:0BA6: before each tick: frame delay, hp deltas cleared, previous sprite boxes kept, landing states closed */
void frame_begin(void)
{
	frame_delay = Kid.f10 == 1 ? 6 : 5;
	Kid.hp_delta = 0; memcpy((uint8_t *)&Kid + 0x2C, (uint8_t *)&Kid + 0x1B, 8);
	if (Kid.f24 == 8 && (Kid.seq_id != 0x34 || Kid.seq_pos == 0)) Kid.f24 = 0;
	int8_t n = room_nchars(drawn_room);
	for (int8_t i = 0; i < n; i++) {
		load_char(i); Char.hp_delta = 0; memcpy((uint8_t *)&Char + 0x2C, (uint8_t *)&Char + 0x1B, 8);
		if (Char.f24 == 8) Char.f24 = 0;
		save_char();
	}
	frame_timers();
}

void draw_chars_state(void);
kid_sprite_t kid_sprite;   /* the prince's entry of the frame's sprite list (DS:5D3A..), read by biting heads */
/* 0993:07F8 / 0C04 (the prince's pass of the drawing): only where his sprite goes (chtab 2, layer = his charid 0),
 * and the sprite it leaves in obj_* */
/* 0993:0C3A / 0D40 (after the prince's body): his sword's sprites; the last one drawn stays in obj_* */
static void sword_sprite(uint16_t sw, uint8_t chtab)
{
	const uint8_t *e = sword_table + sw * 4; int16_t img = (int16_t)(e[0] | e[1] << 8);
	if (img == -1) return;
	obj_id = img; obj_x += Char.direction == 0 ? (int8_t)e[2] : -(int8_t)e[2];   /* 0AFF:0390 */
	obj_y += (int8_t)e[3]; obj_chtab = chtab;
}
static void kid_sword_sprites(void)
{
	uint16_t sw = cur_frame.sword, f = Char.frame;
	int dead = Char.f24 == 4 || Char.f24 == 5 || Char.f24 == 6 || Char.f24 == 7 || Char.f24 == 0xA || Char.f24 == 0xC
	        || (level_kind == 5 && Char.room != 0x13 && Char.room != 0x10 && Char.room != 0xF && is_dead_frame(f));   /* 0AFF:1A64 */
	if (!dead && sw != 0 && (sw <= 0x90 || (sw >= 0xC8 && sw < 0xFB) || (sw >= 0x124 && sw <= 0x130))
	    && !(Char.charid == 0 && Char.f10 == 0 && !(f == 0x85 || (f >= 0xE5 && f <= 0xF0) || f == 0x143 || f == 0x144)))
		sword_sprite(sw, 0);
	if (sw >= 0x91 && sw <= 0xA8) sword_sprite(sw, 1);
}
static void kid_sprite_state(void)
{
	kid_sprite.valid = 0;
	if (Kid.room == 0 || Kid.room != drawn_room || Kid.frame == 0 || Kid.charid != 0) return;
	char_type save = Char;
	Char = Kid; load_frame_to_obj();   /* (obj_* and cur_frame stay: a character drawn next without an image keeps them) */
	if (obj_chtab == 2) { kid_sprite.valid = 1; kid_sprite.x = obj_x < 0 ? obj_x - 1 : obj_x; kid_sprite.y = obj_y; kid_sprite.image = (uint16_t)obj_id; }   /* 0AFF:08D0 */
	kid_sword_sprites();
	Char = save;
}
/* 1375:1FBA (0FB3:12F4 draws the mobs before the characters): the state their drawing changes */
static void draw_mobs_state(void)
{
	for (int i = 0; i < (int16_t)mob_count; i++) {
		cur_mob = mobs[i];
		if (cur_mob.type <= 1 || cur_mob.type == 3) floor_draw_state();   /* 1375:2062 */
		else if (cur_mob.type == 4) trap_draw_state();                   /* 186A:0008 */
		else if (cur_mob.type == 10 && level_kind == 2) slab_draw_state();   /* 347C:0C22 */
		else if (cur_mob.type == 0xC) fireball_draw_state();   /* 33FD:1BE6 (final.c) */
		mobs[i] = cur_mob;
	}
}
uint16_t word_2b90, word_2b92, word_5cee, word_5cce;   /* DS:2B90 / 2B92 / 5CEE / 5CCE: redraw requests (whole screen, message, new room, flip) */
uint8_t byte_6b6c;                                   /* DS:6B6C */
/* 0823:139C: turn the upside-down view on (0x438 frames) or off */
void toggle_upside_down_pub(void);
static void toggle_upside_down(void) { word_5d38 = word_5d38 ? 0 : 0x438; play_sound(word_5d38 ? 0x99 : 0x9A); word_5cce = 1; }
/* 169B:0430: redraw everything (state side: flags cleared, DS:68EA = 2) */
static void redraw_all(void) { word_5cee = 0; if (!word_2b92) { kind5_room_palette(drawn_room);   /* 0CD6:003A */ draw_mobs_state(); kid_sprite_state(); draw_chars_state(); } word_2b92 = 0; word_922a = 2; }   /* DS:2B92 set: only the message is drawn */
/* 169B:0A30: after each tick: drawing, the upside-down countdown and the message / restart countdown. -2 go on, -1 leave */
int frame_end(void)
{
	tick++;
	if (word_2b90) { redraw_all(); word_2b90 = 0; }
	else if (word_5cee) { drawn_room = next_room; redraw_all(); }
	else if (word_5cce) { word_5cce = 0; room_load(drawn_room); redraw_all(); }   /* 0CD6:02BE */
	else { draw_mobs_state(); kid_sprite_state(); draw_chars_state(); if (word_5d38) { if (word_5d38 == 1) toggle_upside_down(); else if (Kid.alive < 0) word_5d38--; } }
	ambient_sound();   /* 1611:04D0 / 1611:03CC(DS:2B98): the level's ambient sounds pick random variants when none is playing */
	if (word_5cda == 1) {
		if (word_5cdc == 0x24 || word_5cdc == 0x258) { byte_6b6c = 0; return -1; }   /* the countdown after a death ran out */
		return -2;   /* 0FB3:2136 clears the message */
	}
	if (word_5cda && word_5cdc != 0x4A4) {
		if (word_0996 != 0xFFFF) word_5cda--;
		if (word_5cdc == 0x258 && drawn_room == 4 && level_number == 13 && word_0996 == 0) word_0996 = 1;
		/* below 0x78 the message blinks (0FB3:20A4 / 2136) */
	}
	return -2;
}
/* 0993:08D0 / 108A (the character pass of the drawing), the parts that change character state: frame, column,
 * sprite box and the drawn position (+0x26 / +0x28), for the characters standing in the drawn room */
void draw_chars_state(void)
{
	int8_t n = room_nchars(drawn_room);
	for (int8_t i = 0; i < n; i++) {
		load_char(i);
		if (Char.room != drawn_room) continue;
		load_fram_det_col(); load_frame_to_obj();
		if (Char.charid != 0xB && Char.charid != 7 && Char.charid != 8 && get_tile_at_char() == 6 && (frame_flags & 0x40))   /* 0AFF:18C0 */
			Char.y = 63 * Char.curr_row + 0x39;
		set_char_collision();   /* 3212:09BE */
		Char.f26 = Char.x; Char.f28 = Char.y;
		save_char();
	}
}

/* 169B:03AE: the first room of a level (or after a restart); -1 = leave */
int level_first_room(void)
{
	next_room = Kid.room;
	switch_room();   /* 0823:0E72 (returns -2) */
	/* 0AAC:00AE / 0FB3:2136: level name or message cleared; 0FB3:24EA hp display */
	word_5cdc = word_5cda = 0; redraw_all(); frame_delay = 5;
	return -2;
}
/* 169B:0505 after the tick up to 0823:0E72 (tick_main's result r): the rest of the tick, the level-end and restart
 * checks and 169B:0A30. -2 go on, -1 leave the level, else the next level's number */
int frame_after_tick(int r)
{
	if (r == 0) r = tick_tail();
	if (r != 0 && r != -2) return r;   /* 169B:05E0 returns -2 for a normal and for a frozen tick */
	if (word_5cd8) {   /* 169B:120C: restart (or the level a restore picked) */
		word_5cd8 = 0;
		return (int8_t)word_32d8 == (int16_t)counter_5cec ? (byte_6b6c ? (int8_t)word_32d8 : 0) : (int8_t)counter_5cec;
	}
	if ((int8_t)word_32d8 != (int16_t)counter_5cec && !level_end_sound_playing() && !level_end_effect_playing()) {
		if (counter_5cec == -1 || (level_number != 9 && level_number != 5)) sound_stop_all();   /* (levels 5 and 9 play on) */
		start_hp = Kid.f13;   /* the next level starts with the prince's hp */
		checkpoint_free(); return (int8_t)counter_5cec;
	}
	int e = frame_end();
	if (e == -2) {   /* 169B:05A1: DS:2BA4 measures lateness (the frame timer DS:24DE ran out before the frame was done) */
		if (frame_on_time()) { if (word_2ba4) word_2ba4--; /* 2797:0134: wait for the timer */ }
		else { if ((int16_t)word_2ba4 < 0x14) word_2ba4++; sound_pass_late = 1; }   /* (the pass took a timer tick more) */
	}
	return e;
}
uint16_t cheat_mode;   /* DS:10C2: the command line's cheat word was given */
/* 18C8:0008 (end of every 169B:0505 pass): with cheats on, the prince is loaded into Char and the cheat keys read */
void frame_wait(void)
{
	if (cheat_mode) { loadkid(); /* 18C8:0800 cheat keys, DS:10DA debug line: not reconstructed */ }
	platform_wait_frame(); sound_pass_done();
}
/* one pass of 169B:0505 */
int play_frame(void) { frame_begin(); int r = frame_after_tick(tick_main()); frame_wait(); return r; }
void toggle_upside_down_pub(void) { toggle_upside_down(); }
