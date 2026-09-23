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
uint16_t word_2b90, word_2b92, word_5cee, word_5cce;   /* DS:2B90 / 2B92 / 5CEE / 5CCE: redraw requests (whole screen, message, new room, flip) */
uint8_t byte_6b6c;                                   /* DS:6B6C */
/* 0823:139C: turn the upside-down view on (0x438 frames) or off */
static void toggle_upside_down(void) { word_5d38 = word_5d38 ? 0 : 0x438; play_sound(word_5d38 ? 0x99 : 0x9A); word_5cce = 1; }
/* 169B:0430: redraw everything (state side: flags cleared, DS:68EA = 2) */
static void redraw_all(void) { word_5cee = 0; if (!word_2b92) draw_chars_state(); word_2b92 = 0; word_922a = 2; }   /* DS:2B92 set: only the message is drawn */
/* 169B:0A30: after each tick: drawing, the upside-down countdown and the message / restart countdown. -2 go on, -1 leave */
int frame_end(void)
{
	tick++;
	if (word_2b90) { redraw_all(); word_2b90 = 0; }
	else if (word_5cee) { drawn_room = next_room; redraw_all(); }
	else if (word_5cce) { word_5cce = 0; redraw_all(); }
	else { draw_chars_state(); if (word_5d38) { if (word_5d38 == 1) toggle_upside_down(); else if (Kid.alive < 0) word_5d38--; } }
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
	if ((int8_t)word_32d8 != (int16_t)counter_5cec && !level_end_sound_playing()) {
		start_hp = Kid.f13;   /* the next level starts with the prince's hp */
		checkpoint_free(); return (int8_t)counter_5cec;
	}
	return frame_end();
}
uint16_t cheat_mode;   /* DS:10C2: the command line's cheat word was given */
/* 18C8:0008 (end of every 169B:0505 pass): with cheats on, the prince is loaded into Char and the cheat keys read */
void frame_wait(void)
{
	if (cheat_mode) { loadkid(); /* 18C8:0800 cheat keys, DS:10DA debug line: not reconstructed */ }
	platform_wait_frame();
}
/* one pass of 169B:0505 */
int play_frame(void) { frame_begin(); int r = frame_after_tick(tick_main()); frame_wait(); return r; }
