/* SDLPoP2's SDL2 frontend: a window, the keyboard and game controllers (controller.c), the game's tick timing, the
 * renderer's screen and palette.
 * The program itself (title, menus, scenes, levels) is source/shell.c, stepped one VGA frame (70.086 Hz) at a time.
 * Settings: SDLPoP2.ini (source/settings.h); replays: source/replay.h.
 * usage: sdlpop2 [--ini PATH] [--record NAME | --replay NAME] GAME_DIR [DOS command line words] */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform.h"
#include <time.h>
#include "../source/core.h"
#include "../source/render.h"
#include "../source/audio.h"
#include "../source/shell.h"
#include "../source/settings.h"
#include "../source/replay.h"
#include "../source/loader.h"
#include "../source/text.h"
#include "../source/globals.h"
#include "controller.h"
#include "overlay_menu.h"
#ifndef SDLPOP2_DATADIR
#define SDLPOP2_DATADIR ""
#endif

static pop2_settings S;   /* SDLPoP2.ini (the defaults when there is none) */

/* ---- sound ---- */
extern void (*sound_start_hook)(int), (*sound_stop_hook)(int);   /* source/sound.c: where the game calls the driver */
static SDL_AudioDeviceID adev;
static void audio_cb(void *u, Uint8 *out, int len) { (void)u; audio_render((int16_t *)out, len / 2, 44100); }
static int audio_allowed(uint16_t id)   /* enable_music / enable_sounds, by the resource's kind (0 speaker, 1 digitized, 2 MIDI) */
{
	uint32_t n; const uint8_t *r = audio_resource(id, &n);
	if (!r || !n) return 1;
	return (r[0] & 0x7F) == 2 ? S.enable_music : S.enable_sounds;
}
static void on_start(int n) { uint16_t id = (uint16_t)(10000 + n); if (audio_allowed(id)) audio_request(id); }   /* (called from shell_step, the audio device locked) */
static void on_stop(int n) { audio_stop(n == -10000 ? 0 : (uint16_t)(10000 + n)); }
static void open_audio(const char *dir)
{
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) { fprintf(stderr, "sdlpop2: no audio (%s): playing without sound\n", SDL_GetError()); return; }
	if (!audio_init(dir, S.sound_device)) { fprintf(stderr, "sdlpop2: the sound files could not be loaded: playing without sound\n"); return; }
	char p[512];
	snprintf(p, sizeof p, "%s/NISDIGI.DAT", dir); audio_add_file(p);
	snprintf(p, sizeof p, "%s/NISMIDI.DAT", dir); audio_add_file(p);
	audio_volume(S.volume);   /* (audio_init leaves it at 15) */
	SDL_AudioSpec want = {0}, have; want.freq = 44100; want.format = AUDIO_S16SYS; want.channels = 1; want.samples = 1024; want.callback = audio_cb;
	adev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
	if (!adev) { fprintf(stderr, "sdlpop2: no audio device (%s): playing without sound\n", SDL_GetError()); return; }
	sound_start_hook = on_start; sound_stop_hook = on_stop;
	SDL_PauseAudioDevice(adev, 0);
}
static int game_volume = 15;   /* the game's own: 15 sound on, 0 off */
static void platform_sound_volume(int v) { game_volume = v; audio_volume(v >= 15 ? S.volume : v * S.volume / 15); }   /* (194C:3380: the game's 15 = on, 0 = off; Alt+S) */

/* ---- the picture, with the frontend's overlay (the info screen, messages) drawn with the shell's text library into
 * a port of its own (1 text, 2 a dark box): the game's screen and palette are not touched ---- */
static SDL_Window *win; static SDL_Renderer *ren; static SDL_Texture *tex, *tex2x;
static gport *overlay; static int overlay_used, info_shown, msg_frames; static char msg[64];
static void overlay_text(int v, int h, const char *s) { gfx_move_to(v, h); gfx_draw_string(s); }
static void overlay_draw(void)
{
	overlay_used = info_shown || msg_frames > 0;
	if (!overlay_used) return;
	if (!overlay) overlay = port_new(&rect_screen);
	gport *keep = the_port; the_port = overlay;
	gfx_text_font(0); overlay->bg = 0; gfx_erase_rect(&rect_screen);
	if (info_shown) {
		static const char *const lines[] = {
			"SDLPoP2: the keys (F1 or Esc: back to the game)", "",
			"Arrows, Home PgUp End PgDn: move",
			"Shift: action   Ctrl: draw the sword / cast",
			"Esc: the menu (pause)   Space: the time left",
			"Alt+A: restart the level   Alt+R: to the title",
			"Alt+G: save the game   Alt+L: restore a game",
			"Alt+O: options   Alt+H: hall of fame",
			"Alt+S: sound on/off   Alt+M: ambient music",
			"Alt+N: the next level (up to level 3)",
			"Ctrl+Q, Alt+Q: quit", "",
			"F6: quicksave   F9: quickload",
			"Alt+Enter: fullscreen   (SDLPoP2.ini: settings)",
			"Pad (default): D-pad / stick, X Shift, B Ctrl,",
			"Y up, A down, Start menu, Back Alt+A, LB/RB F6/F9",
		};
		int n = (int)(sizeof lines / sizeof lines[0]) - (controller_count() ? 0 : 2);   /* (the controller's lines when one is connected) */
		qrect box = { 14, 10, (int16_t)(14 + 14 + 11 * n > 199 ? 199 : 14 + 14 + 11 * n), 310 };
		gfx_fill_rect(2, &box);
		overlay->fg = 1;
		for (int i = 0; i < n; i++) overlay_text(box.top + 16 + 11 * i, box.left + 8, lines[i]);
	}
	if (msg_frames > 0) {
		int w = gfx_text_width(msg, (int)strlen(msg));
		qrect box = { 2, (int16_t)(156 - w / 2), 15, (int16_t)(164 + w / 2) };
		gfx_fill_rect(2, &box); overlay->fg = 1; overlay_text(12, 160 - w / 2, msg);
		msg_frames--;
	}
	the_port = keep;
}
static void message(const char *s) { snprintf(msg, sizeof msg, "%s", s); msg_frames = 140; fprintf(stderr, "sdlpop2: %s\n", s); }
static void present(void)
{
	static uint32_t argb[SCREEN_W * SCREEN_H];
	uint32_t lut[256];
	for (int i = 0; i < 256; i++) {
		uint8_t r = render_palette[3 * i], g = render_palette[3 * i + 1], b = render_palette[3 * i + 2];
		lut[i] = 0xFF000000u | (uint32_t)((r << 2 | r >> 4) << 16 | (g << 2 | g >> 4) << 8 | (b << 2 | b >> 4));
	}
	overlay_draw();
	for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
		uint32_t c = lut[screen_buf[i]];
		if (overlay_used && overlay->bits[i]) c = overlay->bits[i] == 1 ? 0xFFFFFFFFu : 0xFF000000u | ((c >> 2) & 0x3F3F3Fu);
		argb[i] = c;
	}
	overlay_menu_compose(argb);   /* (the in-game menu, when it shows) */
	SDL_UpdateTexture(tex, NULL, argb, SCREEN_W * 4);
	SDL_RenderClear(ren);
	if (tex2x) {   /* fuzzy: nearest-neighbour to twice the size, then smooth to the window */
		SDL_SetRenderTarget(ren, tex2x); SDL_RenderCopy(ren, tex, NULL, NULL);
		SDL_SetRenderTarget(ren, NULL); SDL_RenderCopy(ren, tex2x, NULL, NULL);
	} else SDL_RenderCopy(ren, tex, NULL, NULL);
	SDL_RenderPresent(ren);
}

/* the logical size (4:3 or square pixels), integer scaling and the scaling method's textures (at the start, and when the
 * overlay menu changes them) */
static int setup_video(void)
{
	int lw = SCREEN_W, lh = S.use_correct_aspect_ratio ? SCREEN_H * 6 / 5 : SCREEN_H;
	SDL_RenderSetLogicalSize(ren, lw, lh);
#if SDL_VERSION_ATLEAST(2, 0, 5)
	SDL_RenderSetIntegerScale(ren, S.use_integer_scaling ? SDL_TRUE : SDL_FALSE);
#endif
	if (tex) SDL_DestroyTexture(tex);
	if (tex2x) SDL_DestroyTexture(tex2x);
	tex = tex2x = NULL;
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, S.scaling_type == SCALING_BLURRY ? "linear" : "nearest");
	tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_W, SCREEN_H);
	if (!tex) { fprintf(stderr, "sdlpop2: cannot create the screen texture: %s\n", SDL_GetError()); return 0; }
	if (S.scaling_type == SCALING_FUZZY) {
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
		tex2x = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, SCREEN_W * 2, SCREEN_H * 2);
		if (!tex2x) fprintf(stderr, "sdlpop2: no render target (%s): sharp scaling\n", SDL_GetError());
	}
	return 1;
}

/* ---- keys: SDL scancodes to the PC scan codes the game reads (key_* remapped) ---- */
static uint8_t keymap[SDL_NUM_SCANCODES], keyaction[SDL_NUM_SCANCODES];
static void build_keymap(void)
{
	memset(keyaction, 0, sizeof keyaction);
	for (int i = 0; i < SDL_NUM_SCANCODES; i++) keymap[i] = (uint8_t)shell_pc_scancode(i);
	for (int k = 0; k < KEY_COUNT; k++) {
		SDL_Scancode sc = SDL_GetScancodeFromName(S.keys[k]);
		if (sc == SDL_SCANCODE_UNKNOWN) { fprintf(stderr, "sdlpop2: %s: unknown key name '%s'\n", settings_key_ini_names[k], S.keys[k]); continue; }
		keymap[sc] = settings_key_pc_scan[k]; keyaction[sc] = 1;   /* (typed as that key: no character) */
	}
}
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

/* ---- files ---- */
static int file_exists(const char *p) { return plat_file_exists(p); }
static void warn_ini(const char *m) { fprintf(stderr, "sdlpop2: %s\n", m); }
/* SDLPoP2.ini: --ini PATH, else the current directory, next to the binary, the installed copy. -1: --ini unreadable */
static int load_ini(const char *given, char *used, size_t n)
{
	if (given) { snprintf(used, n, "%s", given); return settings_load(&S, given, warn_ini) ? 1 : -1; }
	static char next[1024], inst[1024]; const char *cands[3] = { "SDLPoP2.ini", NULL, NULL };
	char *base = SDL_GetBasePath();
	if (base) { snprintf(next, sizeof next, "%sSDLPoP2.ini", base); cands[1] = next; SDL_free(base); }
	if (SDLPOP2_DATADIR[0]) { snprintf(inst, sizeof inst, "%s/SDLPoP2.ini", SDLPOP2_DATADIR); cands[2] = inst; }
	for (int i = 0; i < 3; i++) if (file_exists(cands[i])) { snprintf(used, n, "%s", cands[i]); return settings_load(&S, cands[i], warn_ini); }
	return 0;
}
/* a replay NAME without a directory is in replays_folder; ".p2r" is added when it has no extension */
static void replay_path(char *out, size_t n, const char *name, int reading)
{
	int plain = !strchr(name, '/') && !strchr(name, '\\'), ext = strchr(name, '.') != NULL;
	if (reading && file_exists(name)) { snprintf(out, n, "%s", name); return; }
	if (plain) { if (!reading) plat_mkdir(S.replays_folder); snprintf(out, n, "%s/%s%s", S.replays_folder, name, ext ? "" : ".p2r"); }
	else snprintf(out, n, "%s%s", name, ext ? "" : ".p2r");
}
/* SDLPoP2.cfg, the overlay menu's settings: next to the ini used (the current directory when none or the installed one) */
static void cfg_location(char *out, size_t n, const char *ini_used)
{
	const char *slash = strrchr(ini_used, '/'), *bslash = strrchr(ini_used, '\\');
	if (bslash && (!slash || bslash > slash)) slash = bslash;
	int installed = SDLPOP2_DATADIR[0] && !strncmp(ini_used, SDLPOP2_DATADIR, strlen(SDLPOP2_DATADIR));
	if (!ini_used[0] || installed || !slash) snprintf(out, n, "SDLPoP2.cfg");
	else snprintf(out, n, "%.*sSDLPoP2.cfg", (int)(slash - ini_used + 1), ini_used);
}
/* the overlay menu changed settings: apply them (OVERLAY_MENU_APPLY_*) */
static int menu_music, menu_sounds, menu_controller, menu_cheats = -1;   /* (menu_cheats: "Enable cheats" changed, -1 not) */
static void menu_apply(int what)
{
	if (what & OVERLAY_MENU_APPLY_FULLSCREEN) SDL_SetWindowFullscreen(win, S.start_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
	if (what & OVERLAY_MENU_APPLY_VIDEO) setup_video();
	if ((what & OVERLAY_MENU_APPLY_AUDIO) && adev) {
		SDL_LockAudioDevice(adev);
		platform_sound_volume(game_volume);
		if ((menu_music && !S.enable_music) || (menu_sounds && !S.enable_sounds)) audio_stop(0);   /* (turned off: what plays stops) */
		SDL_UnlockAudioDevice(adev);
	}
	menu_music = S.enable_music; menu_sounds = S.enable_sounds;
	if (what & OVERLAY_MENU_APPLY_KEYS) build_keymap();
	if (what & OVERLAY_MENU_APPLY_CHEATS) menu_cheats = overlay_menu_cheats();   /* (the game's, at the next step: a replay records it) */
	if (what & OVERLAY_MENU_APPLY_CONTROLLER) {
		if (menu_controller != S.enable_controller) { controller_quit(); controller_init(&S, 0); menu_controller = S.enable_controller; }
		controller_settings(&S);
		controller_set_pause_menu(S.enable_pause_menu);
	}
}
/* a key typed with Alt held (the overlay menu's RESTART LEVEL / RESTART GAME: PoP2's Alt+A / Alt+R) */
static void type_alt_key(shell_input *in, int scan, int ascii)
{
	int alt = in->down[0x38];
	shell_input_key(in, 0x38, 1, 0); shell_input_key(in, scan, 1, ascii); shell_input_key(in, scan, 0, 0);
	if (!alt) shell_input_key(in, 0x38, 0, 0);
}

static void usage(const char *prog)
{
	fprintf(stderr, "usage: %s [--ini PATH] [--record NAME | --replay NAME] GAME_DIR [DOS COMMAND LINE WORDS, e.g. yippeeyahoo LEVEL3]\n", prog);
}

int main(int argc, char **argv)
{
	const char *ini = NULL, *rec_name = NULL, *play_name = NULL; int a = 1;
	for (; a < argc && !strncmp(argv[a], "--", 2); a++) {
		if (!strcmp(argv[a], "--ini") && a + 1 < argc) ini = argv[++a];
		else if (!strcmp(argv[a], "--record") && a + 1 < argc) rec_name = argv[++a];
		else if (!strcmp(argv[a], "--replay") && a + 1 < argc) play_name = argv[++a];
		else { usage(argv[0]); return 2; }
	}
	if (a >= argc || (rec_name && play_name)) { usage(argv[0]); return 2; }
	const char *dir = argv[a]; int nwords = argc - a - 1; const char **words = (const char **)argv + a + 1;
	settings_defaults(&S);
	char ini_used[1024] = "";
	int ini_ok = load_ini(ini, ini_used, sizeof ini_used);
	if (ini_ok < 0) { fprintf(stderr, "sdlpop2: cannot read %s\n", ini_used); return 2; }
	if (ini_ok) { pop2_settings_game = &S; fprintf(stderr, "sdlpop2: settings from %s\n", ini_used); }
	char cfg_path[1024]; cfg_location(cfg_path, sizeof cfg_path, ini_ok ? ini_used : "");
	if (overlay_menu_load_cfg(&S, cfg_path, ini_ok ? ini_used : NULL, warn_ini)) { pop2_settings_game = &S; fprintf(stderr, "sdlpop2: the in-game menu's settings from %s\n", cfg_path); }
	if ((rec_name || play_name) && !S.enable_replay) { fprintf(stderr, "sdlpop2: replays are off (SDLPoP2.ini enable_replay)\n"); return 2; }

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) { fprintf(stderr, "sdlpop2: SDL: %s\n", SDL_GetError()); return 1; }
	/* with no display SDL falls back to its invisible "offscreen" driver: the game would run unseen */
	const char *vd = SDL_GetCurrentVideoDriver();
	if (vd && (!strcmp(vd, "offscreen") || !strcmp(vd, "dummy")) && !getenv("SDL_VIDEODRIVER")) {
		fprintf(stderr, "sdlpop2: no display to open a window on (DISPLAY and WAYLAND_DISPLAY are not set).\n"
		                "Run it from a desktop session, or over ssh with X forwarding (ssh -X / -Y), or set\n"
		                "SDL_VIDEODRIVER=offscreen to run headless on purpose (e.g. with SDLPOP2_SHOT).\n");
		SDL_Quit(); return 1;
	}

	/* replays: a recording starts with the program; a replay brings its seed, words, gameplay settings and game files */
	static replay_play play; static replay_rec rec; char path[1024], files_tmp[512] = "";
	uint32_t seed = S.random_seed_clock ? (uint32_t)time(NULL) : S.random_seed;   /* (DOS: time() at start) */
	if (play_name) {
		char err[512]; replay_path(path, sizeof path, play_name, 1);
		if (!replay_open(&play, path, err, sizeof err)) { fprintf(stderr, "sdlpop2: %s\n", err); SDL_Quit(); return 1; }
		if (nwords) fprintf(stderr, "sdlpop2: replaying %s: its own command line words are used\n", path);
		settings_copy_gameplay(&S, &play.settings); pop2_settings_game = &S;
		seed = play.seed; nwords = play.argc; words = play.argp;
		if (!plat_temp_dir(files_tmp, sizeof files_tmp, "sdlpop2-replay")) {   /* (the player's own saved games are not touched) */ fprintf(stderr, "sdlpop2: no scratch directory for the replay's files\n"); SDL_Quit(); return 1; }
		replay_write_files(&play, files_tmp); snprintf(file_dir, sizeof file_dir, "%s", files_tmp);
		fprintf(stderr, "sdlpop2: replaying %s\n", path);
	}
	platform_sound_volume_hook = platform_sound_volume;   /* (Alt+S / the game's volume -> the audio module) */
	shell_set_seed(seed);
	if (!shell_init(dir, nwords, words)) { fprintf(stderr, "cannot load the game from %s\n", dir); SDL_Quit(); return 1; }
	if (rec_name) {
		replay_path(path, sizeof path, rec_name, 0);
		if (!replay_record_start(&rec, path, seed, nwords, words, &S)) { fprintf(stderr, "sdlpop2: cannot write %s\n", path); SDL_Quit(); return 1; }
		fprintf(stderr, "sdlpop2: recording to %s\n", path);
	}

	/* the window: 4:3 (320 x 200 drawn 1.2 times taller, as mode 13h on a monitor) or square pixels (setup_video) */
	int lw = SCREEN_W, lh = S.use_correct_aspect_ratio ? SCREEN_H * 6 / 5 : SCREEN_H;
	int ww = S.window_width ? S.window_width : lw * 3, wh = S.window_height ? S.window_height : lh * 3;
	win = SDL_CreateWindow("SDLPoP2", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, ww, wh,
	                       SDL_WINDOW_RESIZABLE | (S.start_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
	if (!win) { fprintf(stderr, "sdlpop2: cannot open a window: %s\n", SDL_GetError()); SDL_Quit(); return 1; }
	ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_TARGETTEXTURE);   /* (fuzzy scaling's; the overlay menu can change the method) */
	if (!ren) ren = SDL_CreateRenderer(win, -1, 0);
	if (!ren) { fprintf(stderr, "sdlpop2: cannot create a renderer: %s\n", SDL_GetError()); SDL_Quit(); return 1; }
	if (!setup_video()) { SDL_Quit(); return 1; }
	build_keymap();
	open_audio(dir);
	controller_init(&S, 0);
	controller_set_pause_menu(S.enable_pause_menu);
	{   /* SDLPoP's in-game menu (sdl/overlay_menu.c) */
		overlay_menu_host host = { &S, win, ren, menu_apply, "" };
		snprintf(host.cfg_path, sizeof host.cfg_path, "%s", cfg_path);
		overlay_menu_init(&host);
		menu_music = S.enable_music; menu_sounds = S.enable_sounds; menu_controller = S.enable_controller;
	}

	static shell_input in; int replaying = play_name != NULL, action = REPLAY_NONE;
	Uint64 next = SDL_GetPerformanceCounter(), hz = SDL_GetPerformanceFrequency();
	for (;;) {
		SDL_Event e;
		int was_open = overlay_menu_is_open();
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT) goto out;
			if (overlay_menu_is_open()) {   /* the in-game menu: the keys and the mouse are its own */
				if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
					SDL_Scancode sc = e.key.keysym.scancode; int down = e.type == SDL_KEYDOWN;
					if (sc == SDL_SCANCODE_RETURN && (e.key.keysym.mod & KMOD_ALT)) {
						if (down && !e.key.repeat) SDL_SetWindowFullscreen(win, (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
						continue;
					}
					if (!replaying) { int n = in.ntyped; shell_input_key(&in, keymap[sc], down, 0); in.ntyped = n; }   /* (the keys held stay the keyboard's; nothing is typed) */
				}
				if (!overlay_menu_event(&e)) controller_event(&e);
				continue;
			}
			if (controller_event(&e)) continue;
			int can_open = shell_mode() == SH_PLAY && !info_shown;   /* (the menu: while playing, as SDLPoP's) */
			if (e.type == SDL_MOUSEBUTTONDOWN) { if (overlay_menu_open_event(&e, can_open) == 1) overlay_menu_open(rec.f != NULL, replaying); continue; }
			if (e.type != SDL_KEYDOWN && e.type != SDL_KEYUP) continue;
			SDL_Scancode sc = e.key.keysym.scancode; int down = e.type == SDL_KEYDOWN;
			/* the frontend's own keys: never passed to the game, nor recorded */
			if (sc == SDL_SCANCODE_RETURN && (e.key.keysym.mod & KMOD_ALT)) {
				if (down && !e.key.repeat) SDL_SetWindowFullscreen(win, (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
				continue;
			}
			if (sc == SDL_SCANCODE_F1 && S.enable_info_screen) { if (down && !e.key.repeat) info_shown = !info_shown; continue; }
			if (info_shown && sc == SDL_SCANCODE_ESCAPE) { if (down) info_shown = 0; continue; }
			if ((sc == SDL_SCANCODE_F6 || sc == SDL_SCANCODE_F9) && S.enable_quicksave) {
				if (down && !e.key.repeat && !replaying) action = sc == SDL_SCANCODE_F6 ? REPLAY_QUICKSAVE : REPLAY_QUICKLOAD;
				continue;
			}
			{   /* Esc (enable_pause_menu) / Backspace: the in-game menu */
				int o = overlay_menu_open_event(&e, can_open);
				if (o == 1) { overlay_menu_open(rec.f != NULL, replaying); continue; }
				if (o == 2) continue;
			}
			if (replaying || info_shown) continue;   /* (a replay plays its own keys) */
			shell_input_key(&in, keymap[sc], down, down && !keyaction[sc] ? ascii_of(e.key.keysym.sym, e.key.keysym.mod) : 0);
		}
		if (overlay_menu_is_open())
			controller_frame(&in, shell_mode(), 0);   /* (the menu reads them: nothing to the game, the keys they held are released) */
		else {   /* the controllers: the keys they hold and type into the frame's input, and the frontend's actions */
			int c = controller_frame(&in, shell_mode(), !replaying && !info_shown), info_before = info_shown;
			if (info_shown && (c & (CONTROLLER_INFO | CONTROLLER_CLOSE))) info_shown = 0;
			else if ((c & CONTROLLER_INFO) && S.enable_info_screen) info_shown = 1;
			if ((c & (CONTROLLER_QUICKSAVE | CONTROLLER_QUICKLOAD)) && S.enable_quicksave && !replaying)
				action = (c & CONTROLLER_QUICKSAVE) ? REPLAY_QUICKSAVE : REPLAY_QUICKLOAD;
			if ((c & CONTROLLER_MENU) && shell_mode() == SH_PLAY && !info_before) overlay_menu_open(rec.f != NULL, replaying);   /* button_menu */
		}
		if (overlay_menu_is_open()) {   /* the in-game menu: the game does not step (its timers and its sound wait) */
			if (!was_open && adev) SDL_PauseAudioDevice(adev, 1);
			SDL_Scancode key = SDL_SCANCODE_UNKNOWN; uint16_t mod = 0;
			switch (overlay_menu_frame(&key, &mod)) {
			case OVERLAY_MENU_QUICKSAVE: action = REPLAY_QUICKSAVE; break;
			case OVERLAY_MENU_QUICKLOAD: action = REPLAY_QUICKLOAD; break;
			case OVERLAY_MENU_RESTART_LEVEL: type_alt_key(&in, 0x1E, 'a'); break;   /* Alt+A */
			case OVERLAY_MENU_RESTART_GAME: type_alt_key(&in, 0x13, 'r'); break;    /* Alt+R */
			case OVERLAY_MENU_QUIT: goto out;
			case OVERLAY_MENU_KEY:   /* a key with Alt or Ctrl: the game's (held as the keyboard holds it) */
				if (!replaying) shell_input_key(&in, keymap[key], 1, keyaction[key] ? 0 : ascii_of(SDL_GetKeyFromScancode(key), mod));
				break;
			case OVERLAY_MENU_CHEAT:   /* the CHEATS page: the cheat's key typed into the game */
				if (!replaying) shell_input_type(&in, overlay_menu_cheat_key());
				break;
			}
			if (!overlay_menu_is_open() && adev) SDL_PauseAudioDevice(adev, 0);
		}
		if (!info_shown && !overlay_menu_is_open()) {   /* (while the info screen or the menu shows, the game waits) */
			if (replaying && !replay_frame(&play, &in, &action)) {
				message(replay_verify(&play) ? "REPLAY VERIFIED" : "REPLAY DIFFERS FROM THE RECORDING");
				replay_close(&play); replaying = 0; memset(&in, 0, sizeof in); action = REPLAY_NONE;   /* (then the keyboard again) */
			}
			if (menu_cheats >= 0) {   /* the menu's "Enable cheats": the game's DS:10C2 now (recorded) */
				if (!replaying && menu_cheats != shell_cheats()) action |= menu_cheats ? REPLAY_CHEATS_ON : REPLAY_CHEATS_OFF;
				menu_cheats = -1;
			}
			if (rec.f) replay_record_frame(&rec, &in, action);
			if (action & REPLAY_CHEATS_OFF) shell_set_cheats(0);
			if (action & REPLAY_CHEATS_ON) shell_set_cheats(1);
			if (action & (REPLAY_CHEATS_OFF | REPLAY_CHEATS_ON)) message(shell_cheats() ? "CHEATS ON" : "CHEATS OFF");
			if (action & REPLAY_QUICKSAVE) shell_quicksave();
			if (action & REPLAY_QUICKLOAD) shell_quickload();
			if ((action & (REPLAY_QUICKSAVE | REPLAY_QUICKLOAD)) && shell_mode() != SH_PLAY && !replaying) message("QUICKSAVE AND QUICKLOAD: ONLY WHILE PLAYING");
			action = REPLAY_NONE;
			if (adev) SDL_LockAudioDevice(adev);
			int r = shell_step(&in);
			if (adev) SDL_UnlockAudioDevice(adev);
			in.ntyped = 0;
			int q = shell_quick_result();
			switch (q) { case 1: message("QUICKSAVE"); break; case 2: message("QUICKLOAD"); break; case -1: message("NO QUICKSAVE YET"); break; }
			controller_after_step(shell_mode(), pop2_level(), Kid.f12, q == 2);   /* (rumble: the hit points, Kid +0x12, went down) */
			if (r == SHELL_EXIT) {
				if (replaying) { int act; shell_input end; replay_frame(&play, &end, &act);
					fprintf(stderr, "sdlpop2: %s\n", replay_verify(&play) ? "REPLAY VERIFIED" : "REPLAY DIFFERS FROM THE RECORDING"); }
				break;
			}
		}
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
	overlay_menu_close();   /* (the menu's settings saved, as when it closes) */
	if (rec.f) { replay_record_end(&rec); fprintf(stderr, "sdlpop2: recording saved to %s\n", path); }
	if (files_tmp[0]) {
		static const char *const names[3] = { "PRINCE.OPT", "PRINCE.HOF", "PRINCE.SAV" }; char p[600];
		for (int i = 0; i < 3; i++) { snprintf(p, sizeof p, "%s/%s", files_tmp, names[i]); remove(p); }
		plat_rmdir(files_tmp);
	}
	if (shell_exit_message()) printf("%s\n", shell_exit_message());
	if (adev) SDL_CloseAudioDevice(adev);
	controller_quit();
	SDL_Quit();
	return shell_exit_code();
}
