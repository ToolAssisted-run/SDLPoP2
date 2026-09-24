/* Level kind 1 (level 2), OVL03 at 33FD: the six-tile puzzle of room 1 (tiles 0x1E at positions 12..17; stepping
 * on the right one, DS:2B6A, twice in a row opens the gate at position 10), the gate that only opens, and the
 * kind tick (DS:0658). The drawing's parts (redraw requests, palette rotations) go to weak hooks (the renderer's).
 * Transcribed from the disassembly. */
#include <stddef.h>
#include "types.h"
#include "globals.h"

int8_t puzzle_answer = 0, puzzle_last = -1;   /* DS:2B6A column of the right tile, DS:2B6B column last stood on (-1 none) */
uint8_t byte_14a0 = 0xFF;                     /* DS:14A0 */

/* platform: 194C:8426 is sound `id` still playing (default: yes, so no sound-gated random draws) */

static uint16_t *attr16(uint8_t room, int8_t tp) { return (uint16_t *)&ROOM_ATTRS(room)[tp]; }
/* 1375:25D6: the n-th live animation of a tile */
static trob_type *nth_trob(uint8_t tile, int n)
{
	int k = 0;
	for (int i = 0; i < (int16_t)trob_count; i++) if (trobs[i].tile == tile && trobs[i].state != 0xFF && ++k == n) return &trobs[i];
	return NULL;
}
/* the drawing's part of this tick code (the renderer's: render_kind_desc.c through shell.c; no-ops in the core) */

/* 33FD:0000: the puzzle chime (with MIDI music, DS:2085 bit 1) */
static void chime(void) { if (sound_caps & 2) sound_1611_01a8(0xFB); }
/* 33FD:0620: open the gate */
static void open_gate(uint8_t room, int8_t tp)
{
	if (get_trob(tp, room)) return;
	add_trob(4, 1, tp, room); sound_1611_01a8(0x27);
}
/* 33FD:032A: frames the prince stands still on */
static int standing_frame(uint16_t f)
{
	return f == 0xF || f == 0x6D || f == 7 || f == 0x2E || (f >= 0xB && f <= 0xE) || f == 0x26 || f == 0x2C || f == 0x1A || f == 0x9E || f == 0xAA || f == 0xAB;
}

/* 33FD:07CE: tile 0x1E animation. Attribute bits 4..8 count down a delay; then the low nibble runs
 * down to 0 (state 1, pressed) or up to 10 (state 3, rising) */
void anim_tile1e(void)
{
	uint16_t m = (uint16_t)anim_mod, dx = (m & 0x1F0) >> 4;
	if (dx == 0) {
		int si = m & 0xF, st = (int8_t)cur_trob.state;
		if (st == 3 && si == 0) play_sound(0x2A);
		if ((si == 10 && st == 3) || (si == 0 && st == 1)) {
			if (st == 3 && !nth_trob(0x1E, 2)) chime();
			cur_trob.state = 0xFF;
		} else anim_mod = (anim_mod & 0xFFFF0000u) | (uint16_t)((m & 0xFFF0) | (uint16_t)(st + si - 2));
		if ((int8_t)cur_trob.state > 0) hook_desert_tile1e();   /* 33FD:08BE: redraw requests */
		return;
	}
	if (Kid.f19 != 0x1B && Kid.alive < 0) anim_mod = (anim_mod & 0xFFFF0000u) | (uint16_t)((m & 0xFE0F) | ((dx - 1) << 4));
}
/* 33FD:0A18: start a tile 0x1E: mode 3 on entering the room (rises after a random delay), mode 1 stepped on */
int tile1e_start(uint8_t room, int8_t tp, uint8_t mode)
{
	uint16_t *a = attr16(room, tp), di = *a; int si = di & 0xF, r;
	if (mode == 3 && si == 0) { *a = (di & 0xFE0F) | (uint16_t)((random_2751(10) + 0x15) << 4); r = 1; }
	else if (mode == 1) {
		if (si != 0) { play_sound(0x2B); r = 1; }
		else { r = get_trob(tp, room) != NULL; if (di & 0x1F0) *a = di & 0xFE0F; }
	} else { r = 0; if (mode == 3 && tp % 10 == 7 && !nth_trob(0x1E, 1)) chime(); }
	if (r) { add_trob(0x1E, mode, tp, room); if (mode == 1) hook_desert_press(tp % 10); }   /* 33FD:0904 */
	return r;
}
/* 33FD:099E: step on the tile at room 1 position tp */
static int step_tile(int8_t tp)
{
	int r = tile1e_start(1, tp, 1);
	if (r) {
		uint32_t save = anim_mod; uint16_t *a = attr16(1, tp);
		anim_mod = (anim_mod & 0xFFFF0000u) | *a; anim_tile1e(); *a = (uint16_t)anim_mod; anim_mod = save;
	}
	return r;
}
/* 33FD:09EE (0AFF:0C8A, landing from a jump on level 2): the tile last stood on and the tile landed on are pressed */
void ovl_349be(void)
{
	if (puzzle_last != -1 && puzzle_last >= 2 && puzzle_last <= 7) step_tile(puzzle_last + 10);
	step_tile(Char.curr_col + 10);
}
/* 33FD:0538: tile 4 (the raft's gate) animation: opens to 0x14 and stops (then sound 7 is stopped); each step the
 * description's object 2 is uncovered 2 more pixels to the left and redrawn (hook_desert_gate) */
void anim_gate_kind1(void)
{
	int di = (uint16_t)anim_mod & 0x1F;
	if (di == 0x14) { cur_trob.state = 0xFF; sound_194c_83d2(0x2717); }
	else { di++; if (!sound_playing(0x273D)) play_sound(7); }
	anim_mod = (anim_mod & ~0x1Fu) | (uint16_t)di;
	hook_desert_gate((int8_t)cur_trob.tilepos);
}
/* 33FD:0658 / 06E4: tiles 0x1C / 0x1D (the waves) cycle 0..7 while visible (1375:04CE), their description object
 * redrawn (hook_desert_wave) */
void anim_wave_kind1(void)
{
	if (!anim_visible_pub()) return;
	uint16_t v = (uint16_t)anim_mod & 0xF; v = v == 7 ? 0 : v + 1;
	anim_mod = (anim_mod & ~0xFu) | v;
	hook_desert_wave((int8_t)cur_trob.tilepos);
}
/* the background id of the room description at DS:01AC (roomhooks.c) */
int room_background_id(void) { return room_bg - 1; }   /* DS:01AC -> +1 */
/* 33FD:0380 (level load, 1286:03B6): with room background 6 showing, the puzzle's answer is chosen once
 * (DS:14A0 0..2, 2 = none drawn); the rest draws it */
void kind1_level_init(void)
{
	puzzle_last = -1;
	if (room_background_id() != 6) return;
	if (byte_14a0 == 0xFF) { byte_14a0 = random_2751(2); puzzle_answer = random_2751(4) + 1; }
}
/* 33FD:002A: with the gate closed, count while only the right tile is down; solved after 0x14 ticks */
static void puzzle_check(void)
{
	uint16_t *a = attr16(1, 10), di = *a;
	if (di & 0x1F) return;
	int si = (di >> 8) & 0x1F, ok = puzzle_answer - puzzle_last != -2;
	for (int8_t i = 0; ok && i < 6; i++) {
		int d = *attr16(1, 12 + i) & 0xF;
		if ((d != 0 && i != puzzle_answer) || (d == 0 && i == puzzle_answer && si < 10)) ok = 0;
	}
	if (!ok) { *a = di & 0xE0FF; return; }
	if (si >= 0x14) { open_gate(1, 10); return; }
	if (si == 6) sound_1611_01a8(0x25);
	else if (si == 9) play_sound(0x2D);
	else if (si == 10) step_tile(puzzle_answer + 12);
	si++;
	*a = (di & 0xE0FF) | (uint16_t)(si << 8);
}
/* 33FD:020A: the prince on the puzzle row, and the level's exits */
static void kind1_kid(void)
{
	loadkid();
	if (Char.room == 1) {
		if (Char.curr_col != puzzle_last && standing_frame(Char.frame) && puzzle_last >= 2 && puzzle_last <= 7 && step_tile(puzzle_last + 10)
		    && puzzle_answer - puzzle_last == -2) sound_1611_01a8(0x29);
		if (standing_frame(Char.frame)) puzzle_last = Char.curr_col;
		if (Char.x < 0x82) {
			if ((int8_t)level_number == (int16_t)counter_5cec) { counter_5cec++; seqtbl_offset_char(2); }
			else { seqtbl_offset_char(2); word_087e = -1; }
		}
	} else if (Char.room == 3 && Char.x >= 0x1E9) {
		if (ctrl1_up || ctrl1_down || (Char.direction == 0 && ctrl1_forward)) { seqtbl_offset_char(2); char_y_to_floor(); }
		if (!sound_playing(0x273E) && (ctrl1_shift < 0 || !random_2751(0x14))) play_sound(0x2E);
	}
	Kid = Char;
}
/* 33FD:011A: outside room 1, every third tick while on time (DS:2BA4 0): the palette's colors 0xB0..0xBC, 0xC4..0xC7
 * and 0xBF..0xC3 rotate (2699:0048: the renderer's) */
static void kind1_palette(void)
{
	if (word_2ba4 != 0 || drawn_room == 1 || tick % 3 != 0) return;
	hook_pal_rotate(0xB0, 0xD); hook_pal_rotate(0xC4, 4); hook_pal_rotate(0xBF, 5);
}
/* 33FD:0170 (DS:0658) */
void kind1_tick(void)
{
	kind1_palette(); kind1_kid(); puzzle_check();
	/* the sounds: outside room 1 sound 0x26 (the wind) goes on and music 0x10B stops; in room 1 the wind stops while
	 * 0x10B plays, else the chime plays once nothing else does (the prince alive, not in state 7, the ambient piece
	 * DS:2B9A over, no puzzle tile moving, the gate shut: 33FD:0618) */
	if (drawn_room != 1) {
		if (!sound_playing(0x2736)) play_sound(0x26);
		if (sound_playing(0x280B)) sound_194c_83d2(0x280B);
	} else if (sound_playing(0x280B)) {
		if (sound_playing(0x2736)) sound_194c_83d2(0x2736);
	} else if (Kid.f24 != 7 && Kid.alive < 0 && !sound_playing((uint16_t)(*(uint16_t *)tiles0 + 0x2710)) && !nth_trob(0x1E, 1)
	           && !(*attr16(1, 10) & 0x1F)) chime();
}
