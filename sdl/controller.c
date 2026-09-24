/* SDL game controllers to the keyboard's shell inputs (controller.h) */
#include <stdio.h>
#include <string.h>
#include "controller.h"

#define MAX_PADS 8
#define NBUTTONS SDL_CONTROLLER_BUTTON_MAX            /* the buttons' bits in the masks ... */
#define BIT_LTRIGGER NBUTTONS                          /* ... then the triggers, pressed beyond joystick_threshold */
#define BIT_RTRIGGER (NBUTTONS + 1)
#define B(b) (1u << (b))
#define DPAD (B(SDL_CONTROLLER_BUTTON_DPAD_UP) | B(SDL_CONTROLLER_BUTTON_DPAD_DOWN) | B(SDL_CONTROLLER_BUTTON_DPAD_LEFT) | B(SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
enum { RUMBLE_STRENGTH = 0xFFFF, RUMBLE_MS = 100 };   /* SDLPoP's: full strength, 100 ms */
enum { REPEAT_DELAY = 28, REPEAT_EVERY = 5 };         /* menus: a held direction repeats (video frames: 0.4 s, then 14 a second) */
/* PC scan codes */
enum { SC_ESC = 0x01, SC_TAB = 0x0F, SC_ENTER = 0x1C, SC_CTRL = 0x1D, SC_A = 0x1E, SC_LSHIFT = 0x2A, SC_ALT = 0x38, SC_SPACE = 0x39,
       SC_UP = 0x48, SC_LEFT = 0x4B, SC_RIGHT = 0x4D, SC_DOWN = 0x50 };
enum { CAT_NONE, CAT_GAME, CAT_MENU, CAT_ANY };

static struct { SDL_GameController *gc; SDL_JoystickID id; } pads[MAX_PADS];
static int npads, enabled, only_virt, rumble_on, threshold, only_horizontal, rumbles, last_used = -1;
static uint32_t button_mask[BUTTON_COUNT];   /* the ini's buttons, by action */
static uint32_t latched;                     /* presses since the last frame */
static uint32_t prev_cur, prev_active, ignored;   /* the buttons down / in use last frame; held across a change of category */
static int prev_cat = CAT_NONE, held_dirs, repeat_count;
static uint8_t held[0x60];                   /* the keys the controllers hold down in the frame's input */
static int prev_mode = -1, prev_level, prev_hp;

static void pad_open(int device);
static uint32_t parse_buttons(const char *action, const char *v)
{
	uint32_t m = 0; char buf[64]; snprintf(buf, sizeof buf, "%s", v);
	for (char *save = NULL, *t = strtok_r(buf, " ,\t", &save); t; t = strtok_r(NULL, " ,\t", &save)) {
		if (!SDL_strcasecmp(t, "none")) continue;
		SDL_GameControllerButton b = SDL_GameControllerGetButtonFromString(t);
		if (b != SDL_CONTROLLER_BUTTON_INVALID && b < NBUTTONS) { m |= B(b); continue; }
		SDL_GameControllerAxis a = SDL_GameControllerGetAxisFromString(t);
		if (a == SDL_CONTROLLER_AXIS_TRIGGERLEFT) m |= B(BIT_LTRIGGER);
		else if (a == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) m |= B(BIT_RTRIGGER);
		else fprintf(stderr, "sdlpop2: %s: unknown controller button '%s'\n", action, t);
	}
	return m;
}

int controller_init(const pop2_settings *s, int only_virtual)
{
	enabled = 0; npads = 0; rumbles = 0; last_used = -1; latched = prev_cur = prev_active = ignored = 0; prev_cat = CAT_NONE;
	held_dirs = repeat_count = 0; memset(held, 0, sizeof held); prev_mode = -1;
	if (!s->enable_controller) return 0;
	for (int b = 0; b < BUTTON_COUNT; b++) button_mask[b] = parse_buttons(settings_button_ini_names[b], s->buttons[b]);
	rumble_on = s->controller_rumble; threshold = s->joystick_threshold; only_horizontal = s->joystick_only_horizontal; only_virt = only_virtual;
	if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) { fprintf(stderr, "sdlpop2: no game controllers (%s)\n", SDL_GetError()); return 0; }
	if (s->gamecontrollerdb_file[0] && SDL_GameControllerAddMappingsFromFile(s->gamecontrollerdb_file) < 0)
		fprintf(stderr, "sdlpop2: gamecontrollerdb_file %s: %s\n", s->gamecontrollerdb_file, SDL_GetError());
	enabled = 1;
	for (int i = 0; i < SDL_NumJoysticks(); i++) pad_open(i);   /* (those connected now; their SDL_CONTROLLERDEVICEADDED events find them open) */
	return 1;
}
void controller_quit(void)
{
	for (int i = 0; i < npads; i++) SDL_GameControllerClose(pads[i].gc);
	npads = 0;
	if (enabled) SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
	enabled = 0;
}
int controller_count(void) { return npads; }
int controller_rumbles(void) { return rumbles; }

static int pad_index(SDL_JoystickID id) { for (int i = 0; i < npads; i++) if (pads[i].id == id) return i; return -1; }
static void pad_open(int device)
{
	if (npads >= MAX_PADS || !SDL_IsGameController(device) || (only_virt && !SDL_JoystickIsVirtual(device))) return;
	if (pad_index(SDL_JoystickGetDeviceInstanceID(device)) >= 0) return;
	SDL_GameController *gc = SDL_GameControllerOpen(device);
	if (!gc) { fprintf(stderr, "sdlpop2: controller %d: %s\n", device, SDL_GetError()); return; }
	pads[npads].gc = gc; pads[npads].id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gc)); npads++;
	fprintf(stderr, "sdlpop2: controller connected: %s\n", SDL_GameControllerName(gc) ? SDL_GameControllerName(gc) : "?");
}
static void pad_close(SDL_JoystickID id)
{
	int i = pad_index(id); if (i < 0) return;
	fprintf(stderr, "sdlpop2: controller disconnected: %s\n", SDL_GameControllerName(pads[i].gc) ? SDL_GameControllerName(pads[i].gc) : "?");
	SDL_GameControllerClose(pads[i].gc);
	pads[i] = pads[--npads];
	if (last_used == i) last_used = -1; else if (last_used == npads) last_used = i;
}
int controller_event(const SDL_Event *e)
{
	if (!enabled) return 0;
	switch (e->type) {
	case SDL_CONTROLLERDEVICEADDED: pad_open(e->cdevice.which); return 1;
	case SDL_CONTROLLERDEVICEREMOVED: pad_close(e->cdevice.which); return 1;
	case SDL_CONTROLLERBUTTONDOWN: {
		int i = pad_index(e->cbutton.which); if (i < 0) return 1;
		if (e->cbutton.button < NBUTTONS) latched |= B(e->cbutton.button);
		last_used = i; return 1; }
	case SDL_CONTROLLERBUTTONUP: case SDL_CONTROLLERAXISMOTION: return 1;
	}
	return 0;
}

/* the left stick: 8 sectors beyond the dead zone (SDLPoP's: 120 degrees each way for left / right and up, 110 for down) */
static void stick_dirs(int x, int y, int horizontal_only, int *dx, int *dy)
{
	*dx = *dy = 0;
	if (horizontal_only) { if (x > threshold) *dx = 1; else if (x < -threshold) *dx = -1; return; }
	if ((double)x * x + (double)y * y < (double)threshold * threshold || (!x && !y)) return;
	double deg = SDL_atan2((double)y, (double)x) * 180.0 / M_PI;   /* 0 right, > 0 down */
	if (SDL_fabs(deg) < 60) *dx = 1; else if (SDL_fabs(deg) > 120) *dx = -1;
	if (deg < -30 && deg > -150) *dy = -1; else if (deg > 35 && deg < 145) *dy = 1;
}
static int category(int mode)
{
	switch (mode) {
	case SH_PLAY: case SH_PAUSE: return CAT_GAME;
	case SH_MENU: return CAT_MENU;
	case SH_START: case SH_TITLE: case SH_SCENE: case SH_DEMO: return CAT_ANY;
	}
	return CAT_NONE;
}
static void key_hold(shell_input *in, int scan, int down) { if (held[scan] != down) { held[scan] = (uint8_t)down; shell_input_key(in, scan, down, 0); } }
static void key_type(shell_input *in, int scan, int ascii) { shell_input_key(in, scan, 1, ascii); shell_input_key(in, scan, 0, 0); }
static void release_all(shell_input *in) { for (int s = 0; s < (int)sizeof held; s++) key_hold(in, s, 0); held_dirs = repeat_count = 0; }

int controller_frame(shell_input *in, int mode, int feed)
{
	if (!enabled) return 0;
	/* the controllers' state, combined */
	uint32_t cur = latched; int sx = 0, sy = 0; latched = 0;
	for (int i = 0; i < npads; i++) {
		SDL_GameController *gc = pads[i].gc; uint32_t m = 0;
		for (int b = 0; b < NBUTTONS; b++) if (SDL_GameControllerGetButton(gc, (SDL_GameControllerButton)b)) m |= B(b);
		if (SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > threshold) m |= B(BIT_LTRIGGER);
		if (SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > threshold) m |= B(BIT_RTRIGGER);
		int x = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTX), y = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTY);
		if (m || x > threshold || x < -threshold || y > threshold || y < -threshold) last_used = i;
		if (!sx && !sy) sx = x, sy = y;
		if (sx * (long)sx + sy * (long)sy < x * (long)x + y * (long)y) sx = x, sy = y;   /* (the stick pushed furthest) */
		cur |= m;
	}
	int cat = feed ? category(mode) : CAT_NONE;
	if (cat != prev_cat) { ignored = cur & prev_cur; prev_cat = cat; }   /* (the buttons already down: not a press in this mode) */
	prev_cur = cur;
	ignored &= cur;
	uint32_t active = cur & ~ignored, pressed = active & ~prev_active;
	prev_active = active;
	#define ACT(a) (active & button_mask[a])
	#define PRESSED(a) (pressed & button_mask[a])
	int actions = 0;
	if (!feed) {   /* nothing to the game */
		release_all(in);
		if (PRESSED(BUTTON_INFO)) actions |= CONTROLLER_INFO;
		else if (pressed & ~DPAD & ~B(SDL_CONTROLLER_BUTTON_GUIDE)) actions |= CONTROLLER_CLOSE;
		return actions;
	}
	/* the directions: the D-pad (never ignored) and the stick; while playing, the up / down buttons */
	int dx = 0, dy = 0;
	stick_dirs(sx, sy, cat == CAT_GAME && only_horizontal, &dx, &dy);
	if (cur & B(SDL_CONTROLLER_BUTTON_DPAD_LEFT)) dx = -1; else if (cur & B(SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) dx = 1;
	if (cur & B(SDL_CONTROLLER_BUTTON_DPAD_UP)) dy = -1; else if (cur & B(SDL_CONTROLLER_BUTTON_DPAD_DOWN)) dy = 1;
	if (cat == CAT_GAME && !dy) { if (ACT(BUTTON_UP)) dy = -1; else if (ACT(BUTTON_DOWN)) dy = 1; }
	if (cat == CAT_MENU && dy) dx = 0;   /* (a menu moves one way at a time) */
	int dirs = (dx < 0) | (dx > 0) << 1 | (dy < 0) << 2 | (dy > 0) << 3;
	static const uint8_t dir_scan[4] = { SC_LEFT, SC_RIGHT, SC_UP, SC_DOWN };
	for (int d = 0; d < 4; d++) if (!(dirs >> d & 1)) key_hold(in, dir_scan[d], 0);
	for (int d = 0; d < 4; d++) if (dirs >> d & 1) key_hold(in, dir_scan[d], 1);
	/* menus: a held direction repeats, as the keyboard's does */
	if (cat == CAT_MENU && dirs && dirs == held_dirs) {
		if (++repeat_count >= REPEAT_DELAY && (repeat_count - REPEAT_DELAY) % REPEAT_EVERY == 0)
			for (int d = 0; d < 4; d++) if (dirs >> d & 1) shell_input_key(in, dir_scan[d], 1, 0);
	} else repeat_count = 0;
	held_dirs = dirs;
	/* the held modifiers (while playing only) */
	key_hold(in, SC_LSHIFT, cat == CAT_GAME && ACT(BUTTON_SHIFT));
	key_hold(in, SC_CTRL, cat == CAT_GAME && ACT(BUTTON_CTRL));
	/* the presses */
	uint32_t others = pressed & ~DPAD & ~B(SDL_CONTROLLER_BUTTON_GUIDE);
	if (PRESSED(BUTTON_INFO)) { actions |= CONTROLLER_INFO; others &= ~button_mask[BUTTON_INFO]; }
	switch (cat) {
	case CAT_GAME:
		if (PRESSED(BUTTON_MENU)) { if (mode == SH_PAUSE) key_type(in, SC_ENTER, 0x0D); else key_type(in, SC_ESC, 0x1B); }
		if (PRESSED(BUTTON_RESTART)) { shell_input_key(in, SC_ALT, 1, 0); key_type(in, SC_A, 'a'); if (!held[SC_ALT]) shell_input_key(in, SC_ALT, 0, 0); }
		if (PRESSED(BUTTON_TIME)) key_type(in, SC_SPACE, 0x20);
		if (PRESSED(BUTTON_QUICKSAVE)) actions |= CONTROLLER_QUICKSAVE;
		if (PRESSED(BUTTON_QUICKLOAD)) actions |= CONTROLLER_QUICKLOAD;
		break;
	case CAT_MENU:
		if (others & B(SDL_CONTROLLER_BUTTON_A)) key_type(in, SC_ENTER, 0x0D);
		else if (others & (B(SDL_CONTROLLER_BUTTON_B) | B(SDL_CONTROLLER_BUTTON_BACK) | button_mask[BUTTON_MENU])) key_type(in, SC_ESC, 0x1B);
		else if (others & B(SDL_CONTROLLER_BUTTON_X)) key_type(in, SC_TAB, 0x09);
		else if (others & B(SDL_CONTROLLER_BUTTON_Y)) {
			static const struct { uint8_t scan; char c; } name[6] = { {0x19, 'P'}, {0x13, 'r'}, {0x17, 'i'}, {0x31, 'n'}, {0x2E, 'c'}, {0x12, 'e'} };
			for (int i = 0; i < 6; i++) key_type(in, name[i].scan, name[i].c);
		}
		else if (others) key_type(in, SC_SPACE, 0x20);
		break;
	case CAT_ANY:
		if (others) { if (mode == SH_DEMO) key_type(in, SC_ENTER, 0x0D); else key_type(in, SC_SPACE, 0x20); }
		break;
	}
	#undef ACT
	#undef PRESSED
	return actions;
}

void controller_after_step(int mode, int level, int hp, int quickloaded)
{
	if (enabled && rumble_on && prev_mode == SH_PLAY && mode == SH_PLAY && level == prev_level && !quickloaded && hp < prev_hp) {
		rumbles++;
		int i = last_used >= 0 && last_used < npads ? last_used : npads ? 0 : -1;
		if (i >= 0) SDL_GameControllerRumble(pads[i].gc, RUMBLE_STRENGTH, RUMBLE_STRENGTH, RUMBLE_MS);
	}
	prev_mode = mode; prev_level = level; prev_hp = hp;
}
