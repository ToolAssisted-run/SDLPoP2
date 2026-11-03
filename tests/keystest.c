/* keystest: the character a key types (sdl/keys.c), as the DOS BIOS gave the game with a US keyboard. The original
 * game's cheats read them: '+' (one more minute) is Shift and =, or the keypad's +; it was = before (issue 3). */
#include <stdio.h>
#include "../sdl/keys.h"

static int failed;
static void expect(SDL_Keycode k, Uint16 mod, int want, const char *what)
{
	const int got = key_ascii(k, mod);
	printf("%s  %s: %d\n", got == want ? "PASS" : "FAIL", what, got);
	if (got != want) failed++;
}

int main(void)
{
	expect(SDLK_EQUALS, KMOD_LSHIFT, '+', "Shift and = types +");
	expect(SDLK_EQUALS, KMOD_NONE, '=', "= alone types =");
	expect(SDLK_KP_PLUS, KMOD_NONE, '+', "the keypad's + types +");
	expect(SDLK_KP_PLUS, KMOD_RSHIFT, '+', "the keypad's + with Shift types +");
	expect(SDLK_MINUS, KMOD_NONE, '-', "- types -");
	expect(SDLK_KP_MINUS, KMOD_NONE, '-', "the keypad's - types -");
	expect(SDLK_MINUS, KMOD_LSHIFT, '_', "Shift and - types _");
	expect(SDLK_1, KMOD_LSHIFT, '!', "Shift and 1 types !");
	expect(SDLK_BACKSLASH, KMOD_LSHIFT, '|', "Shift and \\ types |");
	expect(SDLK_k, KMOD_NONE, 'k', "k types k");
	expect(SDLK_k, KMOD_LSHIFT, 'K', "Shift and k types K");
	expect(SDLK_k, KMOD_CAPS, 'K', "k with Caps Lock types K");
	expect(SDLK_EQUALS, KMOD_LALT, 0, "Alt and = types nothing");
	expect(SDLK_RETURN, KMOD_NONE, 0x0D, "Enter types a carriage return");
	expect(SDLK_LEFT, KMOD_NONE, 0, "an arrow types nothing");
	printf("%s\n", failed ? "FAILED" : "ALL PASSED");
	return failed ? 1 : 0;
}
