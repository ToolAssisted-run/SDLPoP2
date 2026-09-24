/* Game controllers (the 'controller' suite): sdl/controller.c driven by SDL's virtual joystick, no hardware needed.
 *  1. the mapping, by the shell's mode: the keys held and typed for the D-pad, the stick (dead zone, sectors,
 *     joystick_only_horizontal), the buttons (defaults and remapped), the menus (A Enter, B Esc, X Tab, Y a name, a
 *     repeating direction), the title / scenes / demo "any key", the pause ended by Start, the frontend's actions
 *     (quicksave, quickload, the info screen), a button held across a change of mode, hot-plugging, the rumble;
 *  2. (with GAME_DIR) end to end, the whole program: a scripted controller session (the intro cut short, play with the
 *     D-pad, the stick and the buttons, pause and unpause, a quicksave and a quickload, a restart) gives the same
 *     shell inputs every frame and the same pop2_hash every frame as the equivalent keyboard session; the rumbles
 *     are the prince's hit point losses.
 * Exit 77 (meson: skipped) when SDL cannot create a virtual joystick.
 * usage: controllertest [GAME_DIR] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <SDL.h>
#include "../source/core.h"
#include "../source/shell.h"
#include "../source/settings.h"
#include "../source/replay.h"
#include "../source/loader.h"
#include "../source/globals.h"
#include "../sdl/controller.h"

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL: " __VA_ARGS__); printf("\n"); } else if (getenv("VERBOSE")) { printf("ok: " __VA_ARGS__); printf("\n"); } } while (0)

/* ---- the virtual controller ---- */
static SDL_Joystick *vjoy; static int vindex = -1, vrumbles;
static int SDLCALL on_rumble(void *u, Uint16 lo, Uint16 hi) { (void)u; if (lo == 0xFFFF && hi == 0xFFFF) vrumbles++; return 0; }
static void pump(void) { SDL_Event e; SDL_PumpEvents(); while (SDL_PollEvent(&e)) controller_event(&e); }
static int attach(void)
{
	SDL_VirtualJoystickDesc d; SDL_zero(d);
	d.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION; d.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
	d.naxes = SDL_CONTROLLER_AXIS_MAX; d.nbuttons = SDL_CONTROLLER_BUTTON_MAX; d.name = "SDLPoP2 test pad"; d.Rumble = on_rumble;
	vindex = SDL_JoystickAttachVirtualEx(&d);
	if (vindex < 0) return 0;
	vjoy = SDL_JoystickOpen(vindex);
	pump();
	return vjoy != NULL;
}
static void detach(void) { SDL_JoystickClose(vjoy); vjoy = NULL; SDL_JoystickDetachVirtual(vindex); pump(); }
static void button(int b, int down) { SDL_JoystickSetVirtualButton(vjoy, b, (Uint8)down); }
static void axis(int a, int v)   /* (SDL's mapping of a virtual controller takes a trigger's whole range: 0 is -32768) */
{
	if (a == SDL_CONTROLLER_AXIS_TRIGGERLEFT || a == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) v = v * 2 - 32768;
	SDL_JoystickSetVirtualAxis(vjoy, a, (Sint16)(v > 32767 ? 32767 : v));
}
static void neutral(void)
{
	for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; b++) button(b, 0);
	for (int a = 0; a < SDL_CONTROLLER_AXIS_MAX; a++) axis(a, 0);
}
/* one frame: the events, then the mapping into the frame's input (typed cleared first, as the frontend does after a step) */
static shell_input in; static int acts;
static void frame(int mode) { in.ntyped = 0; pump(); acts = controller_frame(&in, mode, 1); }
static int typed1(uint16_t code) { return in.ntyped == 1 && in.typed[0] == code; }
static int held_keys(void) { int n = 0; for (int s = 0; s < (int)sizeof in.down; s++) n += in.down[s] != 0; return n; }
#define A_ SDL_CONTROLLER_BUTTON_A
#define B_ SDL_CONTROLLER_BUTTON_B
#define X_ SDL_CONTROLLER_BUTTON_X
#define Y_ SDL_CONTROLLER_BUTTON_Y

static void mapping_tests(void)
{
	pop2_settings s; settings_defaults(&s);
	/* hot-plug: a controller attached after the start is opened, a detached one closed */
	CHECK(controller_count() == 1, "the virtual controller is opened (%d)", controller_count());
	detach(); CHECK(controller_count() == 0, "a detached controller is closed");
	CHECK(attach() && controller_count() == 1, "a controller attached again is opened");
	neutral(); memset(&in, 0, sizeof in); frame(SH_PLAY);

	/* playing: the D-pad */
	button(SDL_CONTROLLER_BUTTON_DPAD_LEFT, 1); frame(SH_PLAY);
	CHECK(in.down[0x4B] && held_keys() == 1 && typed1(0x4B00), "D-pad left: the left arrow held and typed");
	frame(SH_PLAY); CHECK(in.down[0x4B] && in.ntyped == 0, "held: typed once");
	button(SDL_CONTROLLER_BUTTON_DPAD_UP, 1); frame(SH_PLAY);
	CHECK(in.down[0x4B] && in.down[0x48] && held_keys() == 2 && typed1(0x4800), "D-pad up + left: up and left held (Home)");
	button(SDL_CONTROLLER_BUTTON_DPAD_LEFT, 0); button(SDL_CONTROLLER_BUTTON_DPAD_UP, 0); button(SDL_CONTROLLER_BUTTON_DPAD_DOWN, 1); button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT, 1); frame(SH_PLAY);
	CHECK(in.down[0x4D] && in.down[0x50] && held_keys() == 2, "D-pad down + right: PgDn");
	neutral(); frame(SH_PLAY); CHECK(held_keys() == 0 && in.shift_flags == 0, "released: nothing held");
	/* the buttons */
	button(X_, 1); frame(SH_PLAY);
	CHECK(in.down[0x2A] && in.shift_flags == 2 && in.ntyped == 0, "X: left Shift held, nothing typed");
	button(X_, 0); button(B_, 1); frame(SH_PLAY);
	CHECK(!in.down[0x2A] && in.down[0x1D] && in.shift_flags == 4 && in.ntyped == 0, "B: Ctrl held");
	button(B_, 0); button(Y_, 1); frame(SH_PLAY); CHECK(in.down[0x48] && held_keys() == 1 && typed1(0x4800), "Y: up");
	button(Y_, 0); button(A_, 1); frame(SH_PLAY); CHECK(in.down[0x50] && held_keys() == 1 && typed1(0x5000), "A: down");
	button(A_, 0); axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 20000); frame(SH_PLAY); CHECK(in.down[0x2A], "right trigger: Shift");
	axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 5000); frame(SH_PLAY); CHECK(!in.down[0x2A], "a trigger within the dead zone: nothing");
	neutral(); frame(SH_PLAY);
	button(SDL_CONTROLLER_BUTTON_START, 1); frame(SH_PLAY); CHECK(typed1(0x1B) && held_keys() == 0, "Start: Esc typed (the pause)");
	button(SDL_CONTROLLER_BUTTON_START, 0); frame(SH_PAUSE);
	button(SDL_CONTROLLER_BUTTON_START, 1); frame(SH_PAUSE); CHECK(typed1(0x0D), "Start in the pause: Enter (the pause ends)");
	button(SDL_CONTROLLER_BUTTON_START, 0); frame(SH_PLAY);
	button(SDL_CONTROLLER_BUTTON_BACK, 1); frame(SH_PLAY); CHECK(typed1(0x1E00) && !in.down[0x38] && in.shift_flags == 0, "Back: Alt+A typed (restart the level)");
	button(SDL_CONTROLLER_BUTTON_BACK, 0); button(SDL_CONTROLLER_BUTTON_RIGHTSTICK, 1); frame(SH_PLAY); CHECK(typed1(0x20), "right stick click: space (the time left)");
	button(SDL_CONTROLLER_BUTTON_RIGHTSTICK, 0); button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, 1); frame(SH_PLAY);
	CHECK(acts == CONTROLLER_QUICKSAVE && in.ntyped == 0, "left shoulder: quicksave");
	button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, 0); button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, 1); frame(SH_PLAY);
	CHECK(acts == CONTROLLER_QUICKLOAD && in.ntyped == 0, "right shoulder: quickload");
	button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, 0); frame(SH_PLAY); CHECK(acts == 0, "released: no action");
	/* the stick */
	axis(SDL_CONTROLLER_AXIS_LEFTX, -20000); frame(SH_PLAY); CHECK(in.down[0x4B] && held_keys() == 1, "stick left");
	axis(SDL_CONTROLLER_AXIS_LEFTX, -7000); frame(SH_PLAY); CHECK(held_keys() == 0, "stick within the dead zone (8000): nothing");
	axis(SDL_CONTROLLER_AXIS_LEFTX, -20000); axis(SDL_CONTROLLER_AXIS_LEFTY, -20000); frame(SH_PLAY); CHECK(in.down[0x4B] && in.down[0x48] && held_keys() == 2, "stick up-left: up and left");
	axis(SDL_CONTROLLER_AXIS_LEFTX, 30000); axis(SDL_CONTROLLER_AXIS_LEFTY, 9000); frame(SH_PLAY); CHECK(in.down[0x4D] && held_keys() == 1, "stick right, a little down: right only (the sectors)");
	axis(SDL_CONTROLLER_AXIS_LEFTX, 3000); axis(SDL_CONTROLLER_AXIS_LEFTY, 30000); frame(SH_PLAY); CHECK(in.down[0x50] && held_keys() == 1, "stick down");
	axis(SDL_CONTROLLER_AXIS_LEFTX, -32768); axis(SDL_CONTROLLER_AXIS_LEFTY, -32768); frame(SH_PLAY); CHECK(in.down[0x4B] && in.down[0x48], "stick in the corner (-32768, -32768): up-left");
	neutral(); frame(SH_PLAY);

	/* menus */
	button(A_, 1); frame(SH_MENU); CHECK(typed1(0x0D) && held_keys() == 0, "menu: A Enter");
	button(A_, 0); button(B_, 1); frame(SH_MENU); CHECK(typed1(0x1B), "menu: B Esc");
	button(B_, 0); button(X_, 1); frame(SH_MENU); CHECK(typed1(0x09) && !in.down[0x2A], "menu: X Tab (no Shift)");
	button(X_, 0); button(Y_, 1); frame(SH_MENU);
	CHECK(in.ntyped == 6 && in.typed[0] == 'P' && in.typed[5] == 'e', "menu: Y types the name Prince");
	button(Y_, 0); button(SDL_CONTROLLER_BUTTON_START, 1); frame(SH_MENU); CHECK(typed1(0x1B), "menu: Start Esc");
	button(SDL_CONTROLLER_BUTTON_START, 0); button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, 1); frame(SH_MENU); CHECK(typed1(0x20) && acts == 0, "menu: another button, space (no quicksave)");
	button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, 0); button(SDL_CONTROLLER_BUTTON_DPAD_DOWN, 1); button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT, 1); frame(SH_MENU);
	CHECK(typed1(0x5000) && held_keys() == 1, "menu: a diagonal moves one way (down)");
	int reps = 0; for (int f = 0; f < 70; f++) { frame(SH_MENU); reps += in.ntyped; }
	CHECK(reps == 9, "menu: a held direction repeats after 0.4 s, 14 a second (%d in a second)", reps);
	neutral(); frame(SH_MENU);
	axis(SDL_CONTROLLER_AXIS_LEFTY, -30000); frame(SH_MENU); CHECK(typed1(0x4800), "menu: the stick up");
	neutral(); frame(SH_MENU);
	/* the "any key" of the title, the scenes and the demo */
	button(A_, 1); frame(SH_TITLE); CHECK(typed1(0x20), "title: A space");
	button(A_, 0); frame(SH_SCENE); button(SDL_CONTROLLER_BUTTON_START, 1); frame(SH_SCENE); CHECK(typed1(0x20), "scene: Start space");
	button(SDL_CONTROLLER_BUTTON_START, 0); frame(SH_DEMO); button(X_, 1); frame(SH_DEMO); CHECK(typed1(0x0D) && !in.down[0x2A], "demo: X Enter");
	button(X_, 0); frame(SH_DEMO);
	/* a button held across a change of mode is ignored until released */
	button(X_, 1); frame(SH_MENU); CHECK(typed1(0x09), "menu: X");
	frame(SH_PLAY); CHECK(!in.down[0x2A] && in.ntyped == 0, "X still held when the game goes on: no Shift");
	button(X_, 0); frame(SH_PLAY); button(X_, 1); frame(SH_PLAY); CHECK(in.down[0x2A], "pressed again: Shift");
	button(X_, 0); frame(SH_PLAY);
	/* not fed (a replay, the info screen): the held keys released, buttons close the info screen */
	button(SDL_CONTROLLER_BUTTON_DPAD_LEFT, 1); frame(SH_PLAY); CHECK(in.down[0x4B], "left held");
	in.ntyped = 0; pump(); acts = controller_frame(&in, SH_PLAY, 0); CHECK(held_keys() == 0 && acts == 0, "not fed: released");
	button(B_, 1); in.ntyped = 0; pump(); acts = controller_frame(&in, SH_PLAY, 0); CHECK(acts == CONTROLLER_CLOSE && held_keys() == 0 && in.ntyped == 0, "not fed: B closes the info screen");
	neutral(); frame(SH_PLAY);
	/* the rumble: the hit points going down while playing the same level */
	int r0 = controller_rumbles(), v0 = vrumbles;
	controller_after_step(SH_PLAY, 1, 3, 0); controller_after_step(SH_PLAY, 1, 3, 0); controller_after_step(SH_PLAY, 1, 2, 0);
	CHECK(controller_rumbles() == r0 + 1 && vrumbles == v0 + 1, "hurt: one rumble, full strength (%d)", vrumbles - v0);
	controller_after_step(SH_PLAY, 1, 1, 1); CHECK(controller_rumbles() == r0 + 1, "a quickload: no rumble");
	controller_after_step(SH_PLAY, 2, 0, 0); CHECK(controller_rumbles() == r0 + 1, "another level: no rumble");
	controller_after_step(SH_MENU, 2, 0, 0); controller_after_step(SH_PLAY, 2, 0, 0); controller_after_step(SH_PLAY, 2, 2, 0); controller_after_step(SH_PLAY, 2, 1, 0);
	CHECK(controller_rumbles() == r0 + 2, "hurt again");
	controller_quit();

	/* remapped buttons, joystick_only_horizontal, the info button, no rumble */
	pop2_settings t; settings_defaults(&t);
	CHECK(settings_parse_text(&t, "[Controller]\nbutton_shift = a, lefttrigger\nbutton_down = none\nbutton_info = guide leftstick\nbutton_bogus = a\n"
	                              "joystick_only_horizontal = true\njoystick_threshold = 12000\ncontroller_rumble = false\n", "text", NULL) == 1, "the [Controller] options parse (one unknown)");
	CHECK(controller_init(&t, 1) == 1, "controller_init"); pump();
	CHECK(controller_count() == 1, "the connected controller opened at the start");
	memset(&in, 0, sizeof in); neutral(); frame(SH_PLAY);
	button(A_, 1); frame(SH_PLAY); CHECK(in.down[0x2A] && held_keys() == 1, "button_shift = a");
	button(A_, 0); axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT, 30000); frame(SH_PLAY); CHECK(in.down[0x2A], "button_shift: lefttrigger");
	axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT, 0); button(X_, 1); frame(SH_PLAY); CHECK(held_keys() == 0, "X: nothing any more");
	button(X_, 0); axis(SDL_CONTROLLER_AXIS_LEFTY, -30000); axis(SDL_CONTROLLER_AXIS_LEFTX, -30000); frame(SH_PLAY);
	CHECK(in.down[0x4B] && held_keys() == 1, "joystick_only_horizontal: the stick up-left gives left");
	axis(SDL_CONTROLLER_AXIS_LEFTX, -10000); axis(SDL_CONTROLLER_AXIS_LEFTY, 0); frame(SH_PLAY); CHECK(held_keys() == 0, "joystick_threshold = 12000");
	neutral(); button(SDL_CONTROLLER_BUTTON_DPAD_UP, 1); frame(SH_PLAY); CHECK(in.down[0x48], "the D-pad still gives up");
	neutral(); frame(SH_MENU); axis(SDL_CONTROLLER_AXIS_LEFTY, -30000); frame(SH_MENU); CHECK(typed1(0x4800), "menus: the stick still goes up");
	neutral(); frame(SH_PLAY); button(SDL_CONTROLLER_BUTTON_LEFTSTICK, 1); frame(SH_PLAY); CHECK(acts == CONTROLLER_INFO && in.ntyped == 0, "button_info: the info screen");
	button(SDL_CONTROLLER_BUTTON_LEFTSTICK, 0); frame(SH_PLAY);
	controller_after_step(SH_PLAY, 1, 3, 0); controller_after_step(SH_PLAY, 1, 2, 0); CHECK(controller_rumbles() == 0, "controller_rumble = false");
	controller_quit();
	t.enable_controller = 0; CHECK(controller_init(&t, 1) == 0 && controller_frame(&in, SH_PLAY, 1) == 0, "enable_controller = false: nothing");
}

/* ---- end to end: a controller session and the keyboard session it stands for ---- */
static const char *game_dir;
enum { FRAMES = 6000, PAUSE_AT = 3000, UNPAUSE_AT = 3100, SAVE_AT = 2600, LOAD_AT = 4200, RESTART_AT = 5000 };
typedef struct pad_state { int dx, dy, how; int shift, ctrl, time, start, back, lb, rb, a_title; } pad_state;   /* how: 0 D-pad, 1 stick, 2 Y / A buttons */
static uint32_t lcg; static pad_state cur;
static uint32_t rnd(void) { lcg = lcg * 1103515245u + 12345u; return lcg >> 16; }
/* the script, in what the player does */
static void script(int f)
{
	if (f == 0) { lcg = 4242; memset(&cur, 0, sizeof cur); }
	cur.time = cur.start = cur.back = cur.lb = cur.rb = cur.a_title = 0;   /* (the presses last one frame) */
	if (f == 60) cur.a_title = 1;   /* A at the intro: it is cut short, the game starts */
	int quiet = (f >= PAUSE_AT - 20 && f <= UNPAUSE_AT + 10) || (f >= RESTART_AT - 20 && f <= RESTART_AT + 5);
	if (quiet) { cur.dx = cur.dy = 0; cur.shift = cur.ctrl = 0; }
	else if (f > 200 && f % 11 == 0) {
		uint32_t r = rnd();
		int d = r % 10; cur.dx = d < 9 ? d % 3 - 1 : 0; cur.dy = d < 9 ? d / 3 - 1 : 0;
		cur.how = (r >> 4) % 3; if (cur.how == 2 && cur.dx) cur.how = 0;   /* (the buttons give up and down only) */
		cur.shift = (r >> 6) % 4 == 0 ? 1 + (r >> 8) % 2 : 0;   /* 1 X, 2 the right trigger */
		cur.ctrl = (r >> 9) % 13 == 0;
		cur.time = (r >> 12) % 23 == 0;
	}
	if (f == PAUSE_AT || f == UNPAUSE_AT) cur.start = 1;
	if (f == RESTART_AT) cur.back = 1;
	cur.lb = f == SAVE_AT; cur.rb = f == LOAD_AT;
}
static void to_pad(void)
{
	neutral();
	if (cur.how == 0) {
		if (cur.dx) button(cur.dx < 0 ? SDL_CONTROLLER_BUTTON_DPAD_LEFT : SDL_CONTROLLER_BUTTON_DPAD_RIGHT, 1);
		if (cur.dy) button(cur.dy < 0 ? SDL_CONTROLLER_BUTTON_DPAD_UP : SDL_CONTROLLER_BUTTON_DPAD_DOWN, 1);
	} else if (cur.how == 1) { axis(SDL_CONTROLLER_AXIS_LEFTX, cur.dx * 28000); axis(SDL_CONTROLLER_AXIS_LEFTY, cur.dy * 28000); }
	else if (cur.dy) button(cur.dy < 0 ? Y_ : A_, 1);
	if (cur.shift == 1) button(X_, 1);
	if (cur.shift == 2) axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 32767);
	if (cur.ctrl) button(B_, 1);
	if (cur.time) button(SDL_CONTROLLER_BUTTON_RIGHTSTICK, 1);
	if (cur.start) button(SDL_CONTROLLER_BUTTON_START, 1);
	if (cur.back) button(SDL_CONTROLLER_BUTTON_BACK, 1);
	if (cur.lb) button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, 1);
	if (cur.rb) button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, 1);
	if (cur.a_title) button(A_, 1);
}
/* the same on the keyboard: the arrows, Shift, Ctrl, and the keys typed */
static void to_keys(int f, shell_input *k, int *action)
{
	static uint8_t hold[0x60]; if (f == 0) memset(hold, 0, sizeof hold);
	uint8_t want[0x60] = {0};
	if (cur.dx) want[cur.dx < 0 ? 0x4B : 0x4D] = 1;
	if (cur.dy) want[cur.dy < 0 ? 0x48 : 0x50] = 1;
	if (cur.shift) want[0x2A] = 1;
	if (cur.ctrl) want[0x1D] = 1;
	for (int s = 0; s < 0x60; s++) if (hold[s] && !want[s]) shell_input_key(k, s, 0, 0);
	for (int s = 0; s < 0x60; s++) if (want[s] && !hold[s]) shell_input_key(k, s, 1, 0);
	memcpy(hold, want, sizeof hold);
	if (cur.a_title || cur.time) { shell_input_key(k, 0x39, 1, 0x20); shell_input_key(k, 0x39, 0, 0); }   /* space */
	if (f == PAUSE_AT) { shell_input_key(k, 0x01, 1, 0x1B); shell_input_key(k, 0x01, 0, 0); }                /* Esc: the pause */
	if (f == UNPAUSE_AT) { shell_input_key(k, 0x1C, 1, 0x0D); shell_input_key(k, 0x1C, 0, 0); }              /* Enter: go on */
	if (cur.back) { shell_input_key(k, 0x38, 1, 0); shell_input_key(k, 0x1E, 1, 'a'); shell_input_key(k, 0x1E, 0, 0); shell_input_key(k, 0x38, 0, 0); }   /* Alt+A */
	*action = cur.lb ? REPLAY_QUICKSAVE : cur.rb ? REPLAY_QUICKLOAD : REPLAY_NONE;
}
typedef struct result { uint64_t digest, inputs, hash; int frames, play, paused, saves, loads, hurts, rumbles, vrumbles, ok; } result;
static uint64_t fnv(uint64_t h, const void *p, size_t n) { const uint8_t *b = p; while (n--) h = (h ^ *b++) * 1099511628211ull; return h; }
static uint64_t input_digest(uint64_t h, const shell_input *k)
{
	uint16_t t[8]; int n = k->ntyped < 8 ? k->ntyped : 8; memcpy(t, k->typed, n * sizeof *t);
	for (int i = 1; i < n; i++) for (int j = i; j > 0 && t[j - 1] > t[j]; j--) { uint16_t x = t[j]; t[j] = t[j - 1]; t[j - 1] = x; }   /* (as a set) */
	h = fnv(h, k->down, sizeof k->down); h = fnv(h, &k->shift_flags, 1); h = fnv(h, &n, sizeof n);
	return fnv(h, t, n * sizeof *t);
}
static result session(int pad)
{
	result res = {0}; char dir[64]; snprintf(dir, sizeof dir, "/tmp/sdlpop2-controllertest-XXXXXX");
	if (!mkdtemp(dir)) exit(2);
	snprintf(file_dir, sizeof file_dir, "%s", dir);
	if (pad) {
		pop2_settings s; settings_defaults(&s);
		if (SDL_Init(SDL_INIT_GAMECONTROLLER) != 0 || !controller_init(&s, 1) || !attach() || controller_count() != 1) { res.ok = -1; return res; }
	}
	shell_set_seed(0x5EED);
	if (!shell_init(game_dir, 0, NULL)) { printf("shell_init failed\n"); exit(3); }
	static shell_input k; uint64_t dg = 1469598103934665603ull, di = dg; int prev_hp = -1, prev_mode = -1, prev_level = -1;
	for (int f = 0; f < FRAMES; f++) {
		int action = REPLAY_NONE;
		script(f);
		if (pad) {
			to_pad(); pump();
			int c = controller_frame(&k, shell_mode(), 1);
			if (c & CONTROLLER_QUICKSAVE) action = REPLAY_QUICKSAVE;
			if (c & CONTROLLER_QUICKLOAD) action = REPLAY_QUICKLOAD;
		} else to_keys(f, &k, &action);
		di = input_digest(di, &k);
		if (action == REPLAY_QUICKSAVE) shell_quicksave();
		if (action == REPLAY_QUICKLOAD) shell_quickload();
		int r = shell_step(&k);
		k.ntyped = 0;
		int q = shell_quick_result(); if (q == 1) res.saves++; if (q == 2) res.loads++;
		int m = shell_mode(), hp = Kid.f12, lv = pop2_level();
		if (pad) controller_after_step(m, lv, hp, q == 2);
		else if (prev_mode == SH_PLAY && m == SH_PLAY && lv == prev_level && q != 2 && hp < prev_hp) res.hurts++;
		prev_mode = m; prev_level = lv; prev_hp = hp;
		res.play += m == SH_PLAY; res.paused += m == SH_PAUSE;
		dg = fnv(dg, &(uint64_t){ pop2_hash() }, 8);
		res.frames = f + 1;
		if (r == SHELL_EXIT) break;
	}
	res.digest = dg; res.inputs = di; res.hash = pop2_hash(); res.ok = 1;
	if (pad) { res.rumbles = controller_rumbles(); res.vrumbles = vrumbles; controller_quit(); }
	char p[128]; const char *names[3] = { "PRINCE.OPT", "PRINCE.HOF", "PRINCE.SAV" };
	for (int i = 0; i < 3; i++) { snprintf(p, sizeof p, "%s/%s", dir, names[i]); unlink(p); }
	rmdir(dir);
	return res;
}
static result run(int pad)   /* (a child process: the shell is a process-wide coroutine) */
{
	int fd[2]; if (pipe(fd)) { perror("pipe"); exit(2); }
	fflush(stdout);
	pid_t pid = fork();
	if (pid == 0) { close(fd[0]); result r = session(pad); if (write(fd[1], &r, sizeof r) != sizeof r) _exit(4); _exit(0); }
	close(fd[1]); result r; memset(&r, 0, sizeof r);
	if (read(fd[0], &r, sizeof r) != sizeof r) { failures++; printf("FAIL: a session crashed\n"); }
	close(fd[0]); int st; waitpid(pid, &st, 0);
	return r;
}

int main(int argc, char **argv)
{
	setenv("SDL_VIDEODRIVER", "offscreen", 0); setenv("SDL_AUDIODRIVER", "dummy", 0);
	game_dir = argc > 1 ? argv[1] : NULL;
	/* the end-to-end sessions first (children that start SDL themselves) */
	if (game_dir) {
		result kb = run(0), pad = run(1);
		if (pad.ok == -1) { printf("SKIP: SDL cannot create a virtual game controller\n"); return 77; }
		printf("session: %d frames (%d playing, %d paused), %d quicksave(s), %d quickload(s), %d hit point loss(es), final hash %016llx\n",
		       kb.frames, kb.play, kb.paused, kb.saves, kb.loads, kb.hurts, (unsigned long long)kb.hash);
		CHECK(kb.frames == FRAMES && kb.play > FRAMES / 2 && kb.paused >= 50 && kb.saves == 1 && kb.loads == 1, "the keyboard session plays, pauses, saves and loads");
		CHECK(pad.inputs == kb.inputs, "the controller gives the keyboard session's shell inputs, every frame");
		CHECK(pad.digest == kb.digest && pad.hash == kb.hash && pad.frames == kb.frames, "the same pop2_hash every frame (controller %016llx, keyboard %016llx)",
		      (unsigned long long)pad.hash, (unsigned long long)kb.hash);
		CHECK(pad.saves == kb.saves && pad.loads == kb.loads && pad.paused == kb.paused, "the same quicksaves, quickloads and pause");
		CHECK(kb.hurts > 0 && pad.rumbles == kb.hurts && pad.vrumbles >= 1, "a rumble for each hit point loss (%d, %d losses; %d reached the pad: SDL extends a running one)",
		      pad.rumbles, kb.hurts, pad.vrumbles);
	}
	if (SDL_Init(SDL_INIT_GAMECONTROLLER) != 0) { printf("SKIP: SDL: %s\n", SDL_GetError()); return 77; }
	pop2_settings s; settings_defaults(&s);
	if (!controller_init(&s, 1) || !attach()) { printf("SKIP: SDL cannot create a virtual game controller (%s)\n", SDL_GetError()); return 77; }
	mapping_tests();
	SDL_Quit();
	printf("%s: %d failure(s)\n", failures ? "FAILED" : "passed", failures);
	return failures ? 1 : 0;
}
