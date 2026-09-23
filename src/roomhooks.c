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
static void hook_missing(const char *what, int16_t bg) { char m[24]; static const char hx[] = "0123456789ABCDEF"; int n = 0;
	while (what[n] && n < 16) { m[n] = what[n]; n++; } m[n++] = hx[(bg >> 4) & 15]; m[n++] = hx[bg & 15]; m[n] = 0; note_missing(m); }
/* entry 0 of the background's hooks */
static void room_enter_hook(int16_t bg)
{
	switch (bg) {
	case 6: kind1_level_init(); break;   /* 33FD:0380 (level 2's puzzle room) */
	case 0x22:                            /* 37F0:001C (OVL14, level 8 room 9): the sword room's music, once */
		if (word_2bb2 == 0) { if (Kid.curr_col >= 9 && word_2bb0 == 0) { sound_1611_01a8(0x98); word_2bb0 = 1; } else if (Kid.curr_col < 9) sound_1611_01a8(0xFD); }
		break;
	case 0: case 0x14: case 0x15: case 0x16: case 0x17: case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E:
	case 0x1F: case 0x20: case 0x21: hook_missing("ROOMHOOK_IN_", bg); break;
	default: break;   /* DS:02E2: none */
	}
}
/* entry 1 */
static void room_leave_hook(int16_t bg)
{
	switch (bg) {
	case 0x22: break;   /* 37F0:0060: palette */
	case 0: case 0x16: case 0x20: case 0x21: hook_missing("ROOMHOOK_OUT_", bg); break;
	default: break;
	}
}
/* 0CD6:073A */
static void room_unload(void) { if (room_bg == 0) return; room_leave_hook(room_bg - 1); room_bg = 0; }
/* 0CD6:02BE (switch_room 0823:0EAF, the flip redraw 169B:0A79): load the drawn room's description */
void room_load(uint8_t room)
{
	if (!byte_5ce7) {
		if (room_bg != 0) {
			room_unload();
			if (level_number == 5 && (drawn_room == 7 || drawn_room == 12)) hook_missing("ROOMHOOK_IN_", 0x21);   /* 2A31:0DC1 */
		}
		return;
	}
	int16_t bg = room_description_bg(room);
	if (bg < 0) return;
	if (room_bg != bg + 1) room_unload();
	room_bg = bg + 1;
	room_enter_hook(bg);
}
