/* The character a key types (keys.h). SDL's key codes are the unshifted characters of the keyboard's layout, so Shift
 * is applied here as a US keyboard's BIOS applies it: Shift and = is +, as DOS gave the game. */
#include <string.h>
#include "keys.h"

int key_ascii(SDL_Keycode k, Uint16 mod)
{
	if (k == SDLK_RETURN || k == SDLK_KP_ENTER) return 0x0D;
	if (k == SDLK_ESCAPE) return 0x1B;
	if (k == SDLK_TAB) return 0x09;
	if (k == SDLK_BACKSPACE) return 0x08;
	if (k == SDLK_SPACE) return 0x20;
	if (mod & (KMOD_ALT | KMOD_CTRL)) return 0;
	if (k >= SDLK_a && k <= SDLK_z) return (mod & (KMOD_SHIFT | KMOD_CAPS)) ? (int)(k - 32) : (int)k;
	/* the keypad's signs type their characters with or without Shift */
	if (k == SDLK_KP_PLUS) return '+';
	if (k == SDLK_KP_MINUS) return '-';
	if (k == SDLK_KP_MULTIPLY) return '*';
	if (k == SDLK_KP_DIVIDE) return '/';
	if (k > 0x20 && k < 0x7F && (mod & KMOD_SHIFT)) {
		static const char plain[] = "1234567890-=[]\\;',./`", shifted[] = "!@#$%^&*()_+{}|:\"<>?~";
		const char *at = strchr(plain, (int)k);
		if (at) return shifted[at - plain];
	}
	if (k >= 0x20 && k < 0x7F) return (int)k;
	return 0;
}
