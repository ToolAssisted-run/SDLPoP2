/* Reading the controls (0823:10A0 read_input, 0823:1178 keyboard, 0823:110C joystick). */
#include "types.h"
#include "globals.h"

/* DS:1D00: key states by physical position (nonzero = held), filled by the keyboard interrupt handler. The movement
 * keys form three 3x3 grids: keypad / arrows / Home PgUp End PgDn (0x54..0x5E), W E R / S D F / X C V (0x1E..0x3C)
 * and U I O / J K L / M , . (0x23..0x41); centre and bottom-centre keys mean down. */
uint8_t key_table[0x70];
uint8_t bios_shift_flags;   /* 0040:0017: bit 0/1 shift, bit 2 ctrl, bit 3 alt */
int16_t joy_x, joy_y, joy_cx, joy_cy; uint8_t joy_button;   /* DS:1D06 / 1D04 / 1D0A / 1D08 / 1D0C (0823:164B reads them) */
uint16_t input_device;      /* DS:2BA2: 2 = joystick */

#define K(p) keys[(p) - 0x1D00]
/* 0823:1178 on a given key table and BIOS shift flags (the reader itself adds nothing else) */
void keyboard_controls(const uint8_t *keys, uint8_t flags, int8_t *x, int8_t *y, int8_t *shift)
{
	*x = *y = *shift = 0;
	if (flags & 8) return;   /* Alt held: nothing */
	if (K(0x1D55) || K(0x1D24) || K(0x1D1F) || K(0x1D54) || K(0x1D23) || K(0x1D1E) || K(0x1D56) || K(0x1D25) || K(0x1D20)) *y = -1;
	else if (K(0x1D5D) || K(0x1D32) || K(0x1D2D) || K(0x1D59) || K(0x1D40) || K(0x1D3B) || K(0x1D5C) || K(0x1D3F) || K(0x1D3A)
	         || K(0x1D5E) || K(0x1D41) || K(0x1D3C)) *y = 1;
	if (K(0x1D58) || K(0x1D31) || K(0x1D2C) || K(0x1D54) || K(0x1D23) || K(0x1D1E) || K(0x1D5C) || K(0x1D3F) || K(0x1D3A)) *x = -1;
	else if (K(0x1D5A) || K(0x1D33) || K(0x1D2E) || K(0x1D56) || K(0x1D25) || K(0x1D20) || K(0x1D5E) || K(0x1D41) || K(0x1D3C)) *x = 1;
	if ((flags & 3) || K(0x1D60)) *shift = -1;
	else if ((flags & 4) || K(0x1D5F)) *shift = -2;
}
static void read_keyboard(void)
{
	int8_t x, y, sh;
	keyboard_controls(key_table, bios_shift_flags, &x, &y, &sh);
	bios_shift_flags &= 0xF;
	if (bios_shift_flags & 8) return;
	control_x = x; control_y = y; control_shift = sh;
}
/* 0823:110C (after 0823:164B sampled the stick): beyond half the centre value in either direction */
static void read_joystick(void)
{
	int16_t h = joy_cx / 2, d = joy_x - joy_cx;
	if (d > h) control_x = 1; else if (-h > d) control_x = -1;
	h = joy_cy / 2; d = joy_y - joy_cy;
	if (d > h) control_y = 1; else if (-h > d) control_y = -1;
	control_shift = -(int8_t)joy_button; if ((int8_t)control_shift < -2) control_shift = -1;
}
/* 0823:10A0: clear next_room and the controls, read the device, then the hotkeys (0823:02BE, not reconstructed) */
__attribute__((weak)) int read_input(void)   /* weak: the snapshot tests feed captured controls */
{
	next_room = 0; control_x = control_y = control_shift = 0;
	if (input_device == 2) read_joystick(); else read_keyboard();
	int si = hotkeys_02be();
	return si ? si : (int8_t)control_shift;
}

uint16_t word_2baa;   /* DS:2BAA */
/* 0823:02BE, the part that matters for play: a key or the action button after the prince died (or during a demo)
 * asks for the level to restart (DS:5CD8). The hotkeys themselves (pause, sound, restart, save) are not here yet. */
int hotkeys_02be(void)
{
	int di = bios_key(), restart = 0;
	if (control_shift != 0 || di != 0) {
		if ((minutes_left != 0 && Kid.alive > 6) || word_2ba8) { restart = 1; if (word_2ba8) word_2baa = 1; }
	}
	if (restart && !word_5ce8 && !(drawn_room == 4 && level_number == 13)) word_5cd8 = 1;   /* (level 13 room 4: 2A31:0E11 decides) */
	return di;
}
