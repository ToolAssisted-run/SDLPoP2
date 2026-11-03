/* The character a key types, as the DOS BIOS gives it with a US keyboard: the game's key handlers and its cheats read
 * it ('+' is Shift and '=', or the keypad's +) */
#pragma once
#include <SDL.h>
int key_ascii(SDL_Keycode k, Uint16 mod);   /* 0: none (Alt or Ctrl held, or a key that types nothing) */
