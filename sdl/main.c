/* SDLPoP2's SDL2 frontend: a window, the keyboard, the game's tick timing, the renderer's screen and palette.
 * usage: sdlpop2 GAME_DIR [LEVEL] */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/core.h"
#include "../src/render.h"
#include "../src/audio.h"

extern uint16_t frame_delay;   /* DS:24DE: 60 Hz ticks per game tick (5, or 6 while the prince is ...) */

/* the renderer's frame (render_frame.c when present): weak defaults draw nothing */
__attribute__((weak)) void render_frame(void) {}
__attribute__((weak)) void render_redraw_all(void) {}
__attribute__((weak)) uint8_t render_palette[768];   /* 6-bit VGA DAC values */

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
/* the DOS game's movement keys: arrows and the keypad's 3x3 grid (Home/PgUp/End/PgDn diagonals) */
static void read_input(pop2_input *in)
{
	const Uint8 *k = SDL_GetKeyboardState(NULL);
	int x = 0, y = 0;
	if (k[SDL_SCANCODE_LEFT] || k[SDL_SCANCODE_KP_4] || k[SDL_SCANCODE_KP_7] || k[SDL_SCANCODE_KP_1] || k[SDL_SCANCODE_HOME] || k[SDL_SCANCODE_END]) x = -1;
	if (k[SDL_SCANCODE_RIGHT] || k[SDL_SCANCODE_KP_6] || k[SDL_SCANCODE_KP_9] || k[SDL_SCANCODE_KP_3] || k[SDL_SCANCODE_PAGEUP] || k[SDL_SCANCODE_PAGEDOWN]) x = x ? 0 : 1;
	if (k[SDL_SCANCODE_UP] || k[SDL_SCANCODE_KP_8] || k[SDL_SCANCODE_KP_7] || k[SDL_SCANCODE_KP_9] || k[SDL_SCANCODE_HOME] || k[SDL_SCANCODE_PAGEUP]) y = -1;
	if (k[SDL_SCANCODE_DOWN] || k[SDL_SCANCODE_KP_2] || k[SDL_SCANCODE_KP_5] || k[SDL_SCANCODE_KP_1] || k[SDL_SCANCODE_KP_3] || k[SDL_SCANCODE_END] || k[SDL_SCANCODE_PAGEDOWN]) y = y ? 0 : 1;
	in->x = (int8_t)x; in->y = (int8_t)y;
	in->shift = (k[SDL_SCANCODE_LCTRL] || k[SDL_SCANCODE_RCTRL]) ? 2 : (k[SDL_SCANCODE_LSHIFT] || k[SDL_SCANCODE_RSHIFT]) ? 1 : 0;
}
int main(int argc, char **argv)
{
	if (argc < 2) { fprintf(stderr, "usage: %s GAME_DIR [LEVEL]\n", argv[0]); return 2; }
	int level = argc > 2 ? atoi(argv[2]) : 1;
	if (!pop2_init(argv[1])) { fprintf(stderr, "cannot load the game from %s\n", argv[1]); return 1; }
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
	win = SDL_CreateWindow("SDLPoP2", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_W * 3, SCREEN_H * 3 * 6 / 5, SDL_WINDOW_RESIZABLE);
	ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_PRESENTVSYNC);
	SDL_RenderSetLogicalSize(ren, SCREEN_W * 4, SCREEN_H * 4 * 6 / 5);   /* (the 4:3 aspect of mode 13h) */
	tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_W, SCREEN_H);
	open_audio(argv[1]);
	if (adev) SDL_LockAudioDevice(adev);
	pop2_new_game(level, (uint32_t)SDL_GetTicks());
	if (adev) SDL_UnlockAudioDevice(adev);
	render_redraw_all();
	Uint64 next = SDL_GetPerformanceCounter(), hz = SDL_GetPerformanceFrequency();
	int keystroke = 0, quit = 0;
	while (!quit) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT) quit = 1;
			else if (e.type == SDL_KEYDOWN && !e.key.repeat) { if (e.key.keysym.sym == SDLK_ESCAPE && (e.key.keysym.mod & KMOD_CTRL)) quit = 1; keystroke = 1; }
		}
		pop2_input in; read_input(&in); in.keystroke = (uint8_t)keystroke; keystroke = 0;
		if (adev) SDL_LockAudioDevice(adev);
		int r = pop2_frame(&in);
		if (adev) SDL_UnlockAudioDevice(adev);
		if (r == POP2_QUIT) break;
		if (r != POP2_PLAYING) render_redraw_all(); else render_frame();
		present();
		/* one game tick = frame_delay ticks of the 60 Hz timer */
		next += hz * (frame_delay ? frame_delay : 5) / 60;
		Uint64 now = SDL_GetPerformanceCounter();
		if (next > now) SDL_Delay((Uint32)((next - now) * 1000 / hz)); else next = now;
	}
	if (adev) SDL_CloseAudioDevice(adev);
	SDL_Quit();
	return 0;
}
