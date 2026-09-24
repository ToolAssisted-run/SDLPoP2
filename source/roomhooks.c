/* Rooms with their own description (0CD6): a room whose tiles carry attribute bits 0xC000 has a "CUST" resource
 * (room + 159) * 25 in the level kind's scenery file; byte 1 is its background id, which picks a record of hooks
 * (DS:02FE[id] -> DS:01AA.. far pointers): entry 0 runs when the room is loaded, entry 1 when it is left. Only the
 * game-state parts of the hooks are here (most only change the palette or music). Transcribed from the disassembly. */
#include "types.h"
#include "globals.h"

uint8_t byte_5ce7;        /* DS:5CE7: the drawn room has a description */
int16_t room_bg;          /* background id + 1 of the loaded description (DS:01AC -> +1); 0 none (zeroed state) */

/* 0CD6:027A: any tile of the room with attribute bits 0xC000 */
int room_has_description(uint8_t room)
{
	for (int tp = 0; tp < 30; tp++) if ((uint16_t)ROOM_ATTRS(room)[tp] & 0xC000) return 1;
	return 0;
}
/* the frontend's hooks for the palette parts of the entries below (shell.c -> render_palette.c; no-ops for the core) */
__attribute__((weak)) void hook_room_enter(int bg) { (void)bg; }
__attribute__((weak)) void hook_room_leave(int bg) { (void)bg; }
/* entry 0 of the background's hooks */
static void room_enter_hook(int16_t bg)
{
	hook_room_enter(bg);
	switch (bg) {
	case 6: kind1_level_init(); break;   /* 33FD:0380 (level 2's puzzle room) */
	case 0x22:                            /* 37F0:001C (OVL14, level 8 room 9): the sword room's music, once */
		if (word_2bb2 == 0) { if (Kid.curr_col >= 9 && word_2bb0 == 0) { sound_1611_01a8(0x98); word_2bb0 = 1; } else if (Kid.curr_col < 9) sound_1611_01a8(0xFD); }
		break;
	case 0x17: break;   /* 33FD:145E (OVL08, level 14 room 1): palette, DS:2450 = DS:5CC2 (sound), a digital sound */
	case 0x19: case 0x1A: case 0x1B: break;   /* 33FD:1494 (OVL08): palettes; rooms 4/3 sounds 0x10C/0x10D */
	case 0x1C: case 0x1D: case 0x1E: final_room_enter(); break;   /* 33FD:1708 (OVL08, level 14 rooms 6..8) */
	case 0x14: case 0x15:                 /* 347C:0226 (OVL06, ruins): level 9 music, room 2's once (DS:2BAE) */
		if (Kid.curr_col >= 9) { if (drawn_room == 0x10 && level_number == 9) sound_1611_01a8(0x5C); }
		else if (drawn_room == 2 && level_number == 9 && word_2bae == 0 && Kid.curr_col < 5) { sound_1611_01a8(0x5B); word_2bae = 1; }
		break;
	case 0x16: break;                     /* 347C:01EE (OVL06): palette */
	case 0x1F: if (Kid.curr_col >= 9) sound_1611_01a8(0x5C); break;   /* 347C:0FB2 (OVL07, temple) */
	case 0: lever5_enter(); break;        /* 37F0:0000 (OVL11): the heap block DS:2B76 (graphics, flags), music 0x21 */
	case 0x20: break;                     /* 37F0:0510 (OVL13): graphics set up (heap images at DS:2B76) */
	case 0x21: bridge_room_enter(); break;   /* 37F0:0426 (OVL12, level 5 room 10) */
	default: break;   /* DS:02E2: none */
	}
}
/* entry 1 */
static void room_leave_hook(int16_t bg)
{
	hook_room_leave(bg);
	switch (bg) {
	case 0x22: break;   /* 37F0:0060: palette */
	case 0x16: break;   /* 347C:0202 (OVL06): palette (alive, time left) */
	case 0: lever5_leave(); break;        /* 37F0:0012 (OVL11): graphics freed, sound 0x21 stopped, palette */
	case 0x20: break;                     /* 37F0:06E0 (OVL13): palette */
	case 0x21: break;   /* 37F0:0574: palette */
	default: break;
	}
}
/* 0CD6:073A */
static void room_unload(void) { if (room_bg == 0) return; room_leave_hook(room_bg - 1); room_bg = 0; }
void room_unload_pub(void) { room_unload(); }   /* (shell: the options menu, 0D5E:0696) */
/* 0CD6:02BE (switch_room 0823:0EAF, the flip redraw 169B:0A79): load the drawn room's description */
void room_load(uint8_t room)
{
	if (!byte_5ce7) {
		if (room_bg != 0) {
			room_unload();
			if (level_number == 5 && (drawn_room == 7 || drawn_room == 12)) bridge_room_enter();   /* 2A31:0DC1 -> 37F0:0426 */
		}
		return;
	}
	int16_t bg = room_description_bg(room);
	if (bg < 0) return;
	if (room_bg != bg + 1) room_unload();
	room_bg = bg + 1;
	room_enter_hook(bg);
}
