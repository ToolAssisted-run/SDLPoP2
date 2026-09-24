/* The in-game overlay menu (the 'menu' suite): sdl/overlay_menu.c (SDLPoP's menu) driven headlessly, with synthetic
 * keyboard and mouse events and SDL's virtual joystick (no hardware), on the whole program (the shell) at level 1.
 * The frame loop below is the SDL frontend's (sdl/main.c) for the menu: the events go to the menu while it shows, the
 * game is not stepped then, and the menu's actions become the frontend's (quicksave / quickload, Alt+A, quit).
 *  1. opening with Esc and closing (Esc, RESUME); while it shows the game does not advance (no frame, the same
 *     pop2_hash, the game's screen and palette untouched); the keys held go on being the keyboard's;
 *  2. navigation (up / down with wrap-around, the settings pages, a subsection, Page Down / Home / End, back);
 *  3. QUICKSAVE and QUICKLOAD through the menu (the state after the loading frame is the one after the saving frame);
 *     RESTART LEVEL (the prince back where the level started), and F6 in the menu;
 *  4. settings: a toggle and a number changed (applied at once through the frontend's hook), saved to SDLPoP2.cfg when
 *     the menu closes and read back (SDLPoP's rule: not when the ini is newer); a gameplay setting installs the
 *     settings for the game, and cannot be changed while a replay is recorded; "Restore defaults...";
 *  5. the mouse (a click on an item, the right button backs out), the level and skill pages, the key redefinition,
 *     the quit confirmation (Cancel, then OK: the frontend quits);
 *  6. a controller: button_menu opens it, the D-pad moves, A selects, B backs out;
 *  7. enable_pause_menu = false: Esc is the game's own pause, Backspace still opens the menu;
 *  8. screenshots (PPM) of the pages when OVERLAYMENU_SHOTS names a directory.
 * The copy protection is not involved (level 1).
 * usage: overlaymenutest GAME_DIR */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utime.h>
#include <sys/stat.h>
#include <SDL.h>
#include "../source/core.h"
#include "../source/shell.h"
#include "../source/settings.h"
#include "../source/replay.h"
#include "../source/loader.h"
#include "../source/render.h"
#include "../source/globals.h"
#include "../sdl/controller.h"
#include "../sdl/overlay_menu.h"

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL: " __VA_ARGS__); printf("\n"); } else if (getenv("VERBOSE")) { printf("ok: " __VA_ARGS__); printf("\n"); } } while (0)

static pop2_settings S;
static shell_input in;
static int action, quick_result, quitted, recording, replaying, applied;
static void apply(int what) { applied |= what; }

/* ---- the frontend's frame loop (sdl/main.c) ---- */
static int ascii(SDL_Scancode sc)
{
	if (sc == SDL_SCANCODE_RETURN) return 0x0D;
	if (sc == SDL_SCANCODE_ESCAPE) return 0x1B;
	if (sc == SDL_SCANCODE_BACKSPACE) return 0x08;
	if (sc == SDL_SCANCODE_SPACE) return 0x20;
	if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z) return 'a' + (sc - SDL_SCANCODE_A);
	return 0;
}
static void route(const SDL_Event *e)
{
	if (overlay_menu_is_open()) {
		if (e->type == SDL_KEYDOWN || e->type == SDL_KEYUP) { int n = in.ntyped; shell_input_key(&in, shell_pc_scancode(e->key.keysym.scancode), e->type == SDL_KEYDOWN, 0); in.ntyped = n; }
		overlay_menu_event(e);
		return;
	}
	int o = overlay_menu_open_event(e, shell_mode() == SH_PLAY);
	if (o == 1) { overlay_menu_open(recording, replaying); return; }
	if (o == 2 || (e->type != SDL_KEYDOWN && e->type != SDL_KEYUP)) return;
	int down = e->type == SDL_KEYDOWN;
	shell_input_key(&in, shell_pc_scancode(e->key.keysym.scancode), down, down ? ascii(e->key.keysym.scancode) : 0);
}
static void key(SDL_Scancode sc, int down, Uint16 mod)
{
	SDL_Event e; memset(&e, 0, sizeof e);
	e.type = down ? SDL_KEYDOWN : SDL_KEYUP; e.key.state = down ? SDL_PRESSED : SDL_RELEASED;
	e.key.keysym.scancode = sc; e.key.keysym.sym = SDL_GetKeyFromScancode(sc); e.key.keysym.mod = mod;
	route(&e);
}
static void mouse(int type, int x, int y, int button)
{
	SDL_Event e; memset(&e, 0, sizeof e); e.type = (Uint32)type;
	if (type == SDL_MOUSEMOTION) { e.motion.x = x; e.motion.y = y; }
	else { e.button.x = x; e.button.y = y; e.button.button = (Uint8)button; }
	route(&e);
}
static void type_alt(int scan, int c)
{
	int alt = in.down[0x38];
	shell_input_key(&in, 0x38, 1, 0); shell_input_key(&in, scan, 1, c); shell_input_key(&in, scan, 0, 0);
	if (!alt) shell_input_key(&in, 0x38, 0, 0);
}
static void frame(void)
{
	SDL_Event e; SDL_PumpEvents();
	while (SDL_PollEvent(&e)) controller_event(&e);
	if (overlay_menu_is_open()) controller_frame(&in, shell_mode(), 0);
	else if (controller_frame(&in, shell_mode(), 1) & CONTROLLER_MENU) overlay_menu_open(recording, replaying);
	if (overlay_menu_is_open()) {
		SDL_Scancode k = SDL_SCANCODE_UNKNOWN; uint16_t mod = 0;
		switch (overlay_menu_frame(&k, &mod)) {
		case OVERLAY_MENU_QUICKSAVE: action = REPLAY_QUICKSAVE; break;
		case OVERLAY_MENU_QUICKLOAD: action = REPLAY_QUICKLOAD; break;
		case OVERLAY_MENU_RESTART_LEVEL: type_alt(0x1E, 'a'); break;
		case OVERLAY_MENU_RESTART_GAME: type_alt(0x13, 'r'); break;
		case OVERLAY_MENU_QUIT: quitted = 1; break;
		case OVERLAY_MENU_KEY: shell_input_key(&in, shell_pc_scancode(k), 1, ascii(k)); break;
		}
		if (overlay_menu_is_open()) return;
	}
	if (action == REPLAY_QUICKSAVE) shell_quicksave();
	if (action == REPLAY_QUICKLOAD) shell_quickload();
	action = REPLAY_NONE;
	shell_step(&in);
	in.ntyped = 0;
	int q = shell_quick_result(); if (q) quick_result = q;
}
static void frames(int n) { for (int i = 0; i < n; i++) frame(); }
static void until_quick(void) { for (int i = 0; i < 20 && !quick_result; i++) frame(); }   /* (done at the next game tick) */
static void press(SDL_Scancode sc) { key(sc, 1, 0); frame(); key(sc, 0, 0); frame(); }

/* the menu's state */
static int page, dialog; static const char *item, *subsection, *setting;
static void state(void) { overlay_menu_state(&page, &item, &subsection, &setting, &dialog); }
static int at_item(const char *t) { state(); return !strcmp(item, t); }
static int at_setting(const char *t) { state(); return !strcmp(setting, t); }

/* ---- the picture: the game's screen with the menu over it, as the frontend shows it ---- */
static const char *shots;
static void screenshot(const char *name)
{
	static uint32_t argb[SCREEN_W * SCREEN_H];
	for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
		const uint8_t *c = render_palette + 3 * screen_buf[i];
		argb[i] = 0xFF000000u | (uint32_t)((c[0] << 2 | c[0] >> 4) << 16 | (c[1] << 2 | c[1] >> 4) << 8 | (c[2] << 2 | c[2] >> 4));
	}
	overlay_menu_compose(argb);
	if (!shots) return;
	char p[1024]; snprintf(p, sizeof p, "%s/%s.ppm", shots, name);
	FILE *f = fopen(p, "wb"); if (!f) { printf("cannot write %s\n", p); return; }
	fprintf(f, "P6 %d %d 255\n", SCREEN_W, SCREEN_H);
	for (int i = 0; i < SCREEN_W * SCREEN_H; i++) { fputc(argb[i] >> 16 & 0xFF, f); fputc(argb[i] >> 8 & 0xFF, f); fputc(argb[i] & 0xFF, f); }
	fclose(f);
}

/* ---- the virtual controller (as tests/controllertest.c) ---- */
static SDL_Joystick *vjoy; static int vindex = -1;
static int attach(void)
{
	SDL_VirtualJoystickDesc d; SDL_zero(d);
	d.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION; d.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
	d.naxes = SDL_CONTROLLER_AXIS_MAX; d.nbuttons = SDL_CONTROLLER_BUTTON_MAX; d.name = "SDLPoP2 test pad";
	vindex = SDL_JoystickAttachVirtualEx(&d);
	if (vindex < 0) return 0;
	vjoy = SDL_JoystickOpen(vindex);
	SDL_Event e; SDL_PumpEvents(); while (SDL_PollEvent(&e)) controller_event(&e);
	for (int a = 0; a < SDL_CONTROLLER_AXIS_MAX; a++) SDL_JoystickSetVirtualAxis(vjoy, a, a >= SDL_CONTROLLER_AXIS_TRIGGERLEFT ? -32768 : 0);
	return vjoy != NULL && controller_count() == 1;
}
static void button(int b) { SDL_JoystickSetVirtualButton(vjoy, b, 1); frame(); SDL_JoystickSetVirtualButton(vjoy, b, 0); frame(); }

static int file_newer(const char *path, int seconds)   /* the file's modification time moved by seconds */
{
	struct stat st; if (stat(path, &st)) return 0;
	struct utimbuf t = { st.st_atime, st.st_mtime + seconds };
	return utime(path, &t) == 0;
}

int main(int argc, char **argv)
{
	if (argc < 2) { fprintf(stderr, "usage: overlaymenutest GAME_DIR\n"); return 2; }
	shots = getenv("OVERLAYMENU_SHOTS");
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) { printf("SDL: %s (skipped)\n", SDL_GetError()); return 77; }
	char dir[256]; snprintf(dir, sizeof dir, "/tmp/sdlpop2-overlaymenutest-XXXXXX");
	if (!mkdtemp(dir)) { perror("mkdtemp"); return 2; }
	snprintf(file_dir, sizeof file_dir, "%s", dir);   /* (the game's PRINCE.OPT / HOF / SAV) */
	char cfg[512], ini[512]; snprintf(cfg, sizeof cfg, "%s/SDLPoP2.cfg", dir); snprintf(ini, sizeof ini, "%s/SDLPoP2.ini", dir);

	settings_defaults(&S);
	S.skip_title = 1; S.enable_quicksave_penalty = 0;
	pop2_settings_game = &S;
	shell_set_seed(0x5EED);
	if (!shell_init(argv[1], 0, NULL)) { printf("shell_init failed\n"); return 3; }
	overlay_menu_host host = { &S, NULL, NULL, apply, "" };
	snprintf(host.cfg_path, sizeof host.cfg_path, "%s", cfg);
	overlay_menu_init(&host);
	controller_init(&S, 1);
	controller_set_pause_menu(S.enable_pause_menu);

	for (int f = 0; f < 3000 && shell_mode() != SH_PLAY; f++) frame();
	frames(60);
	CHECK(shell_mode() == SH_PLAY && pop2_level() == 1, "the game is playing level 1 (frame %u)", shell_frame_count());
	int16_t start_x = Kid.x; uint8_t start_room = Kid.room;
	if (getenv("VERBOSE")) printf("level 1: the prince at x %d, room %d\n", start_x, start_room);

	/* 1. open, frozen, close */
	static uint8_t screen0[SCREEN_W * SCREEN_H], pal0[768];
	memcpy(screen0, screen_buf, sizeof screen0); memcpy(pal0, render_palette, sizeof pal0);
	uint32_t f0 = shell_frame_count(); uint64_t h0 = pop2_hash();
	screenshot("game");
	key(SDL_SCANCODE_ESCAPE, 1, 0); frame();
	CHECK(overlay_menu_is_open() && at_item("RESUME") && page == 0, "Esc opens the pause menu at RESUME");
	key(SDL_SCANCODE_ESCAPE, 1, 0); frame();   /* (a held key's repeat) */
	CHECK(overlay_menu_is_open(), "the held Esc's repeats do not close it");
	key(SDL_SCANCODE_ESCAPE, 0, 0);
	key(SDL_SCANCODE_RIGHT, 1, 0); frames(100);   /* (the right arrow held while the menu shows) */
	screenshot("pause");
	CHECK(shell_frame_count() == f0 && pop2_hash() == h0, "the game does not advance while the menu shows");
	CHECK(!memcmp(screen0, screen_buf, sizeof screen0) && !memcmp(pal0, render_palette, sizeof pal0), "the game's screen and palette are not touched");
	CHECK(in.down[0x4D] && in.ntyped == 0, "a key held in the menu is held for the game, not typed");
	key(SDL_SCANCODE_RIGHT, 0, 0);
	press(SDL_SCANCODE_ESCAPE);
	CHECK(!overlay_menu_is_open(), "Esc closes it");
	CHECK(shell_frame_count() > f0, "the game goes on");
	press(SDL_SCANCODE_BACKSPACE);
	CHECK(overlay_menu_is_open(), "Backspace opens it");
	press(SDL_SCANCODE_RETURN);
	CHECK(!overlay_menu_is_open(), "Enter on RESUME closes it");

	/* 2. navigation */
	press(SDL_SCANCODE_ESCAPE);
	press(SDL_SCANCODE_DOWN); CHECK(at_item("QUICKSAVE (F6)"), "down: QUICKSAVE (%s)", item);
	press(SDL_SCANCODE_DOWN); CHECK(at_item("QUICKLOAD (F9)"), "down: QUICKLOAD (%s)", item);
	press(SDL_SCANCODE_UP); press(SDL_SCANCODE_UP); press(SDL_SCANCODE_UP);
	CHECK(at_item("QUIT GAME"), "up from RESUME wraps to QUIT GAME (%s)", item);
	press(SDL_SCANCODE_UP); press(SDL_SCANCODE_UP);
	CHECK(at_item("SETTINGS"), "up: SETTINGS (%s)", item);
	press(SDL_SCANCODE_RETURN); state();
	CHECK(page == 1 && at_item("GENERAL"), "SETTINGS: the settings page at GENERAL (%d, %s)", page, item);
	screenshot("settings");
	press(SDL_SCANCODE_RETURN); state();
	CHECK(!strcmp(subsection, "GENERAL") && at_setting("Enable pause menu"), "GENERAL: its first setting (%s, %s)", subsection, setting);
	screenshot("settings_general");
	press(SDL_SCANCODE_END); CHECK(at_setting("Restore defaults..."), "End: the last setting (%s)", setting);
	screenshot("settings_general_end");
	press(SDL_SCANCODE_HOME); CHECK(at_setting("Enable pause menu"), "Home: the first (%s)", setting);
	press(SDL_SCANCODE_PAGEDOWN); CHECK(at_setting("Horizontal joystick movement only"), "Page Down: nine down (%s)", setting);
	press(SDL_SCANCODE_HOME);
	press(SDL_SCANCODE_ESCAPE); state();
	CHECK(page == 1 && !strcmp(subsection, "") && at_item("GENERAL"), "Esc: back to the categories");
	static const char *const pages[4] = { "GAMEPLAY", "VISUALS", "MODS", "CONTROLS" };
	for (int i = 0; i < 4; i++) {
		press(SDL_SCANCODE_DOWN); press(SDL_SCANCODE_RETURN); state();
		CHECK(!strcmp(subsection, pages[i]), "%s's page (%s)", pages[i], subsection);
		char name[64]; snprintf(name, sizeof name, "settings_%s", pages[i]); for (char *p = name; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
		screenshot(name);
		press(SDL_SCANCODE_ESCAPE);
	}
	press(SDL_SCANCODE_DOWN); CHECK(at_item("BACK"), "BACK (%s)", item);
	press(SDL_SCANCODE_RETURN); state();
	CHECK(page == 0 && at_item("SETTINGS"), "BACK: the pause menu at SETTINGS");
	press(SDL_SCANCODE_ESCAPE); CHECK(!overlay_menu_is_open(), "closed");

	/* 3. quicksave, quickload, restart */
	frames(5);
	press(SDL_SCANCODE_ESCAPE); press(SDL_SCANCODE_DOWN);
	quick_result = 0; key(SDL_SCANCODE_RETURN, 1, 0); frame();
	CHECK(!overlay_menu_is_open(), "QUICKSAVE closes the menu");
	until_quick();
	CHECK(quick_result == 1, "QUICKSAVE: saved (%d)", quick_result);
	uint64_t saved_hash = pop2_hash();
	key(SDL_SCANCODE_RETURN, 0, 0);
	key(SDL_SCANCODE_RIGHT, 1, 0); frames(120); key(SDL_SCANCODE_RIGHT, 0, 0); frames(40);
	CHECK(pop2_hash() != saved_hash, "the game moved on");
	press(SDL_SCANCODE_ESCAPE); press(SDL_SCANCODE_DOWN); press(SDL_SCANCODE_DOWN);
	quick_result = 0; key(SDL_SCANCODE_RETURN, 1, 0); frame(); until_quick();
	CHECK(!overlay_menu_is_open() && quick_result == 2 && pop2_hash() == saved_hash, "QUICKLOAD: the saved state again (%d)", quick_result);
	key(SDL_SCANCODE_RETURN, 0, 0); frames(2);
	press(SDL_SCANCODE_ESCAPE); quick_result = 0; key(SDL_SCANCODE_F6, 1, 0); frame(); key(SDL_SCANCODE_F6, 0, 0); frame(); until_quick();
	CHECK(!overlay_menu_is_open() && quick_result == 1, "F6 in the menu: quicksave, the menu closes");
	key(SDL_SCANCODE_RIGHT, 1, 0); frames(150); key(SDL_SCANCODE_RIGHT, 0, 0); frames(20);
	CHECK(Kid.x != start_x || Kid.room != start_room, "the prince has moved (x %d -> %d)", start_x, Kid.x);
	press(SDL_SCANCODE_ESCAPE); for (int i = 0; i < 3; i++) press(SDL_SCANCODE_DOWN);
	CHECK(at_item("RESTART LEVEL"), "RESTART LEVEL (%s)", item);
	press(SDL_SCANCODE_RETURN);
	CHECK(!overlay_menu_is_open(), "RESTART LEVEL closes the menu");
	frames(200);
	CHECK(shell_mode() == SH_PLAY && abs(Kid.x - start_x) <= 4 && Kid.room == start_room, "the level restarted: the prince back at the start (x %d, room %d)", Kid.x, Kid.room);

	/* 4. settings and SDLPoP2.cfg */
	press(SDL_SCANCODE_ESCAPE); for (int i = 0; i < 4; i++) press(SDL_SCANCODE_DOWN);
	press(SDL_SCANCODE_RETURN); press(SDL_SCANCODE_RETURN);   /* SETTINGS, GENERAL */
	applied = 0; press(SDL_SCANCODE_LEFT);
	CHECK(S.enable_pause_menu == 0 && (applied & OVERLAY_MENU_APPLY_CONTROLLER), "Left: 'Enable pause menu' off, applied");
	press(SDL_SCANCODE_RIGHT); CHECK(S.enable_pause_menu == 1, "Right: on again");
	for (int i = 0; i < 4; i++) press(SDL_SCANCODE_DOWN);
	CHECK(at_setting("Volume"), "Volume (%s)", setting);
	applied = 0; press(SDL_SCANCODE_LEFT); press(SDL_SCANCODE_LEFT); press(SDL_SCANCODE_LEFT);
	CHECK(S.volume == 12 && (applied & OVERLAY_MENU_APPLY_AUDIO), "Left x3: volume 12, applied (%d)", S.volume);
	screenshot("settings_general_volume");
	press(SDL_SCANCODE_RIGHT); press(SDL_SCANCODE_RIGHT); press(SDL_SCANCODE_RIGHT); press(SDL_SCANCODE_RIGHT);
	CHECK(S.volume == 15, "Right: not beyond 15 (%d)", S.volume);
	press(SDL_SCANCODE_LEFT); press(SDL_SCANCODE_LEFT); press(SDL_SCANCODE_LEFT); press(SDL_SCANCODE_LEFT);   /* 11 */
	press(SDL_SCANCODE_ESCAPE); press(SDL_SCANCODE_DOWN); press(SDL_SCANCODE_DOWN); press(SDL_SCANCODE_DOWN); press(SDL_SCANCODE_RETURN);   /* MODS */
	press(SDL_SCANCODE_DOWN); press(SDL_SCANCODE_DOWN); press(SDL_SCANCODE_DOWN); press(SDL_SCANCODE_DOWN);
	CHECK(at_setting("Starting hitpoints"), "MODS: Starting hitpoints (%s)", setting);
	pop2_settings_game = NULL;
	press(SDL_SCANCODE_RIGHT); press(SDL_SCANCODE_RIGHT);
	CHECK(S.start_hitp == 5 && pop2_settings_game == &S, "starting hitpoints 5: the settings installed for the game");
	press(SDL_SCANCODE_ESCAPE); press(SDL_SCANCODE_ESCAPE); press(SDL_SCANCODE_ESCAPE);
	CHECK(!overlay_menu_is_open() && access(cfg, F_OK) == 0, "closed: SDLPoP2.cfg written");
	{
		pop2_settings t; settings_defaults(&t);
		FILE *f = fopen(ini, "w"); if (f) { fputs("[General]\nvolume = 7\n", f); fclose(f); }
		file_newer(ini, -100);
		CHECK(overlay_menu_load_cfg(&t, cfg, ini, NULL) && t.volume == 11 && t.start_hitp == 5 && t.enable_pause_menu == 1, "SDLPoP2.cfg read back (volume %d, hit points %d)", t.volume, t.start_hitp);
		pop2_settings u; settings_defaults(&u); settings_load(&u, cfg, NULL);
		CHECK(settings_gameplay_equal(&u, &S) && u.volume == S.volume && !strcmp(u.keys[KEY_SHIFT], S.keys[KEY_SHIFT]), "SDLPoP2.cfg holds the menu's settings");
		file_newer(ini, 200);
		settings_defaults(&t);
		CHECK(!overlay_menu_load_cfg(&t, cfg, ini, NULL) && t.volume == 15, "not read when SDLPoP2.ini is newer (SDLPoP's rule)");
	}
	/* while a replay is recorded: the game's settings cannot change */
	recording = 1;
	press(SDL_SCANCODE_ESCAPE); for (int i = 0; i < 4; i++) press(SDL_SCANCODE_DOWN);
	press(SDL_SCANCODE_RETURN); press(SDL_SCANCODE_DOWN); press(SDL_SCANCODE_DOWN); press(SDL_SCANCODE_DOWN); press(SDL_SCANCODE_RETURN);   /* MODS */
	for (int i = 0; i < 4; i++) press(SDL_SCANCODE_DOWN);
	press(SDL_SCANCODE_LEFT); press(SDL_SCANCODE_LEFT);
	CHECK(S.start_hitp == 5, "recording a replay: the starting hitpoints do not change (%d)", S.start_hitp);
	screenshot("settings_mods_recording");
	/* the level page and the skill page */
	press(SDL_SCANCODE_HOME); press(SDL_SCANCODE_RETURN); state();
	CHECK(dialog != 0, "Customize level...: the level dialog");
	press(SDL_SCANCODE_RIGHT); press(SDL_SCANCODE_RIGHT); press(SDL_SCANCODE_RIGHT); press(SDL_SCANCODE_RIGHT); press(SDL_SCANCODE_RIGHT);
	screenshot("dialog_select_level");
	press(SDL_SCANCODE_RETURN); state();
	CHECK(dialog == 0 && !strcmp(subsection, "LEVEL") && at_setting("Customize another level..."), "level 6's page (%s)", subsection);
	press(SDL_SCANCODE_DOWN);
	screenshot("settings_level6_recording");
	recording = 0; press(SDL_SCANCODE_ESCAPE); press(SDL_SCANCODE_ESCAPE); press(SDL_SCANCODE_ESCAPE); press(SDL_SCANCODE_ESCAPE);
	CHECK(!overlay_menu_is_open(), "closed");
	press(SDL_SCANCODE_ESCAPE); for (int i = 0; i < 4; i++) press(SDL_SCANCODE_DOWN);
	press(SDL_SCANCODE_RETURN); press(SDL_SCANCODE_DOWN); press(SDL_SCANCODE_DOWN); press(SDL_SCANCODE_DOWN); press(SDL_SCANCODE_RETURN);   /* MODS */
	press(SDL_SCANCODE_RETURN); press(SDL_SCANCODE_RETURN); press(SDL_SCANCODE_DOWN);   /* level 6 (remembered), Sword type */
	CHECK(at_setting("Sword type") && S.sword_type[6] == SWORD_NONE, "level 6: sword type none");
	press(SDL_SCANCODE_RIGHT); press(SDL_SCANCODE_RIGHT);
	CHECK(S.sword_type[6] == 2, "Right x2: sword type 2 (%d)", S.sword_type[6]);
	screenshot("settings_level6");
	press(SDL_SCANCODE_LEFT); press(SDL_SCANCODE_LEFT); CHECK(S.sword_type[6] == SWORD_NONE, "back to none");
	press(SDL_SCANCODE_ESCAPE); press(SDL_SCANCODE_DOWN); press(SDL_SCANCODE_RETURN);   /* MODS: Customize guard skill... */
	press(SDL_SCANCODE_RIGHT); press(SDL_SCANCODE_RIGHT); press(SDL_SCANCODE_RETURN); state();
	CHECK(!strcmp(subsection, "SKILL"), "skill 2's page (%s)", subsection);
	press(SDL_SCANCODE_DOWN); press(SDL_SCANCODE_RIGHT);
	CHECK(S.strikeprob[2] == 76, "skill 2: strike probability 76 (%d)", S.strikeprob[2]);
	screenshot("settings_skill2");
	press(SDL_SCANCODE_LEFT);
	/* Restore defaults... (and its dialog) */
	press(SDL_SCANCODE_ESCAPE); press(SDL_SCANCODE_ESCAPE);
	for (int i = 0; i < 3; i++) press(SDL_SCANCODE_UP);   /* GENERAL */
	press(SDL_SCANCODE_RETURN); press(SDL_SCANCODE_END); press(SDL_SCANCODE_RETURN); state();
	CHECK(dialog != 0, "Restore defaults...: the confirmation dialog");
	screenshot("dialog_restore_defaults");
	applied = 0; press(SDL_SCANCODE_RETURN);
	CHECK(S.volume == 15 && S.start_hitp == 3 && applied, "OK: the defaults (volume %d, hit points %d)", S.volume, S.start_hitp);
	/* the key redefinition */
	press(SDL_SCANCODE_ESCAPE); for (int i = 0; i < 4; i++) press(SDL_SCANCODE_DOWN);   /* CONTROLS */
	press(SDL_SCANCODE_RETURN); for (int i = 0; i < 8; i++) press(SDL_SCANCODE_DOWN);
	CHECK(at_setting("Action"), "Action (%s)", setting);
	press(SDL_SCANCODE_RETURN); state();
	CHECK(dialog != 0, "Enter: the key dialog");
	screenshot("dialog_redefine_key");
	applied = 0; press(SDL_SCANCODE_RSHIFT); state();
	CHECK(dialog == 0 && !strcmp(S.keys[KEY_SHIFT], "Right Shift") && (applied & OVERLAY_MENU_APPLY_KEYS), "Right Shift: key_shift = %s", S.keys[KEY_SHIFT]);
	screenshot("settings_controls");
	press(SDL_SCANCODE_RETURN); press(SDL_SCANCODE_ESCAPE);
	CHECK(!strcmp(S.keys[KEY_SHIFT], "Right Shift"), "Esc: the key dialog cancelled");
	snprintf(S.keys[KEY_SHIFT], sizeof S.keys[KEY_SHIFT], "Left Shift");
	press(SDL_SCANCODE_ESCAPE); press(SDL_SCANCODE_ESCAPE); press(SDL_SCANCODE_ESCAPE);
	CHECK(!overlay_menu_is_open(), "closed");

	/* 5. the mouse; the quit confirmation */
	press(SDL_SCANCODE_ESCAPE);
	mouse(SDL_MOUSEMOTION, 160, 57 + 13 * 4, 0); frame();
	CHECK(at_item("SETTINGS"), "the mouse over SETTINGS (%s)", item);
	mouse(SDL_MOUSEBUTTONDOWN, 160, 57 + 13 * 4, SDL_BUTTON_LEFT); frame(); state();
	CHECK(page == 1, "a click on SETTINGS");
	mouse(SDL_MOUSEBUTTONDOWN, 160, 57 + 13 * 4, SDL_BUTTON_RIGHT); frame(); state();
	CHECK(page == 0, "the right button backs out");
	mouse(SDL_MOUSEMOTION, 160, 57, 0); frame();
	mouse(SDL_MOUSEBUTTONDOWN, 160, 57, SDL_BUTTON_LEFT); frame();
	CHECK(!overlay_menu_is_open(), "a click on RESUME closes it");
	mouse(SDL_MOUSEBUTTONDOWN, 100, 100, SDL_BUTTON_LEFT); frame();
	CHECK(overlay_menu_is_open(), "a click in the game opens it (SDLPoP's)");
	press(SDL_SCANCODE_UP); CHECK(at_item("QUIT GAME"), "QUIT GAME");
	press(SDL_SCANCODE_RETURN); frame(); state();
	CHECK(dialog != 0, "QUIT GAME: the confirmation");
	screenshot("dialog_quit");
	press(SDL_SCANCODE_RIGHT); press(SDL_SCANCODE_RETURN); state();
	CHECK(dialog == 0 && overlay_menu_is_open() && !quitted, "Cancel: back to the menu");
	press(SDL_SCANCODE_RETURN); press(SDL_SCANCODE_RETURN);
	CHECK(quitted && !overlay_menu_is_open(), "OK: the frontend quits");
	quitted = 0;

	/* 6. a controller */
	if (attach()) {
		button(SDL_CONTROLLER_BUTTON_START);
		CHECK(overlay_menu_is_open() && at_item("RESUME"), "the controller's Start opens the menu");
		uint32_t fc = shell_frame_count();
		button(SDL_CONTROLLER_BUTTON_DPAD_DOWN); frames(3);
		CHECK(at_item("QUICKSAVE (F6)"), "D-pad down (%s)", item);
		button(SDL_CONTROLLER_BUTTON_DPAD_DOWN); button(SDL_CONTROLLER_BUTTON_DPAD_DOWN); button(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
		CHECK(at_item("SETTINGS"), "D-pad down x3 (%s)", item);
		button(SDL_CONTROLLER_BUTTON_A); state();
		CHECK(page == 1, "A selects");
		button(SDL_CONTROLLER_BUTTON_B); state();
		CHECK(page == 0, "B backs out");
		CHECK(shell_frame_count() == fc, "the game waits");
		button(SDL_CONTROLLER_BUTTON_START);
		CHECK(!overlay_menu_is_open(), "Start closes it");
		frames(5);
		CHECK(shell_frame_count() > fc, "the game goes on");
		SDL_JoystickClose(vjoy); SDL_JoystickDetachVirtual(vindex);
	} else printf("(no virtual joystick: the controller part is skipped)\n");

	/* 7. enable_pause_menu = false */
	S.enable_pause_menu = 0;
	press(SDL_SCANCODE_ESCAPE); frames(3);
	CHECK(!overlay_menu_is_open() && shell_mode() == SH_PAUSE, "enable_pause_menu off: Esc is the game's pause");
	press(SDL_SCANCODE_SPACE); frames(3);
	CHECK(shell_mode() == SH_PLAY, "a key ends it");
	press(SDL_SCANCODE_BACKSPACE);
	CHECK(overlay_menu_is_open(), "Backspace still opens the menu");
	press(SDL_SCANCODE_ESCAPE);
	CHECK(!overlay_menu_is_open(), "Esc closes it");
	/* replaying: no quicksave, quickload or restarts offered */
	S.enable_pause_menu = 1; replaying = 1;
	press(SDL_SCANCODE_ESCAPE); press(SDL_SCANCODE_DOWN);
	CHECK(at_item("SETTINGS"), "replaying: RESUME, then SETTINGS (%s)", item);
	screenshot("pause_replaying");
	press(SDL_SCANCODE_ESCAPE); replaying = 0;

	controller_quit();
	remove(cfg); remove(ini);
	const char *names[3] = { "PRINCE.OPT", "PRINCE.HOF", "PRINCE.SAV" }; char p[600];
	for (int i = 0; i < 3; i++) { snprintf(p, sizeof p, "%s/%s", dir, names[i]); remove(p); }
	rmdir(dir);
	SDL_Quit();
	printf("%s: %d failure(s)\n", failures ? "FAILED" : "passed", failures);
	return failures ? 1 : 0;
}
