/* Level 5's room 3 (OVL11 at 37F0, loaded with room description 0): the mouth in the ceiling (tile 0x1B at row 0,
 * column 6, opened and closed by button links) and the pit trap (tile 0x12 at row 2): crouching on the trap starts
 * seq 0x80, which lifts the prince (frame 0x127); reaching the top while the mouth is closed kills him (seq 0x81).
 * The two flags live in the room's heap block (DS:2B76 +0x3C / +0x3E), allocated zeroed by the room hook.
 * The drawing is render_ovl37f0.c's (tick-time requests through weak hooks). Transcribed from the disassembly. */
#include "types.h"
#include "globals.h"

uint16_t lever5_flag3c, lever5_flag3e;   /* DS:2B76 block +0x3C (lifted; then counts the frames drawn), +0x3E (caught) */
/* the drawing's parts of this tick code (the renderer's, through shell.c; no-ops in the core) */

static int block(void) { return room_bg == 1; }   /* DS:2B76 != 0: description id 0 loaded (room_bg = id + 1) */
/* 37F0:0000 (room hook 0 enter): the block is allocated zeroed (0406; its images built, 002A; the palette, 0436), then
 * music 0x21 */
void lever5_enter(void) { lever5_flag3c = lever5_flag3e = 0; sound_1611_01a8(0x21); }
/* 37F0:0012 (room hook 0 leave): 0622 frees the images and stops sound 0x21 (1611:085C(0x21, 0x24) frees the sounds
 * 0x21..0x24 not playing: memory only); then the guards' palette (the renderer's) */
void lever5_leave(void) { sound_194c_83d2(0x2731); }
/* 37F0:06CE: the mouth (row 0, column 6) is open (attribute nibble >= 3) */
static int mouth_open(void) { get_tile(0, 6, 3); return ((uint8_t)curr_modifier & 7) >= 3; }
/* 37F0:05E8 (2FDF:088F, crouching on tile 0x12 in room 3) */
void ovl_384e8(void)
{
	if (!block()) return;
	seqtbl_offset_char(0x80);
	Char.x = Char.direction == 0 ? 0x120 : 0x10C;
	sound_1611_01a8(mouth_open() ? 0x22 : 0x23);
}
/* 37F0:04FA (2FDF:048C, frame 0x127) */
void ovl_383fa(void)
{
	if (!block()) return;
	if (lever5_flag3c == 0 && Char.frame == 0x127) {   /* the trap's tile (row 2, column 3) goes: 1375:17FC */
		lever5_flag3c = 1; get_tile(2, 3, 3); remove_loose_pub(curr_tilepos, curr_room); hook_lever5_trap(); return;   /* (053B: its image's tiles redrawn) */
	}
	if (lever5_flag3e == 0 && Char.y < 0x37 && !mouth_open()) {
		lever5_flag3c = 1; lever5_flag3e = 1;
		Char.y = 0x14; Char.f24 = 0xA; seqtbl_offset_char(0x81); play_sound(0x24);
		return;
	}
	if (lever5_flag3e != 0 && (int16_t)lever5_flag3c >= 1) { take_hp(100); seq_set_85f8(9); }
}
/* 37F0:0676 (1375:0096, tile 0x1B): the mouth: a trob in state 0 opens it (nibble up to 3), otherwise it closes */
void anim_tile1b(void)
{
	int si = (uint8_t)anim_mod & 7;
	if (cur_trob.state == 0) {
		if (si >= 3) { cur_trob.state = 0xFF; return; }
		if (si == 0) play_sound(0x4E);
		anim_mod = (anim_mod & ~7u) | (uint32_t)(si + 1);
	} else {
		if (si == 0) { cur_trob.state = 0xFF; return; }
		if (si == 3) play_sound(0x4F);
		anim_mod = (anim_mod & ~7u) | (uint32_t)(si - 1);
	}
	hook_lever5_mouth();   /* 37F0:0756: its tiles redrawn */
}
/* 37F0:023A (drawing the prince's object of type 0xD, added by 37F0:044A for the prince on frame 0x127 in room 3 of
 * level 5 once lifted): the drawing counts +0x3C up, the animation's frame (game.c's drawing state) */
void lever5_draw_state(void)
{
	if (block() && lever5_flag3c != 0 && level_number == 5 && Kid.room == 3 && Kid.room == drawn_room && Kid.frame == 0x127) lever5_flag3c++;
}
/* 37F0:0786 (2A31:0DAD, a button link to tile 0x1B): its state: 0 closed, 1 open, -1 moving */
int ovl_2a31_dad(uint8_t room, int8_t tp)
{
	if (curr_modifier & 0x800) return -1;
	uint16_t w = *(uint16_t *)&ROOM_ATTRS(room)[tp];
	return w == 0 ? 0 : w == 3 ? 1 : -1;
}
