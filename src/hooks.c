/* Level-kind hooks after a character moves (169B:0731 for the prince, 169B:090D for the others). */
#include "types.h"
#include "globals.h"

/* 33FD:01F4 (kind 3): flying rocks hitting Char (caverns.c) */
static void kind3_objects_hit(void) { rocks_hit_char(); }
/* 33FD:06E6 (kind 3): standing on tiles 0x17/0x18; not reconstructed yet (draws random numbers) */
static void kind3_platform(void)
{
	uint8_t t = get_tile(Char.curr_row, Char.curr_col, Char.room);
	int16_t d = col_x_left[Char.curr_col] - dx_weight() + 0x1A;
	if ((t == 0x17 || (t == 0x18 && d >= 0)) && (frame_flags & 0x40) && 63 * Char.curr_row + 0x29 < Char.y && Char.f24 != 5) note_missing("KIND3_06E6");
}
void level_kind_hooks(void)
{
	switch (level_kind) {
	case 2: trap_touch_check(); ovl_347c_e48(); if (word_5ce8 == 0) trap_catch_check(); ovl_33fd_b2(); if (word_5ce8 == 0) ovl_33fd_118(); break;
	case 3: trap_touch_check(); if (word_5ce8 == 0) { kind3_objects_hit(); trap_catch_check(); kind3_platform(); } break;
	case 4: ovl_33fd_b2(); if (word_5ce8 == 0) ovl_33fd_118(); break;
	}
}
void level_kind_hooks_char(void)
{
	switch (level_kind) {
	case 2: trap_touch_check(); ovl_347c_e48(); trap_catch_check(); ovl_33fd_b2(); ovl_33fd_118(); break;
	case 3: trap_touch_check(); kind3_objects_hit(); trap_catch_check(); kind3_platform(); break;
	case 4: ovl_33fd_b2(); ovl_33fd_118(); break;
	case 6: ovl_kind6_char(); break;
	}
}
