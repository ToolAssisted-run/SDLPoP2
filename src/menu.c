/* The screens outside the game (segment 0D5E, with the line editor 25BF): saved games, options, hall of fame, copy
 * protection, title credits. Transcribed from the DOS program; the drawing goes through text.c (the 194C library's
 * primitives), the waits and keys through shell.c. See menu.h and docs/SHELL.md. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "globals.h"
#include "text.h"
#include "loader.h"
#include "shell.h"
#include "menu.h"
#include "glue.h"

uint16_t word_1392;           /* DS:1392 */
uint8_t sound_volume = 0xF;   /* DS:2086 */
extern int  checkpoint_get_block(uint8_t *b);           /* level.c: the checkpoint as its DS:5AB2 block (0: none) */
extern void checkpoint_put_block(const uint8_t *b);
extern uint16_t word_2baa;
void sound_set_volume(int v);  /* shell.c: 194C:3380 */
void pal_game_save(void); void pal_game_restore(void); void pal_std16(void);   /* shell.c: 0FB3:29B8 / 294C / 293A */
void render_after_menu(uint8_t kind);   /* shell.c: 1286:07CE (the prince's colours for the level kind) */
void sh_trace(const char *what, int v);
void room_unload_pub(void);   /* roomhooks.c: 0CD6:073A */

/* ---- the menu screen (0D5E:04B2 / 02D4 / 03DC / 022E / 02B4 / 0534) ---- */
int sh_enter_mode(int m); void sh_leave_mode(int old);   /* shell.c */
static int menu_prev_mode = -1;
static void menu_enter(void)   /* 0D5E:04B2 */
{
	sh_trace("menu_enter", 0);
	if (menu_prev_mode < 0) menu_prev_mode = sh_enter_mode(SH_MENU);
	shell_sound_stop(0);                     /* 194C:83D2(0) */
	pal_game_save();                         /* 0FB3:29B8 */
	sh_set_poll(1);                          /* DS:1F32 = 0D5E:0390 */
	the_port = &port_screen;
	status_erase_line();                     /* 0FB3:2104 */
	gfx_text_font(11); port_screen.bg = 0xE;
	port_free(port_back); port_back = port_new(&rect_screen); the_port = port_back;
	gfx_text_font(11); gfx_erase_rect(&rect_screen);
}
static void menu_leave(int back_to_room)   /* 0D5E:02D4 */
{
	sh_trace("menu_leave", 0);
	if (menu_prev_mode >= 0) { sh_leave_mode(menu_prev_mode); menu_prev_mode = -1; }
	port_free(port_back); port_back = port_new(&rect_game);   /* 2699:000E(DS:097E): the game's offscreen buffer */
	the_port = &port_screen; gfx_text_font(0); port_screen.bg = 0; port_screen.fg = 0xF;
	gfx_erase_rect(&rect_screen);
	pal_std16();                             /* 0FB3:293A */
	if (back_to_room && !word_1392) {
		for (int i = 0; i < 5; i++) chars[i].f26 = 0;   /* 0993:118C */
		switch_room();                       /* 0823:0E72(0) */
		word_5cce = 1;
	}
	pal_game_restore();                      /* 0FB3:294C */
	render_after_menu(level_kind);           /* 1286:07CE */
	sh_set_poll(0);                          /* DS:1F32 = 0823:10A0 */
}
static int set_8000(void) { return gfx_shape_set(8000, 1, 1) ? 8000 : 0; }   /* 0D5E:0534: 26BC:0034(1, 1, 8000) */
static void menu_background(uint16_t set)   /* 0D5E:03DC: the frame images 1..3 of set 8000 and the red inside */
{
	int loaded_here = set == 0;
	if (loaded_here) set = (uint16_t)set_8000();
	gfx_draw_shape(set, 1, 0, 0, 0);         /* 26BC:0876 (mode 0) */
	gfx_draw_shape(set, 2, 0xC0, 0, 0);
	gfx_draw_shape(set, 3, 8, 0, 0);         /* 2583:0006, then mirrored (0823:1447) on the right */
	gfx_draw_shape(set, 3, 8, 0x138, 1);
	qrect r = rect_inset(rect_screen, 9, 9); gfx_fill_rect(0xC, &r);
	(void)loaded_here;                       /* (194C:7F28 releases the set) */
}
static int menu_fade_in(void)   /* 0D5E:022E: the offscreen port to the screen, banks 0-1 from black */
{
	sh_trace("menu_fade_in", 0);
	uint8_t save[0x60]; pal_get(save, 0, 0x20); pal_set(NULL, 0, 0x20);
	gfx_copy_bits(port_back, &port_screen, &rect_screen, &rect_screen);
	int key = sh_fade(0, 3, save);
	if (key) pal_set(save, 0, 0x20);
	return key;
}
static int menu_fade_out(void)   /* 0D5E:02B4 */
{
	sh_trace("menu_fade_out", 0);
	int key = sh_fade(0, 3, NULL);
	gport *p = the_port; gfx_erase_rect(&rect_screen); (void)p;
	return key;
}

/* ---- the line editor (25BF): the name fields ---- */
typedef struct line_edit {
	qrect view;          /* +0C (the destination +04 is the same rectangle here) */
	int pos, len, max;   /* +1A cursor, +2C length, +2A the most characters (0x18) */
	int caret_v, caret_h, caret_on;   /* +1C / +1E / +20 */
	uint32_t blink_t;    /* +22 */
	char *text;          /* +26 */
	gport *port;         /* +00 */
} line_edit;
extern uint32_t sh_ticks60;   /* shell.c: DS:24E4, the 60 Hz tick count */
static void edit_draw_text(line_edit *e)   /* 0D5E:000C (the fields' draw routine, +2E) */
{
	int16_t just[2] = {0, -1};
	text_shadowed(just, &e->view, NULL, the_port->bg != 0xC, 10, e->text);
}
static void edit_caret(line_edit *e, int show)   /* 25BF:00A0 */
{
	qrect save = the_port->clip; the_port->clip = e->view;   /* 194C:4C34 */
	int fg = the_port->fg;
	e->caret_on = show;
	if (!show) the_port->fg = the_port->bg;
	qrect l = { (int16_t)e->caret_v, (int16_t)e->caret_h, (int16_t)(e->caret_v + (e->view.bottom - e->view.top) + 2), (int16_t)(e->caret_h + 1) };
	gfx_fill_rect(the_port->fg, &l);        /* 194C:5038: a vertical line down from the pen */
	the_port->fg = fg;
	if (!show) edit_draw_text(e);
	the_port->clip = save;                   /* 194C:4C9A */
}
static void edit_place(line_edit *e) { e->caret_h = gfx_text_width(e->text, e->pos) + e->view.left; }   /* 25BF:0026 */
static void edit_redraw(line_edit *e)    /* 25BF:0690 */
{
	qrect save = the_port->clip; the_port->clip = e->view;
	gfx_erase_rect(&e->view); edit_draw_text(e);
	the_port->clip = save;
}
static void edit_new(line_edit *e, char *text, int max, const qrect *r)   /* 25BF:0588 / 0650 */
{
	memset(e, 0, sizeof *e);
	e->port = the_port; e->view = *r; e->max = max; e->caret_v = r->top; e->caret_h = r->left - 1;
	e->blink_t = sh_ticks60; e->text = text; e->len = (int)strlen(text); e->pos = 0;
	edit_place(e);
}
static void edit_blink(line_edit *e)   /* 25BF:01D8: the caret blinks every half DS:24DA (60) ticks */
{
	if (sh_ticks60 - e->blink_t > 30) { edit_caret(e, !e->caret_on); e->blink_t = sh_ticks60; }
}
static int edit_printable(int c) { return c >= 0x20 && c < 0x7F; }   /* the C library's ctype (DS:234B & 0x57) */
static int edit_key(line_edit *e, int key)   /* 25BF:0236: -> the key when the editor did not take it */
{
	int beep = 0, changed = 0, left = 0;
	if (key != 0 && (key & 0xFF00) == 0) {
		edit_caret(e, 0);
		if (edit_printable(key) && gfx_text_width((char *)&key, 1) != 0) {   /* (a character the font has) */
			if (e->len != e->max && gfx_text_width((char *)&key, 1) + gfx_text_width(e->text, e->len) <= e->view.right - e->view.left) {
				memmove(e->text + e->pos + 1, e->text + e->pos, e->len - e->pos + 1);
				e->text[e->pos] = (char)key; e->len++; e->pos++; edit_place(e); changed = 1;
			} else beep = 1;
			goto done;
		}
	}
	switch (key) {
	case 0x4B00: if (e->pos == 0) beep = 1; else { e->pos--; edit_place(e); } break;   /* left */
	case 8: if (e->pos == 0) beep = 1; else { e->pos--; memmove(e->text + e->pos, e->text + e->pos + 1, e->len - e->pos); e->len--; edit_place(e); changed = 1; } break;   /* backspace */
	case 0x4700: case 0x4800: case 0x4900: e->pos = 0; edit_place(e); break;   /* Home, up, PgUp */
	case 0x4D00: if (e->pos == e->len) beep = 1; else { e->pos++; edit_place(e); } break;   /* right */
	case 0x4F00: case 0x5000: case 0x5100: e->pos = e->len; edit_place(e); break;   /* End, down, PgDn */
	case 0x5300: if (e->pos == e->len) beep = 1; else { memmove(e->text + e->pos, e->text + e->pos + 1, e->len - e->pos); e->len--; changed = 1; } break;   /* Del */
	default: left = key;
	}
done:
	if (changed) edit_redraw(e); else if (beep) shell_sound(0xFFFE);   /* 194C:840E(0xFFFE) */
	if (key) edit_caret(e, 1);
	return left;
}

/* ---- saved games (0D5E:0544 .. 0FB3) ---- */
int menu_allowed(void) { return (minutes_left != 0 || clock_ticks != 0) && mob_count == 0; }   /* 0D5E:0516 */
int menu_can_save(void) { return (int8_t)word_32d8 <= 14 && Kid.alive < 0 && (int8_t)Kid.f12 > 0; }   /* 0D5E:0CD4 */
static qrect slot_rect(int i)   /* 0D5E:0742: two columns of five */
{
	qrect r; r.left = i < 5 ? 0x13 : 0xA3; if (i >= 5) i -= 5;
	r.right = r.left + 0x89; r.top = (int16_t)(i * 0x16 + 0x25); r.bottom = r.top + 0x14; return r;
}
static void box(int hilite, const qrect *r)   /* 0D5E:0816 */
{
	gfx_fill_rect(hilite ? 0xE : 0xB, r);
	the_port->fg = hilite ? 3 : 6; gfx_frame_rect(r);
}
static void slot_draw(int hilite, int i, uint8_t *hdr)   /* 0D5E:079A */
{
	qrect r = slot_rect(i); box(hilite, &r);
	char *name = (char *)hdr + i * SAV_NAME + 2;
	if (strlen(name)) { r = rect_inset(r, 2, 2); text_shadowed(NULL, &r, NULL, hilite != 0, 10, name); }
}
static const qrect rect_title = {14, 16, 37, 304}, rect_footer = {167, 16, 183, 304};   /* DS:0346 / 034E */
static void slots_screen(int save, uint8_t *hdr)   /* 0D5E:0850 */
{
	int16_t just[2] = {0, 0};
	text_res_shadowed(just, &rect_title, NULL, 0, 0xC, save ? 8000 : 8004);   /* "Saved Games" / "Resume Saved Game" */
	qrect f = rect_offset(rect_footer, -0x12, 0);
	text_res_shadowed(just, &f, NULL, 0, 10, 8001);                             /* "TAB to select" */
	just[1] = -1; text_res_shadowed(just, &rect_footer, NULL, 0, 10, save ? 8003 : 8013);   /* "ENTER to save" / "ENTER to load" */
	just[1] = 1; text_res_shadowed(just, &rect_footer, NULL, 0, 10, 8002);     /* "ESCAPE to cancel" */
	for (int i = 0; i < 10; i++) slot_draw(0, i, hdr);
}
static void save_message(int r)   /* 0D5E:0972 */
{
	if (r == -1) return;
	if (r == 0) { status_message("UNABLE TO SAVE GAME"); shell_sound(9999); }   /* 1611:053C(-1): resource 9999 */
	else status_message("GAME SAVED");
	word_5cdc = word_5cda = 0x18;
}
/* 0D5E:09AE: choose a slot (and, saving, type its name); -> the slot or -1 */
static int slot_choose(int save, uint8_t *hdr)
{
	int shown = -1, cur = 0, result = -1, done = 0; line_edit ed, *e = NULL; char name[0x1A];
	for (;;) {
		if (shown != cur) {
			if (shown != -1) { slot_draw(0, shown, hdr); e = NULL; }   /* 25BF:01C4 */
			shown = cur; slot_draw(1, cur, hdr);
			if (save == 1) {
				qrect r = rect_inset(slot_rect(cur), 2, 2);
				snprintf(name, sizeof name, "%s", (char *)hdr + cur * SAV_NAME + 2);
				edit_new(&ed, name, 0x18, &r); e = &ed; edit_caret(e, 1);   /* 25BF:000E */
			}
		}
		if (e) edit_blink(e);
		int key = sh_key(), next = cur;
		if (key == 9) next = cur == 9 ? 0 : cur + 1;   /* TAB */
		else if (key == 0xD) {
			if ((save == 0 && !strlen((char *)hdr + cur * SAV_NAME + 2)) || (save == 1 && !strlen(name))) shell_sound(0xFFFE);
			else {
				if (e) { char *dst = (char *)hdr + cur * SAV_NAME + 2; size_t l = strlen(name); if (l > SAV_NAME - 1) l = SAV_NAME - 1; memcpy(dst, name, l); dst[l] = 0; }   /* 2812:1F00 strcpy */
				result = cur; done = 1;
			}
		}
		else if (key == 0x11 || key == 0x1000) sh_quit(0, NULL);   /* Ctrl-Q, Alt-Q */
		else if (key == 0x1B) done = 1;
		else if (e) { if (key && key != 0x3E && key != 0x3C) edit_key(e, key); }
		else switch (key) {
			case 0x4700: next = 0; break;                            /* Home */
			case 0x4800: next = cur == 0 ? 9 : cur - 1; break;       /* up */
			case 0x4B00: case 0x4D00: next = cur >= 5 ? cur - 5 : cur + 5; break;
			case 0x4F00: next = 9; break;                            /* End */
			case 0x5000: next = cur == 9 ? 0 : cur + 1; break;       /* down */
		}
		if (done) return result;
		cur = next;
	}
}
int save_header_read(uint8_t hdr[SAV_HEADER])   /* 0D5E:0C0C */
{
	memset(hdr, 0, SAV_HEADER);
	long n = file_size("PRINCE.SAV");
	if (n < 0) return 1;   /* no file: an empty list */
	long k = n < SAV_HEADER ? n : SAV_HEADER;
	if (file_read("PRINCE.SAV", 0, hdr, k) != k || (int16_t)(hdr[0] | hdr[1] << 8) > 10) { memset(hdr, 0, SAV_HEADER); return 0; }
	return 1;
}
int save_slot_read(int slot)   /* 0D5E:0CF2 */
{
	if (slot >= 10) return 0;
	static uint8_t b[SAV_SLOT];
	if (file_read("PRINCE.SAV", SAV_HEADER + (long)slot * SAV_SLOT, b, SAV_SLOT) != SAV_SLOT) return 0;
	if ((b[0] | b[1] << 8) == 0) checkpoint_free(); else checkpoint_put_block(b);
	minutes_left = b[4] | b[5] << 8; clock_ticks = b[6] | b[7] << 8;
	word_32d8 = (int8_t)b[8]; start_hp = b[9]; Kid.f12 = Kid.f13 = b[9];
	flag_5cb9 = b[0xB]; byte_5cba = b[0xC]; word_5d38 = b[0xF] | b[0x10] << 8; byte_016a = (int8_t)b[0x11];
	word_5cb6 = 1;
	/* (the block is written back unchanged) */
	return 1;
}
int save_slot_write(int slot, const uint8_t hdr[SAV_HEADER])   /* 0D5E:0E5A */
{
	if (slot >= 10) return 0;
	static uint8_t b[SAV_SLOT];
	if (!checkpoint_get_block(b)) {
		memset(b, 0, SAV_SLOT);
		b[8] = (uint8_t)word_32d8; b[9] = start_hp; b[0xA] = ((uint8_t *)&level)[0x1862];   /* DS:441A: the level's start direction */
	}
	b[4] = (uint8_t)minutes_left; b[5] = minutes_left >> 8; b[6] = (uint8_t)clock_ticks; b[7] = clock_ticks >> 8;
	b[0xB] = flag_5cb9; b[0xC] = byte_5cba; b[0xF] = (uint8_t)word_5d38; b[0x10] = word_5d38 >> 8; b[0x11] = (uint8_t)byte_016a;
	if (!file_write_at("PRINCE.SAV", 0, hdr, SAV_HEADER, 1)) return 0;
	return file_write_at("PRINCE.SAV", SAV_HEADER + (long)slot * SAV_SLOT, b, SAV_SLOT, 1);
}
int game_restore(void)   /* 0D5E:0544 */
{
	sh_trace("game_restore", 0);
	int r = 0; static uint8_t hdr[SAV_HEADER];
	menu_enter();
	if (save_header_read(hdr)) {
		menu_background(0);
		slots_screen(0, hdr);
		the_port = &port_screen; menu_fade_in();
		port_free(port_back); port_back = NULL;
		r = slot_choose(0, hdr);
		if (r != -1) {
			r = save_slot_read(r);
			if (r) { byte_6b6c = (uint8_t)word_32d8; counter_5cec = (int8_t)word_32d8; word_5cd8 = 1; }
		}
		menu_fade_out();
	}
	menu_leave(r == -1);
	if (r > 0) copy_protection();
	if (r != -1 && port_back) { port_free(port_back); port_back = NULL; }
	return r;
}
int game_save(void)   /* 0D5E:060A */
{
	sh_trace("game_save", 0);
	int r = 0; static uint8_t hdr[SAV_HEADER];
	menu_enter(); menu_background(0);
	if (save_header_read(hdr)) {
		slots_screen(1, hdr);
		the_port = &port_screen; menu_fade_in();
		port_free(port_back); port_back = NULL;
		r = slot_choose(1, hdr);
		r = r != -1 ? save_slot_write(r, hdr) : -1;
		menu_fade_out();
	}
	menu_leave(1);
	save_message(r);
	return r;
}

/* ---- options (0D5E:0696, 1D2C .. 21CE, 20F4, 2166) ---- */
static qrect option_rect(int i)   /* 0D5E:1D2C */
{
	qrect r; r.left = i < 3 ? 0x26 : 0xC6; if (i >= 3) i -= 3;
	r.right = r.left + 0x8C > 0x136 ? 0x136 : r.left + 0x8C;
	r.top = (int16_t)(i * 0x16 + 0x28); r.bottom = r.top + 0x14; return r;
}
static const char *option_text(int i)   /* 0D5E:1F58's table */
{
	switch (i) {
	case 0: return sound_volume ? "Sound On" : "Sound Off";
	case 1: return amb_state[0] ? "Ambient Music On" : "Ambient Music Off";
	case 2: return input_device == 0 ? "Keyboard Mode" : "Joystick Mode";
	case 3: return "Save Game"; case 4: return "Resume Game"; default: return "Quit Game";
	}
}
static void option_draw(int erase, int i)   /* 0D5E:1F58 */
{
	qrect r = option_rect(i); int16_t just[2] = {0, -1};
	if (erase) gfx_fill_rect(0xC, &r);
	text_shadowed(just, &r, NULL, 0, 10, option_text(i));
}
static void option_pointer(int show, int i)   /* 0D5E:2032: set 1000's image 5 (mirrored) left of the item */
{
	qrect r = option_rect(i); int v = r.top + 6, h = r.left - 0x16, ih, iw;
	gfx_shape_size(1000, 5, &ih, &iw);
	if (show) gfx_draw_shape(1000, 5, v, h, 1);   /* 0823:1414 flips the rows; 194C:511C draws */
	else { qrect e = { (int16_t)v, (int16_t)h, (int16_t)(v + ih), (int16_t)(h + iw) }; gfx_fill_rect(0xC, &e); }
}
static void options_screen(void)   /* 0D5E:1E88 */
{
	the_port->bg = 0xC; int16_t just[2] = {0, 0};
	text_res_shadowed(just, &rect_title, NULL, 0, 0xC, 8016);   /* "Options" */
	qrect f = rect_offset(rect_footer, -0x12, 0);
	text_res_shadowed(just, &f, NULL, 0, 10, 8001);             /* "TAB to select" */
	just[1] = -1; text_res_shadowed(just, &rect_footer, NULL, 0, 10, 8014);   /* "ENTER to change" */
	just[1] = 1; text_res_shadowed(just, &rect_footer, NULL, 0, 10, 8015);    /* "ESCAPE to play" */
	for (int i = 0; i < 6; i++) option_draw(0, i);
	option_pointer(1, 0);
}
int sound_toggle(void) { int on = sound_volume == 0; sound_set_volume(on ? 0xF : 0); options_save(); return on; }   /* 0823:0AF0 */
int music_toggle_msg(void);   /* shell.c: 1611:07D4 */
int joystick_toggle_msg(void);   /* shell.c: 0823:0B1A */
static int option_change(int i)   /* 0D5E:21CE: -> 0 when nothing changed */
{
	int r = 1;
	if (i == 0) sound_set_volume(sound_volume == 0 ? 0xF : 0);
	else if (i == 1) { uint8_t was = amb_state[0]; music_toggle_msg(); r = amb_state[0] != was; }
	else if (i == 2) { if (input_device == 2) input_device = 0; else { joystick_toggle_msg(); if (input_device != 2) r = 0; } }
	else if (i == 5) sh_quit(0, NULL);
	return r;
}
static int options_choose(void)   /* 0D5E:1D7A */
{
	int shown = -1, cur = 0;
	for (;;) {
		if (shown != cur) { if (shown != -1) option_pointer(0, shown); option_pointer(1, cur); shown = cur; }
		int key = sh_key(), done = 0;
		switch (key) {
		case 9: cur = cur == 5 ? 0 : cur + 1; break;
		case 0xD:
			if (cur == 3 || cur == 4) { if (cur == 3 && !menu_can_save()) shell_sound(0xFFFE); else done = 1; }
			else if (option_change(cur)) option_draw(1, cur); else shell_sound(0xFFFE);
			break;
		case 0x11: case 0x1000: sh_quit(0, NULL); break;
		case 0x1B: cur = 0; done = 1; break;
		case 0x4700: cur = 0; break;
		case 0x4800: cur = cur == 0 ? 5 : cur - 1; break;
		case 0x4B00: case 0x4D00: cur = cur < 3 ? cur + 3 : cur - 3; break;
		case 0x4F00: cur = 5; break;
		case 0x5000: cur = cur == 5 ? 0 : cur + 1; break;
		}
		if (done) return cur;
	}
}
void options_menu(void)   /* 0D5E:0696 */
{
	sh_trace("options_menu", 0);
	gfx_erase_rect(&rect_screen);
	room_unload_pub();                       /* 0CD6:073A: the room description goes */
	menu_enter(); menu_background(0);
	gfx_shape_set(1000, 2, 1);               /* 26BC:0034(1, 2, 1000): the prince's set in bank 1 */
	options_screen();
	the_port = &port_screen; menu_fade_in();
	port_free(port_back); port_back = NULL;
	int r = options_choose();
	menu_fade_out();
	menu_leave(0);
	if (r == 3) game_save(); else if (r == 4) game_restore();
	options_save();
	render_after_menu(level_kind);
	word_5cce = 1;
}
void options_save(void)   /* 0D5E:20F4: two words, sound on and ambient music on */
{
	uint8_t b[4] = { sound_volume == 0xF, 0, amb_state[0], 0 };
	file_write_at("PRINCE.OPT", 0, b, 4, 1);
}
void options_load(void)   /* 0D5E:2166 */
{
	uint8_t b[4];
	if (file_read("PRINCE.OPT", 0, b, 4) != 4) return;
	if ((b[0] | b[1] << 8) == 0) sound_set_volume(0);
	if ((b[2] | b[3] << 8) == 0) amb_state[0] = 0;
}

/* ---- hall of fame (0D5E:1684 .. 1CC2) ---- */
int hof_read(hof_list *h)
{
	uint8_t b[2 + 6 * 29]; memset(h, 0, sizeof *h);
	long n = file_size("PRINCE.HOF");
	if (n < 0 || n > (long)sizeof b) return 0;
	if (file_read("PRINCE.HOF", 0, b, n) != n || n < 2) return 0;
	h->n = (int16_t)(b[0] | b[1] << 8);
	if (h->n > 5 || h->n < 0 || 2 + h->n * 29 > n) { h->n = 0; return 0; }
	for (int i = 0; i < h->n; i++) { memcpy(h->e[i].name, b + 2 + i * 29, 27); h->e[i].minutes = (int16_t)(b[2 + i * 29 + 27] | b[2 + i * 29 + 28] << 8); }
	return 1;
}
int hof_insert(hof_list *h, int minutes)   /* 0D5E:19F4 */
{
	int i = 0;
	while (i < h->n && h->e[i].minutes > minutes) i++;
	if (i < h->n && i != 4) memmove(&h->e[i + 1], &h->e[i], (h->n - i) * sizeof h->e[0]);
	if (h->n < 5) h->n++;
	else if (i == 5) i = 4;   /* (a full list always takes the new entry, in the last place) */
	h->e[i].name[0] = 0; h->e[i].minutes = (int16_t)minutes;
	return i;
}
int hof_write(const hof_list *h)   /* 0D5E:1CC2 */
{
	uint8_t b[2 + 6 * 29]; b[0] = (uint8_t)h->n; b[1] = (uint8_t)(h->n >> 8);
	for (int i = 0; i < h->n; i++) { memcpy(b + 2 + i * 29, h->e[i].name, 27); b[2 + i * 29 + 27] = (uint8_t)h->e[i].minutes; b[2 + i * 29 + 28] = (uint8_t)(h->e[i].minutes >> 8); }
	return file_create("PRINCE.HOF", b, 2 + h->n * 29);   /* 2812:1C03 creat */
}
static qrect hof_rect(int i) { qrect r = { (int16_t)(i * 0x18 + 0x3B), 0x19, (int16_t)(i * 0x18 + 0x3B + 0x14), 0x9B }; return r; }   /* 0D5E:17BA */
static void hof_screen(const hof_list *h)   /* 0D5E:17F2 */
{
	int16_t just[2] = {0, 0}; static const int16_t pt_name[2] = {53, 25}, pt_time[2] = {53, 260};   /* DS:0368 / 036C */
	static const qrect frame = {38, 20, 57, 300};   /* DS:0370 */
	menu_background(0);
	text_res_shadowed(just, &rect_title, NULL, 0, 0xC, 8009);   /* "Hall Of Fame" */
	box(0, &frame);
	just[1] = -1; text_res_shadowed(just, NULL, pt_name, 0, 10, 8010);   /* "Name" */
	text_res_shadowed(just, NULL, pt_time, 0, 10, 8012);                /* "Time" */
	for (int i = 0; i < h->n; i++) {
		qrect r = hof_rect(i); just[1] = -1;
		text_shadowed(just, &r, NULL, 0, 10, h->e[i].name);
		r.left = 0xB9; r.right = 0xDC; r = rect_offset(r, 0, 0x4B);
		char t[8]; snprintf(t, sizeof t, "%d", h->e[i].minutes); just[1] = 0;   /* 0D5E:006A (itoa) */
		text_shadowed(just, &r, NULL, 0, 10, t);
	}
}
static void hof_type_name(int i, hof_list *h)   /* 0D5E:18FC */
{
	qrect r = rect_inset(hof_rect(i), 0, -2);
	box(1, &r); r = rect_inset(r, 2, 2);
	line_edit ed; edit_new(&ed, h->e[i].name, 0x18, &r); edit_caret(&ed, 1);
	for (int done = 0; !done;) {
		edit_blink(&ed);
		int key = sh_key();
		if (key == 0xD) { if (!strlen(h->e[i].name)) shell_sound(0xFFFE); else done = 1; }
		else if (key && key != 0x3E && key != 0x3C) edit_key(&ed, key);
	}
}
void hall_of_fame_enter(int minutes)   /* 0D5E:1684 */
{
	hof_list h; hof_read(&h);
	int i = minutes > 0 ? hof_insert(&h, minutes) : 0;
	menu_enter(); hof_screen(&h);
	the_port = &port_screen; menu_fade_in();
	hof_type_name(i, &h);
	the_port = port_back;
	qrect r = rect_inset(hof_rect(i), 0, -2); gfx_fill_rect(0xC, &r);
	r = rect_inset(r, 0, 2); text_shadowed(NULL, &r, NULL, 0, 10, h.e[i].name);
	r = rect_inset(r, 0, -2);
	the_port = &port_screen; gfx_copy_bits(port_back, &port_screen, &r, &r);
	hof_write(&h);
	sh_wait(0x384, 0);
	menu_fade_out(); menu_leave(0);
	port_free(port_back); port_back = NULL;
	shell_sound_stop(0x280D);   /* 194C:83D2(0x280D) */
}
int hall_of_fame(int interactive)   /* 0D5E:1BEA */
{
	sh_trace("hall_of_fame", 0);
	hof_list h; int key = 0; hof_read(&h);
	if (h.n == 0 && !interactive) return 0;
	menu_enter(); shell_sound(10000 + 0xFD);   /* 1611:053C(0xFD) */
	hof_screen(&h);
	the_port = &port_screen; key = menu_fade_in();
	if (!key) key = interactive ? sh_wait_key() : sh_wait(0x384, 0);
	if (key && !interactive) gfx_fill_rect(0, &rect_screen);
	else key = menu_fade_out();
	menu_leave(interactive);
	if (!interactive) { port_free(port_back); port_back = NULL; }
	shell_sound_stop(0x280D);
	return key;
}

/* ---- copy protection (0D5E:1288 .. 1598) ---- */
static int16_t cp_pt(int i, int *h) { *h = (i % 5) * 0x2A + 0x3E; return (int16_t)((i / 5) * 0x2A + 0x45); }   /* 0D5E:13DA */
static const qrect rect_cp = {32, 73, 50, 266};   /* DS:035E */
static void cp_screen(int attempt, uint16_t set, int page)   /* 0D5E:1412 */
{
	static const int16_t pt_q1[2] = {28, 47}, pt_q2[2] = {44, 72};   /* DS:0356 / 035A */
	if (attempt) gfx_fill_rect(0xC, &rect_cp);
	char t[100]; const char *fmt = txt4_get(8006, NULL); snprintf(t, sizeof t, fmt ? fmt : "", page);   /* "on page %d of the manual" */
	text_shadowed(NULL, NULL, pt_q2, 0, 10, t);
	if (attempt) return;
	text_res_shadowed(NULL, NULL, pt_q1, 0, 10, 8005);   /* "Select the symbol that appears" */
	int16_t just[2] = {0, -1}; text_res_shadowed(just, &rect_footer, NULL, 0, 10, 8007);   /* "TAB to highlight" (just[0] is uninitialised: any value, the rows fit) */
	just[1] = 1; text_res_shadowed(just, &rect_footer, NULL, 0, 10, 8008);                 /* "ENTER to continue" */
	for (int i = 0; i < 10; i++) { int h, v = cp_pt(i, &h); gfx_draw_shape(set, i + 4, v, h, 0); }   /* images 4..13, mode 0xA */
}
static void cp_frame(int hilite, int i)   /* 0D5E:1536 */
{
	int fg = the_port->fg; the_port->fg = hilite ? 0xE : 0xC;
	int h, v = cp_pt(i, &h); qrect r = { (int16_t)(v - 3), (int16_t)(h - 3), (int16_t)(v - 3 + 0x23), (int16_t)(h - 3 + 0x23) };
	gfx_frame_rect(&r); the_port->fg = fg;
}
static int cp_choose(void)   /* 0D5E:1598 */
{
	int shown = -1, cur = 0;
	for (;;) {
		if (shown != cur) { if (shown != -1) cp_frame(0, shown); cp_frame(1, cur); shown = cur; }
		int key = sh_key(), done = 0;
		switch (key) {
		case 9: cur = cur == 9 ? 0 : cur + 1; break;
		case 0xD: done = 1; break;
		case 0x11: case 0x1000: sh_quit(0, NULL); break;
		case 0x4700: cur = 0; break;
		case 0x4800: case 0x5000: cur = cur >= 5 ? cur - 5 : cur + 5; break;
		case 0x4B00: cur = cur % 5 == 0 ? cur + 4 : cur - 1; break;
		case 0x4D00: cur = cur % 5 == 4 ? cur - 4 : cur + 1; break;
		case 0x4F00: cur = 9; break;
		}
		if (done) { cp_frame(0, cur); return cur; }
	}
}
void copy_protection(void)   /* 0D5E:1288 */
{
	sh_trace("copy_protection", 0);
	int ok = 1;
	if (word_0366 == 0 && word_2ba8 == 0) {
		word_0366 = 1;
		menu_enter();
		uint16_t set = (uint16_t)set_8000(); menu_background(set);
		uint16_t n; const uint8_t *tab = res_get("", 8000, &n);   /* PRINCE.DAT 8000: a count, then {page, symbol} words */
		int count = tab ? (tab[0] | tab[1] << 8) : 0, prev = -1;
		for (int attempt = 0; attempt < 3 && count > 1; attempt++) {
			int r = random_2751(count - 1);
			if (r == prev) r = (random_2751(count - 2) + r + 1) % count;
			prev = r;
			int page = tab[2 + r * 4] | tab[3 + r * 4] << 8, answer = tab[4 + r * 4] | tab[5 + r * 4] << 8;
			the_port = port_back; cp_screen(attempt, set, page); the_port = &port_screen;
			if (attempt == 0) menu_fade_in(); else gfx_copy_bits(port_back, &port_screen, &rect_cp, &rect_cp);
			int c = cp_choose();
			ok = c == answer || c == -2;
			if (ok) break;
		}
		port_free(port_back); port_back = NULL;
		menu_fade_out(); menu_leave(0);
	}
	if (!ok) sh_quit(1, "Copy protection failure.");
}

/* ---- the title credits (0D5E:225C / 23C8 / 2402) ---- */
static void credit_text(uint16_t id)   /* 0D5E:2402 */
{
	uint16_t len; const char *list = txt4_get(id, &len); if (!list) return;
	qrect r = { (int16_t)(the_port->pen_v - 0x13), 16, the_port->pen_v, 304 };   /* DS:0350 / 0354 */
	int16_t just[2] = {0, -1}; int count = (uint8_t)list[0];
	if (count == 1) { just[1] = 0; if (id == 1011 || id == 1012 || (id >= 1020 && id <= 1026)) r = rect_inset(rect_screen, 9, 9); }
	text_shadowed(just, &r, NULL, 0, 10, txt4_string(id, 1));
	if (count > 1) {
		just[1] = 1;
		for (int k = 2; k <= count; k++) { text_shadowed(just, &r, NULL, 0, 10, txt4_string(id, k)); r = rect_offset(r, 0x15, 0); gfx_move(0x15, 0); }
	} else gfx_move(0x15, 0);
}
static void credit_page(uint16_t first, uint16_t last)   /* 0D5E:23C8 */
{
	gfx_move_to(37, 16);   /* DS:034A / 0348 */
	for (uint16_t id = first; id <= last; id++) { credit_text(id); gfx_move(4, 0); }
}
int sh_transition(int effect, gport *src, const qrect *r);   /* shell.c: 2A31:0CE5 -> 33B9:0000 */
int title_credits(void)   /* 0D5E:225C */
{
	sh_trace("title_credits", 0);
	static const uint16_t first[5] = {1000, 1007, 1009, 1011, 1012}, last[5] = {1006, 1008, 1010, 1011, 1012};   /* DS:039C / 03A6 */
	menu_enter(); the_port = port_back; menu_background(0);
	shell_sound(10000 + 0x10C);   /* 1611:053C(0x10C) */
	qrect inner = rect_inset(rect_screen, 9, 9); int key = 0;
	for (int page = 0; page < 5 && !key; page++) {
		the_port = port_back; gfx_fill_rect(0xC, &inner);
		credit_page(first[page], last[page]);
		the_port = &port_screen;
		key = page ? sh_transition(0x5A, port_back, &rect_screen) : menu_fade_in();
		if (key) break;
		key = sh_wait(0x1E0, 0);   /* (with BLINK on the command line the first page blinks DS:03B0 instead) */
	}
	the_port = &port_screen; menu_fade_out();
	menu_leave(0);
	port_free(port_back); port_back = NULL;
	shell_sound_stop(0x281C);
	return key;
}
