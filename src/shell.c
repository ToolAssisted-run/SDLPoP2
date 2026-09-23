/* The DOS program around the game's tick (see shell.h and docs/SHELL.md): 0823:0000 main, 0823:00B2 the main loop,
 * 0823:01CA the title and demo loop, 169B:0006 / 0070 / 03AE / 0504 the game and level loops, 0823:02BE the in-game
 * keys, 0823:0528 the cheat keys, 0823:10A0's pause, 169B:123E the time-out, 0AAC:0274 the scenes, 15DB the recorded
 * demos, and the waits of 2797 / 2631 / 2768 on a clock of video frames. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <ucontext.h>
#include <time.h>
#include "types.h"
#include "globals.h"
#include "glue.h"
#include "core.h"
#include "render.h"
#include "text.h"
#include "loader.h"
#include "menu.h"
#include "shell.h"
#include "nis.h"

extern uint16_t word_2baa;
int load_level_ex(int n, int full);   /* level.c */
void sound_194c_83d2(uint16_t res);   /* sound.c */
extern int (*frame_on_time_hook)(void);   /* glue.c */

/* ---- the command line (194C:2CEA looks a word up by its prefix, case-insensitive) ---- */
static char args[16][64]; static int nargs;
static const char *arg_find(const char *word)
{
	size_t n = strlen(word);
	for (int i = 0; i < nargs; i++) {
		size_t k = 0; while (k < n && args[i][k] && toupper((unsigned char)args[i][k]) == toupper((unsigned char)word[k])) k++;
		if (k == n) return args[i];
	}
	return NULL;
}

/* ---- the program's state outside the game ---- */
static uint32_t seed_value; static int seed_set;
static int running, exit_code, mode = SH_START; static char exit_msg[128]; static char game_dir[400];
uint16_t word_00ec = 1;            /* DS:00EC: the title skips the credits and the hall of fame (1 at start) */
#define word_2ba6 (*(uint16_t *)(tiles0 + 0xC))   /* DS:2BA6: a scene is playing (read by the drawing: 0FB3:22BA, 33FD:145E) */
static int poll_menu;              /* DS:1F32: 0823:10A0 (0) or 0D5E:0390 (1) */
static pop2_config config;         /* DS:[1FB8] */

/* ---- the clock: video frames (70.086 Hz), the 60 Hz timer derived from them ---- */
static uint32_t frames; uint32_t sh_ticks60;   /* DS:24E4 */
static uint64_t tick_acc;
uint16_t sh_timers[4];             /* DS:24DC.. (index 1 is frame_delay, DS:24DE) */
static uint16_t *timer(int i) { return i == 1 ? &frame_delay : &sh_timers[i]; }
static void timer_interrupt(void)   /* 194C:7EE7 (the 60 Hz part) */
{
	sh_ticks60++;
	for (int i = 0; i < 4; i++) if (*timer(i)) (*timer(i))--;
}

/* ---- the coroutine ---- */
static ucontext_t ctx_host, ctx_shell; static char *shell_stack; static int shell_done;
static const shell_input *cur_in; static uint16_t keyq[16]; static int keyq_n;
static void take_input(const shell_input *in)
{
	/* the keyboard interrupt's table (DS:1D0D + scan code) and the BIOS shift flags */
	for (int s = 0; s < 0x59 && s < (int)sizeof in->down; s++) key_table[s + 0xD] = in->down[s];
	bios_shift_flags = in->shift_flags;
	for (int i = 0; i < in->ntyped && i < 8; i++) if (keyq_n < 8) keyq[keyq_n++] = in->typed[i];   /* the 8-deep event queue (194C:99EC) */
}
void sh_frame(void)
{
	if (!running) return;
	swapcontext(&ctx_shell, &ctx_host);
}
static void advance_frame(void)
{
	frames++;
	tick_acc += (uint64_t)60 * 44900;   /* the VGA refresh is 3146875 / 44900 Hz */
	while (tick_acc >= 3146875) { tick_acc -= 3146875; timer_interrupt(); }
}

/* SHELL_TRACE: the shell's steps on stderr, with the frame (for comparing with the oracle's probes) */
void sh_trace(const char *what, int v)
{
	static int on = -1; if (on < 0) on = getenv("SHELL_TRACE") != NULL;
	if (on) fprintf(stderr, "frame=%u shell %s %d (tick %u)\n", (unsigned)frames, what, v, (unsigned)tick);
}

/* ---- keys and waits (2768:02CA, 2797) ---- */
int sh_key(void)   /* 2768:02CA */
{
	if (keyq_n) { int k = keyq[0]; memmove(keyq, keyq + 1, --keyq_n * sizeof *keyq); return k; }
	sh_frame(); return 0;
}
static int key_peek_take(void) { if (!keyq_n) return 0; int k = keyq[0]; memmove(keyq, keyq + 1, --keyq_n * sizeof *keyq); return k; }
void sh_quit(int code, const char *msg)   /* 2797:000A */
{
	exit_code = code; snprintf(exit_msg, sizeof exit_msg, "%s", msg ? msg : "");
	shell_done = 1; mode = SH_QUIT;
	for (;;) sh_frame();   /* (the host sees SHELL_EXIT and stops stepping) */
}
static int key_quit_check(int k) { if (k == 0x1000 || k == 0x11) sh_quit(0, NULL); return k; }   /* 2797:00E2 */
static int poll_menu_keys(void)   /* 0D5E:0390 */
{
	int k = key_peek_take();
	if (!k) return input_device == 2 ? (int8_t)control_shift : 0;
	key_quit_check(k);
	return k == 0x1B || k == 0x20 ? k : 0;
}
int sound_toggle(void);
static int poll_scene_keys(void)   /* 2D7D:49F6, the scenes' poll routine: Esc, space or Alt-L end a scene */
{
	int k = key_quit_check(key_peek_take());
	if (k == 0x1F00) { sound_toggle(); k = 0; }   /* Alt-S works during a scene */
	if (k == 0x2600) { word_1392 = 1; return 1; }  /* Alt-L: restore a saved game after it */
	return k == 0x1B || k == 0x20;                 /* (other keys are dropped; the joystick's button too) */
}
int sh_poll(void)   /* 2797:009C: [DS:1F32] */
{
	if (poll_menu == 1) return poll_menu_keys();
	if (poll_menu == 2) return poll_scene_keys();
	return read_input();   /* 0823:10A0: the keys (0823:02BE) and the controls: a keystroke, else the action button */
}
void sh_set_poll(int menu) { poll_menu = menu; }   /* 0 the game's (0823:10A0), 1 the menus' (0D5E:0390), 2 the scenes' (2D7D:49F6) */
int sh_wait(int ticks, int t)   /* 2797:0158 / 0104 */
{
	*timer(t) = (uint16_t)ticks;
	for (;;) { int k = sh_poll(); if (*timer(t) == 0 || k) return k; sh_frame(); }
}
int sh_wait_key(void) { for (;;) { int k = sh_poll(); if (k) return k; sh_frame(); } }   /* 2797:01A8 */
void sh_wait_timer(int t) { while (*timer(t)) sh_frame(); }   /* 2797:0134 */

/* ---- sounds outside the tick's queue: to the driver through sound.c's hooks (the SDL frontend's audio_request) ---- */
extern void (*sound_start_hook)(int n);   /* sound.c: resource 10000 + n */
void shell_sound(int res) { if (sound_start_hook) sound_start_hook(res - 10000); }   /* 194C:840E / 1611:053C */
void shell_sound_stop(int res) { if (res == 0) sound_stop_all(); else sound_194c_83d2((uint16_t)res); }   /* 194C:83D2 */
void sound_res_start(uint16_t res) { if (running || sound_start_hook) shell_sound(res); }   /* (game.c: 169B:0B9C's beep) */
__attribute__((weak)) void platform_sound_volume(int v) { (void)v; }   /* the frontend: audio_volume */
void sound_set_volume(int v) { sound_volume = (uint8_t)(v > 15 ? 15 : v); platform_sound_volume(sound_volume); }   /* 194C:3380 */
int music_toggle_msg(void)   /* 1611:07D4 -> the message */
{
	if (!(sound_caps & 2)) return 0xA44;   /* "Music Unavailable" */
	amb_state[0] = !amb_state[0];
	options_save();
	return amb_state[0] ? 0xA20 : 0xA32;   /* "Ambient Music On" / "Off" (off also stops the piece, 194C:83D2) */
}
int joystick_toggle_msg(void)   /* 0823:0B1A: no joystick here */
{
	if ((sound_caps & 1) && config.w[4] == 0x20) return 0xA56;   /* "Joystick Unavailable" */
	input_device = 0;
	memset(&joy_y, 0, sizeof joy_y); joy_x = joy_cx = joy_cy = 0; joy_button = 0;   /* (DS:1D04, 9 bytes) */
	return 0xA7A;   /* "Joystick Not Found" */
}
static const char *ds_str(int a)
{
	switch (a) {
	case 0xA0C: return "Sound On"; case 0xA16: return "Sound Off"; case 0xA20: return "Ambient Music On";
	case 0xA32: return "Ambient Music Off"; case 0xA44: return "Music Unavailable"; case 0xA56: return "Joystick Unavailable";
	case 0xA6C: return "Joystick Mode"; case 0xA7A: return "Joystick Not Found"; case 0xA8E: return "Keyboard Mode";
	case 0x7E: return "PRINCE OF PERSIA 2 v1.1";
	}
	return "";
}

/* ---- palettes (0FB3:29B8 / 294C / 293A, 2631) ---- */
static uint8_t game_pal[0x2D0]; static int game_pal_saved;
void pal_game_save(void)   /* 0FB3:29B8: the game's colours 16..255 put aside, black meanwhile */
{
	if (game_pal_saved) return;
	pal_get(game_pal, 0x10, 0xF0); pal_set(NULL, 0x10, 0xF0); game_pal_saved = 1;
}
void pal_game_restore(void) { if (!game_pal_saved) return; pal_set(game_pal, 0x10, 0xF0); game_pal_saved = 0; }   /* 0FB3:294C */
/* 0FB3:2B1C: `count` colours from entry `index` of palette `id` (PALC id: the entry count, PALS id: the colours) to
 * the DAC from `first`; the colours from 16 on go to the put-aside game palette while a menu shows */
static void pal_entry(uint16_t id, int index, int count, int first)
{
	uint16_t n; const uint8_t *c = res_get("PALC", id, &n), *p = res_get("PALS", id, &n);
	if (!c || !p || index >= (c[0] | c[1] << 8) || (index + 1) * count * 3 > n) return;
	const uint8_t *src = p + index * count * 3;
	for (int i = 0; i < count; i++) {
		int k = first + i;
		if (k >= 16 && game_pal_saved) memcpy(game_pal + (k - 16) * 3, src + i * 3, 3); else pal_set(src + i * 3, k, 1);
	}
}
void pal_std16(void) { pal_entry(10, 0, 16, 0); }   /* 0FB3:293A: colours 0..15 from PALS 10 */
void render_after_menu(uint8_t kind) { pal_entry(25001, kind - 1, 16, 16); }   /* 1286:07CE: colours 16..31, KID.DAT's PALS 25001 entry kind - 1 */
/* 2631:037E: 64 steps, one per video frame (each waits for the retrace), from the current DAC to target (NULL:
 * black) for the 16-colour banks in mask; the poll routine's key stops it */
int sh_fade(int delay, uint16_t mask, const uint8_t *target)
{
	uint8_t from[768], to[768], cur[768];
	pal_get(from, 0, 256); memcpy(to, from, 768);
	for (int b = 0; b < 16; b++) if (mask & (1u << b)) { if (target) memcpy(to + b * 48, target + b * 48, 48); else memset(to + b * 48, 0, 48); }   /* (the target is read as a whole DAC) */
	if (!memcmp(from, to, 768)) return 0;
	int key = 0;
	for (int step = 1; step <= 64; step++) {
		sh_timers[3] = (uint16_t)delay;
		memcpy(cur, from, 768);
		for (int b = 0; b < 16; b++) if (mask & (1u << b))
			for (int i = b * 48; i < b * 48 + 48; i++) { int d = (to[i] - from[i]) * step; cur[i] = (uint8_t)(from[i] + (d < 0 ? -((-d) >> 6) : d >> 6)); }
		pal_set(cur, 0, 256);
		sh_frame();
		if (step == 64) break;
		key = sh_poll(); if (key) break;
		while (sh_timers[3]) sh_frame();
	}
	return key;
}
/* 2A31:0CE5 -> 33B9:0000 (mode 1): the dissolve from an offscreen port over dur ticks. Each step copies the pixels whose
 * byte of the LCG x*5+1 (low byte, high byte, next; seed DS:1F1E = 0x4583 again each step) is <= the level, which
 * grows with the ticks since the start (194C:13E6, 33B9:0300). One step a frame here; the original takes as many as
 * the machine manages (nis.c models that for the scenes). */
int sh_transition(int dur, gport *src, const qrect *r)
{
	uint32_t start = sh_ticks60; int key = 0;
	for (;;) {
		key = sh_poll();
		uint32_t el = sh_ticks60 - start; int level = (int)((uint64_t)el * 255 / (uint32_t)dur);
		if (el >= (uint32_t)dur) { gfx_copy_bits(src, &port_screen, r, r); break; }
		if (level < 1) level = 1;
		uint16_t x = 0x4583; int phase = 0;
		for (int v = r->top; v < r->bottom; v++)
			for (int h = r->left; h < r->right; h++) {
				uint8_t b;
				if (phase == 0) b = x & 0xFF; else if (phase == 1) b = x >> 8; else { x = (uint16_t)(x * 5 + 1); phase = 0; b = x & 0xFF; }
				phase++;
				if (b <= level) screen_buf[v * SCREEN_W + h] = src->bits[(v - src->origin_v) * SCREEN_W + (h - src->origin_h)];
			}
		if (key) break;
		sh_frame();
	}
	return key;
}
__attribute__((weak)) void render_frame(void) {}           /* the renderer's frame (169B:0A30's drawing) */
__attribute__((weak)) void render_redraw_all(void) {}      /* 169B:0430 */
/* the renderer draws from the game state; what the drawing itself changes in the game is modelled by the core (game.c's
 * draw_*_state), so the game state is kept as it was around the renderer's calls */
static void draw(void (*f)(void))
{
	static uint8_t *save; static size_t n;
	if (!save) { n = pop2_state_size(); save = malloc(n); }
	pop2_save(save); f(); pop2_load(save);
}

/* ---- scenes (0AAC:0274) ---- */
static int cur_scene;
int shell_scene(void) { return mode == SH_SCENE ? cur_scene : 0; }
static void nis_sound(int kind, int id, void *u)   /* the scenes' sounds to the driver (sound.c's hooks) */
{
	(void)u;
	if (kind == NIS_SND_STOP) shell_sound_stop(id); else shell_sound(id);
}
/* 0AAC:0376 for transitions 2 and 3: the level loaded and the room drawn into the scene's picture; the game state is
 * put back after (the level that follows the scene is loaded again anyway) */
__attribute__((weak)) void shell_nis_room(int lv, int room, uint8_t *pixels, int rowbytes)
{
	static uint8_t *save; if (!save) save = malloc(pop2_state_size());
	pop2_save(save);
	uint8_t kind = level_kind;
	word_2b96 = 0;                                   /* 169B:018E */
	if (load_level_ex(lv, 1)) {                      /* 1286:02EE / 00A2 / 043A / 03B6 / 0592 */
		drawn_room = 0; next_room = (uint8_t)room;
		switch_room();                               /* 0823:0E72(1) */
		render_redraw_all();                         /* 169B:0430 */
		for (int y = 0; y < 192; y++) memcpy(pixels + y * rowbytes, screen_buf + y * SCREEN_W, SCREEN_W);
	}
	pop2_load(save); level_kind = kind;
}
static void nis_room(int lv, int room, uint8_t *pixels, int rowbytes, void *u) { (void)u; shell_nis_room(lv, room, pixels, rowbytes); }
int sh_scene(int n)
{
	int result = 1, kind = level_kind; word_2ba6 = 1;
	sh_trace("scene", n);
	if (n != 100) {
		if (n < 0x14 && n != 6) checkpoint_free();   /* 1286:0D7C(0), 0D5E:1094 */
		port_free(port_back); port_back = NULL;
	}
	if (n != 1 && n != 100 && n != 6) { pal_set(NULL, 0, 256); the_port = &port_screen; gfx_erase_rect(&rect_screen); }   /* 0AAC:0080 (its other test reads an uninitialised word) */
	if (n == 100) copy_protection();
	else {
		int prev = mode; mode = SH_SCENE;
		/* (nis.c derives its 60 Hz ticks from its frames, at the same nominal rate as the shell's clock) */
		nis_set_sound_callback(nis_sound, NULL); nis_set_room_hook(nis_room, NULL);
		if (n == 6) { nis_kid k = { (int8_t)Kid.direction, Kid.x, Kid.y, char_x_left }; nis_set_kid(&k); }   /* 0AAC:0442 (DS:5B37 / 5B38 / 5B3A, DS:6116) */
		cur_scene = n;
		if (nis_open(game_dir, n)) {
			int aborted = 0, save_poll = poll_menu; poll_menu = 2;   /* 2D7D:4A6A: DS:1F32 = 2D7D:49F6 while it plays */
			for (;;) {
				if (!nis_step(screen_buf, render_palette)) break;
				sh_frame();
				if (!aborted && sh_poll()) { nis_abort(); aborted = 1; }
			}
			nis_close(); poll_menu = save_poll;
			if (aborted && n < 0x14) result = 2;   /* (NIS_INTRO = -1: the intro's three) */
		}
		mode = prev;
		if (n >= 1 && n <= 6) kind = n == 1 ? 4 : n == 2 ? 2 : n == 3 ? 6 : kind;
	}
	if (n != 1 && n != 100 && n != 2 && n != 3 && n != 6) { the_port = &port_screen; gfx_erase_rect(&rect_screen); }   /* 0AAC:0050 */
	pal_std16();
	render_after_menu((uint8_t)kind);
	word_2ba6 = 0;
	return result;
}

/* ---- the demo player (15DB) ---- */
static struct { uint16_t mode, pos, sub, slot[6]; const uint8_t *res; uint16_t size; uint16_t id; } demo;   /* DS:2AC4.. */
static int16_t rd16(const uint8_t *p) { return (int16_t)(p[0] | p[1] << 8); }
static int tick_before(int16_t w) { int32_t v = w; return (uint32_t)v < tick; }   /* the word sign-extended, compared unsigned with DS:5D04 */
static void demo_apply(uint16_t w)   /* 15DB:01BC */
{
	ctrl1_forward = (int8_t)((w & 3) - 1); ctrl1_backward = (int8_t)(((w >> 2) & 3) - 1);
	ctrl1_up = (int8_t)(((w >> 4) & 3) - 1); ctrl1_down = (int8_t)(((w >> 6) & 3) - 1);
	ctrl1_shift = (int8_t)(((w >> 8) & 0x1F) - 2);
}
void ovl_15db_64(void)   /* 15DB:0064: in the control of the prince and of each character (a demo only) */
{
	if (demo.mode != 3 || !demo.res) return;
	int who = Char.index == 10 ? 5 : Char.index;
	const uint8_t *d = demo.res;
	if (rd16(d + 3) > (int16_t)demo.pos) {
		const uint8_t *rec = d + demo.pos + 5;
		if (tick_before(rd16(rec))) {   /* 15DB:020A: the next record */
			demo.pos += (uint16_t)(((int8_t)rec[2] + 1) * 3); demo.sub = 0;
			if (demo.mode == 3 && rd16(d + 3) < (int16_t)demo.pos) sh_quit(0, "Error playing recorded game");
			rec = d + demo.pos + 5;
		}
		int32_t t = rd16(rec);
		if ((uint32_t)t == tick && (int8_t)rec[2] > (int16_t)demo.sub && rec[3 + demo.sub * 3] == who) {
			uint16_t w = (uint16_t)(rec[4 + demo.sub * 3] | rec[5 + demo.sub * 3] << 8);
			demo_apply(w); demo.slot[who] = w; demo.sub++;
		}
	}
	demo_apply(demo.slot[who]);
}
int demo_timing_check(void)   /* 15DB:000C */
{
	if (demo.mode == 3 && demo.res && tick_before(rd16(demo.res + 1))) { demo.mode = 0; demo.res = NULL; return -1; }
	return -2;
}
static void demo_start(void)   /* 15DB:0278 (at the level's start) */
{
	if (demo.mode != 4 && demo.mode != 3) return;
	demo.mode = 3; demo.pos = demo.sub = 0; memset(demo.slot, 0, sizeof demo.slot);
	demo.res = word_2ba8 ? res_get("", demo.id, &demo.size) : NULL;
	tick = 0;
	random_seed = 1;             /* 2751:00D6(1) */
	random_2751(0x11);           /* 2751:008C(0x11) */
	checkpoint_free();
}
static void demo_end_level(void) { if (demo.mode == 4) word_32d8 = 0xFF; }   /* 15DB:0148 */
static void demo_stop(void) { word_2ba8 = 0; demo.mode = 0; demo.res = NULL; }   /* 15DB:02F8 */
static void demo_arm(void)   /* 15DB:0316: the next of the three recordings */
{
	demo.mode = 4;
	if (demo.id == 0 || ++demo.id > 0x1B) demo.id = 0x19;
	uint16_t n; const uint8_t *d = res_get("", demo.id, &n);
	word_2ba8 = 1; word_2baa = 0;
	byte_6b6c = d ? d[0] : 0;
}
static int demo_toggle(void) { demo.mode = demo.mode == 4 ? 0 : 4; return demo.mode != 0; }   /* 15DB:02E0 (cheat F3) */

/* ---- the in-game keys (0823:02BE, with 0823:10A0's pause and 0823:0528's cheats) ---- */
extern int hotkeys_02be_core(void);   /* input.c: the core's version (tests, pop2_frame) */
static void message(const char *s) { status_message(s); word_5cdc = word_5cda = 0x18; }
static void cheat_keys(int di)   /* 0823:0528 (the gameplay ones; the debug displays are left out) */
{
	char t[40];
	switch (di) {
	case 0x2B: minutes_left++; word_5cdc = word_5cda = 0; word_5cd0 = 1; break;   /* '+' */
	case 0x2D: if (minutes_left > 1) minutes_left--; word_5cdc = word_5cda = 0; word_5cd0 = 1; break;   /* '-' */
	case 0x49: toggle_upside_down_pub(); break;   /* 'I' */
	case 0x52: snprintf(t, sizeof t, "Room %d", drawn_room); message(t); break;   /* 'R' */
	case 0x54: sound_1611_01a8(0x65); { int m = (int8_t)Char.f13 + 1; Char.f13 = (uint8_t)(m > 12 ? 12 : m); Char.hp_delta = (int8_t)(Char.f13 - Char.f12); } Kid = Char; break;   /* 'T' (0823:0F16) */
	case 0x57: word_5d36 = 0xE4; sound_1611_01a8(0x69); sound_stop_all(); word_087e = -1; break;   /* 'W' (0823:13C4) */
	case 0x72: if ((int8_t)Kid.alive > 0) { word_5ce8 = 0x14; Kid.alive = -1; status_clear(1); } break;   /* 'r' */
	case 0x3D00: message(demo_toggle() ? "PLAYER ON" : "PLAYER OFF"); break;   /* F3 */
	}
}
static void pause_game(void)   /* 0823:10C4 */
{
	word_2b96 = 0;
	int prev = mode; mode = SH_PAUSE;
	status_message("GAME PAUSED");
	shell_sound_stop(0);   /* 194C:83D2(0) */
	sh_wait_key();
	status_clear(1);
	mode = prev;
}
int hotkeys_02be(void)
{
	if (!running) return hotkeys_02be_core();
	int di = key_quit_check(key_peek_take()), restart = 0, msg = 0;
	if (di) sh_trace("key", di);
	if (control_shift != 0 || di != 0)
		if ((minutes_left != 0 && Kid.alive > 6) || word_2ba8) { restart = 1; if (word_2ba8) word_2baa = 1; }
	if (di) {
		switch (di) {
		case 0x1B: word_2b96 = 1; break;                                    /* Esc: pause */
		case 0x20: if (byte_016a >= 0) word_5cd0 = 1; break;                /* space: the time left */
		case 0x1300: byte_6b6c = 0; byte_016a = -1; if (!level_end_sound_playing()) restart = 1; break;   /* Alt-R: to the title */
		case 0x1E00: if (!level_end_sound_playing()) restart = 1; break;    /* Alt-A: restart the level */
		case 0x1F00: msg = sound_toggle() ? 0xA0C : 0xA16; break;           /* Alt-S */
		case 0x1800: if (menu_allowed() && !word_2ba8) { options_menu(); loadkid(); } break;   /* Alt-O */
		case 0x2200: if (menu_allowed() && menu_can_save()) { game_save(); loadkid(); } break;   /* Alt-G */
		case 0x2300: if (menu_allowed()) hall_of_fame(1); break;             /* Alt-H */
		case 0x2400: msg = joystick_toggle_msg(); break;                     /* Alt-J */
		case 0x2500: msg = 0xA8E; input_device = 0; break;                   /* Alt-K */
		case 0x2600: if (word_2ba8) word_1392 = 1; else if (menu_allowed()) { game_restore(); loadkid(); } break;   /* Alt-L */
		case 0x2F00: msg = 0x7E; break;                                      /* Alt-V */
		case 0x3100:                                                         /* Alt-N: the next level (up to 3 without the cheat word) */
			if ((int8_t)word_32d8 > 3 && !cheat_mode) break;
			if ((int8_t)word_32d8 == 14 && cheat_mode) counter_5cec = 1;
			else { counter_5cec = (uint16_t)((int8_t)word_32d8 + 1); if (!cheat_mode && minutes_left > 15) { minutes_left = 15; clock_ticks = 0x2CF; } }
			sound_stop_all(); break;
		case 0x3200: msg = music_toggle_msg(); break;                        /* Alt-M */
		}
		if (cheat_mode) cheat_keys(di);
		if (msg) message(ds_str(msg));
	}
	if (restart && !word_5ce8 && !(drawn_room == 4 && level_number == 13 && shadow13_present())) { word_5cd8 = 1; shell_sound_stop(0); }
	if (word_2b96) pause_game();   /* (0823:10BF, right after this in read_input) */
	return di;
}

/* ---- the status line where the game shows its messages (game.c / kidctl.c: shell_status) ---- */
void shell_status(int op)
{
	switch (op) {
	case 0: time_message(); break;        /* 0823:0E58 -> 0FB3:204C */
	case 1: status_clear(1); break;       /* 0FB3:2136(1) */
	case 2: status_clear(0); break;       /* 0FB3:2136(0) */
	case 3: status_press_key(); break;    /* 0FB3:20A4 */
	}
}
/* the program's mode for the frontend (menus set SH_MENU while they show) */
int sh_enter_mode(int m) { int old = mode; mode = m; return old; }
void sh_leave_mode(int old) { mode = old; }

/* ---- 169B:123E: out of time ---- */
void restart_prompt(void)
{
	sound_stop_all();
	if (!running) return;
	int r = sh_scene(0x1C);
	minutes_left = 0; byte_016a = 8;
	if (r == 2) word_2b96 = 0;   /* 169B:018E */
}

/* ---- the game (169B:0006 / 0070 / 03AE / 0504) ---- */
extern int last_scene; extern uint16_t word_5cce;
int load_level_ex(int n, int full);
static int frame_on_time_shell(void) { return frame_delay != 0; }
static int first_room(int n)   /* 169B:03AE */
{
	(void)n;
	int r = level_first_room();   /* (0823:0E72, status line, 169B:0430, hit points) */
	draw(render_redraw_all);
	frame_delay = 5; sh_wait_timer(1);   /* DS:24DE = 5, pumping keys (0823:12FA) */
	return r;
}
void (*shell_tick_hook)(void);   /* tests: called where the oracle's 169B:05E0 probe fires (after frame_begin) */
void frame_wait(void);
static int play_loop(void)   /* 169B:0504 */
{
	for (;;) {
		frame_begin();                               /* 169B:0BA6 */
		if (shell_tick_hook) shell_tick_hook();
		int r = frame_after_tick(tick_main());      /* 169B:05E0 .. 0A30 */
		frame_wait();                                /* 18C8:0008 */
		draw(render_frame);
		if (r != -2) return r;
		if (frame_delay) sh_wait_timer(1);   /* on time: 2797:0134(1) */
	}
}
static int level_loop(int n)   /* 169B:0070 */
{
	while (n != 0 && n != -1 && n <= 14) {
		int full = 0;
		if (!word_5cb6) {
			int si = story_scene((int8_t)word_32d8, n);   /* 0AAC:000E / 0120 */
			if (si) full = sh_scene(si) != 0;
			if (word_1392) { uint8_t save = byte_6b6c; byte_6b6c = (uint8_t)n; int r = game_restore(); word_1392 = 0; if (r > 0) { byte_6b6c = (uint8_t)word_32d8; n = (int8_t)word_32d8; word_5cd8 = 0; } byte_6b6c = save; full = 1; }
			if (n != (int8_t)word_32d8) full = 1;
		} else full = 1;
		if (!load_level_ex(n, full)) return -1;
		level_begin();
		mode = word_2ba8 ? SH_DEMO : SH_PLAY;
		n = first_room(n);
		if (n == -1) break;
		if (word_2ba8) demo_start();
		n = play_loop();
		if (word_2ba8) { demo_end_level(); word_32d8 = 0; n = 0; counter_5cec = 0; }
	}
	return n;
}
static int game(int n)   /* 169B:0006 */
{
	game_start();
	return level_loop(n);
}
static int restore_from_title(void)   /* 0823:028C */
{
	int r = byte_6b6c;
	if (game_restore() > 0) { byte_6b6c = (uint8_t)word_32d8; r = byte_6b6c; word_5cd8 = 0; }
	word_1392 = 0;
	return r;
}
static int title(void)   /* 0823:01CA */
{
	mode = SH_TITLE;
	int si = 1;
	if (word_00ec == 0) { if (word_2baa || title_credits() || hall_of_fame(0)) si = 2; }
	else word_00ec = 0;
	if (si != 2) si = sh_scene(NIS_INTRO);   /* 0AAC:0274(7), (4), (8): nis.c plays them as one (the music runs on) */
	if (si == 2) { byte_6b6c = 1; word_00ec = 1; demo_stop(); } else demo_arm();
	int r = byte_6b6c;
	word_2b96 = 0;   /* 169B:018E */
	if (word_1392) r = restore_from_title();
	return r;
}
static void main_loop(void)   /* 0823:00B2 */
{
	int local = (int8_t)byte_6b6c;
	for (;;) {
		if (cheat_mode && (arg_find("NIS") || arg_find("TREE"))) for (;;) sh_scene(story_scene((int8_t)word_32d8, 0));   /* (0AAC:000E(0, 0) for ever) */
		sound_stop_all(); word_32d8 = 0xFF; word_5cc0 = 0;   /* 0823:13E6 (with 0993:075E) */
		if (byte_6b6c == 0) { local = title(); continue; }
		local = (int8_t)game(local);
		if (word_2ba8) { local = 0; level_number = 0; }
		if (minutes_left == 0) { local = 0; byte_6b6c = 0; continue; }
		if (local > 14) { sh_scene(0xB); hall_of_fame_enter(minutes_left); local = 0; byte_6b6c = 0; continue; }   /* the end */
		if (byte_6b6c == 0 || local != 0) continue;
		local = level_number; byte_6b6c = (uint8_t)local;
	}
}
static void shell_main(void)   /* 0823:0000 */
{
	/* 2D3E:019A: configuration, resource files, the cheat word */
	config_load(&config);
	res_open("PRINCE.DAT");
	input_device = 0;              /* (a joystick only when one answers: 26B5:000C) */
	if (config.w[3] == -1) sound_set_volume(0);
	sound_init_ambient();          /* 1611:02AC(DS:2B98) */
	pal_std16();                   /* 0FB3:293A */
	const char *cheat = txt4_get(10, NULL);   /* "YIPPEEYAHOO" */
	cheat_mode = cheat && arg_find(cheat) ? 1 : 0;
	options_load();                /* 0D5E:2166 */
	byte_6b6c = 0;                 /* 0823:0192: LEVELn with the cheat word */
	const char *lv = cheat_mode ? arg_find("LEVEL") : NULL;
	if (lv) { int v = atoi(lv + 5); byte_6b6c = (uint8_t)(v < 1 || v > 14 ? 1 : v); level_switch = 1; }
	res_open("KID.DAT"); res_open("SEQUENCE.DAT");   /* DS:0598 */
	random_seed = seed_set ? seed_value : (uint32_t)time(NULL);   /* 2751:00D6(0): the C library's time() */
	random_2751(1);                /* 2751:008C(1) */
	level_kind = 0;
	main_loop();
}

/* ---- the host side ---- */
void shell_set_seed(uint32_t seed) { seed_value = seed; seed_set = 1; }
static void entry(void) { shell_main(); shell_done = 1; for (;;) swapcontext(&ctx_shell, &ctx_host); }
int shell_init(const char *dir, int argc, const char **argv)
{
	snprintf(game_dir, sizeof game_dir, "%s", dir);
	if (!pop2_init(dir)) return 0;
	nargs = 0; for (int i = 0; i < argc && nargs < 16; i++) snprintf(args[nargs++], sizeof args[0], "%s", argv[i]);
	pop2_reset_state();           /* the data segment as the program starts */
	byte_6b6c = 0; cheat_mode = 0; level_switch = 0; word_32d8 = 0xFF;
	frame_on_time_hook = frame_on_time_shell;
	if (!shell_stack) shell_stack = malloc(1 << 20);
	getcontext(&ctx_shell);
	ctx_shell.uc_stack.ss_sp = shell_stack; ctx_shell.uc_stack.ss_size = 1 << 20; ctx_shell.uc_link = NULL;
	makecontext(&ctx_shell, entry, 0);
	running = 1; shell_done = 0; frames = 0; sh_ticks60 = 0; tick_acc = 0;
	return 1;
}
int shell_step(const shell_input *in)
{
	if (shell_done) return SHELL_EXIT;
	cur_in = in; if (in) take_input(in);
	advance_frame();
	swapcontext(&ctx_host, &ctx_shell);
	return shell_done ? SHELL_EXIT : SHELL_RUNNING;
}
/* for frontends: a key goes down or up; the BIOS shift flags follow the modifier keys, and a key going down types the
 * code DOS would give: Alt+key the scan code << 8, Ctrl+letter 1..26, a character its ASCII code, others scan << 8 */
void shell_input_key(shell_input *in, int scan, int down, int ascii)
{
	if (scan <= 0 || scan >= (int)sizeof in->down) return;
	in->down[scan] = (uint8_t)(down != 0);
	in->shift_flags = (uint8_t)((in->down[0x36] ? 1 : 0) | (in->down[0x2A] ? 2 : 0) | (in->down[0x1D] ? 4 : 0) | (in->down[0x38] ? 8 : 0));
	if (!down || scan == 0x2A || scan == 0x36 || scan == 0x1D || scan == 0x38 || in->ntyped >= 8) return;
	int code = in->down[0x38] ? scan << 8 : in->down[0x1D] && ascii >= 'a' && ascii <= 'z' ? ascii & 0x1F : ascii ? ascii : scan << 8;
	in->typed[in->ntyped++] = (uint16_t)code;
}
/* the PC scan code (set 1) of a USB HID keyboard usage (= SDL_Scancode); 0 when the DOS game has no such key */
int shell_pc_scancode(int hid)
{
	static const uint8_t letters[26] = {30,48,46,32,18,33,34,35,23,36,37,38,50,49,24,25,16,19,31,20,22,47,17,45,21,44};
	if (hid >= 4 && hid <= 29) return letters[hid - 4];
	if (hid >= 30 && hid <= 39) return hid - 28;          /* 1..9, 0 */
	if (hid >= 58 && hid <= 67) return hid + 1;           /* F1..F10 */
	switch (hid) {
	case 40: return 0x1C; case 41: return 0x01; case 42: return 0x0E; case 43: return 0x0F; case 44: return 0x39;
	case 45: return 0x0C; case 46: return 0x0D; case 47: return 0x1A; case 48: return 0x1B; case 49: return 0x2B;
	case 51: return 0x27; case 52: return 0x28; case 53: return 0x29; case 54: return 0x33; case 55: return 0x34; case 56: return 0x35;
	case 57: return 0x3A; case 68: return 0x57; case 69: return 0x58; case 71: return 0x46;
	case 73: return 0x52; case 74: return 0x47; case 75: return 0x49; case 76: return 0x53; case 77: return 0x4F; case 78: return 0x51;
	case 79: return 0x4D; case 80: return 0x4B; case 81: return 0x50; case 82: return 0x48; case 83: return 0x45;
	case 84: return 0x35; case 85: return 0x37; case 86: return 0x4A; case 87: return 0x4E; case 88: return 0x1C;
	case 89: return 0x4F; case 90: return 0x50; case 91: return 0x51; case 92: return 0x4B; case 93: return 0x4C;
	case 94: return 0x4D; case 95: return 0x47; case 96: return 0x48; case 97: return 0x49; case 98: return 0x52; case 99: return 0x53;
	case 224: case 228: return 0x1D; case 225: return 0x2A; case 229: return 0x36; case 226: case 230: return 0x38;
	}
	return 0;
}
int shell_mode(void) { return mode; }
int shell_exit_code(void) { return exit_code; }
const char *shell_exit_message(void) { return exit_msg[0] ? exit_msg : NULL; }
uint32_t shell_frame_count(void) { return frames; }
