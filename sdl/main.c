/* SDLPoP2's SDL2 frontend: a window, the keyboard, the game's tick timing, the renderer's screen and palette.
 * The program itself (title, menus, scenes, levels) is src/shell.c, stepped one VGA frame (70.086 Hz) at a time.
 * usage: sdlpop2 GAME_DIR [DOS command line words] */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/core.h"
#include "../src/render.h"
#include "../src/audio.h"
#include "../src/shell.h"
#include <time.h>



extern void (*sound_start_hook)(int), (*sound_stop_hook)(int);   /* src/sound.c: where the game calls the driver */
static SDL_AudioDeviceID adev;
static void audio_cb(void *u, Uint8 *out, int len) { (void)u; audio_render((int16_t *)out, len / 2, 44100); }
static void on_start(int n) { audio_request((uint16_t)(10000 + n)); }   /* (called from pop2_frame, the audio device locked) */
static void on_stop(int n) { audio_stop(n == -10000 ? 0 : (uint16_t)(10000 + n)); }
static void open_audio(const char *dir)
{
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0 || !audio_init(dir, 3)) return;
	char p[512];
	snprintf(p, sizeof p, "%s/NISDIGI.DAT", dir); audio_add_file(p);
	snprintf(p, sizeof p, "%s/NISMIDI.DAT", dir); audio_add_file(p);
	SDL_AudioSpec want = {0}, have; want.freq = 44100; want.format = AUDIO_S16SYS; want.channels = 1; want.samples = 1024; want.callback = audio_cb;
	adev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
	if (!adev) return;
	sound_start_hook = on_start; sound_stop_hook = on_stop;
	SDL_PauseAudioDevice(adev, 0);
}
static SDL_Window *win; static SDL_Renderer *ren; static SDL_Texture *tex;
static void present(void)
{
	static uint32_t argb[SCREEN_W * SCREEN_H];
	uint32_t lut[256];
	for (int i = 0; i < 256; i++) {
		uint8_t r = render_palette[3 * i], g = render_palette[3 * i + 1], b = render_palette[3 * i + 2];
		lut[i] = 0xFF000000u | (uint32_t)((r << 2 | r >> 4) << 16 | (g << 2 | g >> 4) << 8 | (b << 2 | b >> 4));
	}
	for (int i = 0; i < SCREEN_W * SCREEN_H; i++) argb[i] = lut[screen_buf[i]];
	SDL_UpdateTexture(tex, NULL, argb, SCREEN_W * 4);
	SDL_RenderClear(ren); SDL_RenderCopy(ren, tex, NULL, NULL); SDL_RenderPresent(ren);
}
void platform_sound_volume(int v) { audio_volume(v); }   /* (Alt+S, 194C:3380) */
static int ascii_of(SDL_Keycode k, Uint16 mod)
{
	if (k == SDLK_RETURN || k == SDLK_KP_ENTER) return 0x0D;
	if (k == SDLK_ESCAPE) return 0x1B;
	if (k == SDLK_TAB) return 0x09;
	if (k == SDLK_BACKSPACE) return 0x08;
	if (k == SDLK_SPACE) return 0x20;
	if (mod & (KMOD_ALT | KMOD_CTRL)) return 0;
	if (k >= SDLK_a && k <= SDLK_z) return (mod & (KMOD_SHIFT | KMOD_CAPS)) ? (int)(k - 32) : (int)k;
	if (k >= 0x20 && k < 0x7F) return (int)k;
	return 0;
}
int main(int argc, char **argv)
{
	if (argc < 2) { fprintf(stderr, "usage: %s GAME_DIR [DOS COMMAND LINE WORDS, e.g. yippeeyahoo LEVEL3]\n", argv[0]); return 2; }
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
	shell_set_seed((uint32_t)time(NULL));
	if (!shell_init(argv[1], argc - 2, (const char **)argv + 2)) { fprintf(stderr, "cannot load the game from %s\n", argv[1]); return 1; }
	win = SDL_CreateWindow("SDLPoP2", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_W * 3, SCREEN_H * 3 * 6 / 5, SDL_WINDOW_RESIZABLE);
	ren = SDL_CreateRenderer(win, -1, 0);
	SDL_RenderSetLogicalSize(ren, SCREEN_W * 4, SCREEN_H * 4 * 6 / 5);   /* (the 4:3 aspect of mode 13h) */
	tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_W, SCREEN_H);
	open_audio(argv[1]);
	static shell_input in;
	Uint64 next = SDL_GetPerformanceCounter(), hz = SDL_GetPerformanceFrequency();
	for (;;) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT) goto out;
			if ((e.type == SDL_KEYDOWN || e.type == SDL_KEYUP)) {
				int sc = shell_pc_scancode(e.key.keysym.scancode);
				shell_input_key(&in, sc, e.type == SDL_KEYDOWN, e.type == SDL_KEYDOWN ? ascii_of(e.key.keysym.sym, e.key.keysym.mod) : 0);
			}
		}
		SDL_Keymod m = SDL_GetModState();
		in.shift_flags = (uint8_t)(((m & KMOD_RSHIFT) ? 1 : 0) | ((m & KMOD_LSHIFT) ? 2 : 0) | ((m & KMOD_CTRL) ? 4 : 0) | ((m & KMOD_ALT) ? 8 : 0));
		if (adev) SDL_LockAudioDevice(adev);
		int r = shell_step(&in);
		if (adev) SDL_UnlockAudioDevice(adev);
		in.ntyped = 0;
		if (r == SHELL_EXIT) break;
		present();
		if (getenv("SDLPOP2_SHOT") && shell_frame_count() == (uint32_t)atoi(getenv("SDLPOP2_SHOT"))) {   /* (tests: the screen as a PPM) */
			FILE *f = fopen(getenv("SDLPOP2_SHOT_FILE") ? getenv("SDLPOP2_SHOT_FILE") : "shot.ppm", "wb");
			if (f) { fprintf(f, "P6 %d %d 255\n", SCREEN_W, SCREEN_H);
				for (int i = 0; i < SCREEN_W * SCREEN_H; i++) { const uint8_t *c = render_palette + 3 * screen_buf[i]; fputc(c[0] << 2, f); fputc(c[1] << 2, f); fputc(c[2] << 2, f); }
				fclose(f); }
		}
		next += (Uint64)((double)hz / 70.086);   /* one VGA frame */
		Uint64 now = SDL_GetPerformanceCounter();
		if (next > now) SDL_Delay((Uint32)((next - now) * 1000 / hz)); else next = now;
	}
out:
	if (shell_exit_message()) printf("%s\n", shell_exit_message());
	if (adev) SDL_CloseAudioDevice(adev);
	SDL_Quit();
	return shell_exit_code();
}
