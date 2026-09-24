/*
SDLPoP, a port/conversion of the DOS game Prince of Persia.
Copyright (C) 2013-2025  Dávid Nagy

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

The authors of this program may be contacted at https://forum.princed.org
*/

/* SDLPoP2: transcribed and adapted for SDLPoP2 from SDLPoP's src/menu.c at commit 3c5add5fb7f8
 * (https://github.com/NagyD/SDLPoP/blob/3c5add5fb7f8/src/menu.c), with the pieces of seg000.c / seg009.c it relies on
 * (draw_menu's caller, process_events' menu part, the dialog frame, showmessage_any_key / redefine_key).
 * The design, the layout, the colours, the fonts, the navigation (keyboard, mouse, controller), the pages and the code's
 * structure and names are SDLPoP's; the settings are SDLPoP2.ini's. The adaptations (marked "SDLPoP2:" below):
 *  - SDLPoP's menu runs its own loops (draw_menu, the dialogs) inside the game's tick. SDLPoP2's frontend runs one video
 *    frame at a time, so draw_menu's loop body runs once per queued key each frame (overlay_menu_frame) and the dialogs
 *    (confirmation, level / skill selection, key redefinition) are states that each pass steps; the controller's
 *    repeat delays are counted in video frames (0.3 s = 21, 0.1 s = 7) instead of the performance counter.
 *  - SDLPoP's overlay_surface (32-bit RGBA, blended over the game) is here a 320 x 200 port of SDLPoP2's text library
 *    (text.h) whose pixel values index this file's RGBA table (overlay_colors: SDLPoP's 16 VGA colours, then the alpha
 *    fills and the setting box's grey), blended over the game's picture by overlay_menu_compose; the game's screen_buf
 *    and render_palette are not touched, so nothing needs restoring. The game's own "GAME PAUSED" (PoP2's status line
 *    message, its font 0) goes in a layer under the overlay, as SDLPoP's display_text_bottom goes to the screen.
 *  - Fonts: SDLPoP's small menu font (hc_small_font, below, transcribed) is given to the text library
 *    (gfx_add_font); SDLPoP's big font (hc_font, PoP1's) is PoP2's own status-line font (font 0: the same 7 + 2 pixel
 *    metrics), used for the dialogs and "GAME PAUSED". '\n' in the texts is the library's '\r'.
 *  - Sounds: SDLPoP plays PoP1 sounds on navigation (play_menu_sound). SDLPoP2 freezes the game's sound while the menu
 *    shows (the frontend pauses the audio device), so play_menu_sound makes no sound.
 *  - Pause menu: RESTART LEVEL / RESTART GAME type PoP2's Alt+A / Alt+R (SDLPoP: Ctrl+A / Ctrl+R); QUICKSAVE / QUICKLOAD
 *    are the frontend's F6 / F9 (shell_quicksave / shell_quickload), available with enable_quicksave and not while a
 *    replay plays back (nor the restarts): SDLPoP's `required`, but an unavailable item is greyed out in its place
 *    where SDLPoP leaves it out; QUIT GAME's confirmation ends the program.
 *    Keys with Alt or Ctrl (PoP2's commands) close the menu and go to the game, as SDLPoP's Ctrl+ keys.
 *  - Cheats: SDLPoP's "Enable cheats" (GAMEPLAY) turns PoP2's cheats (DS:10C2, shell_cheats / shell_set_cheats) on
 *    and off at any time; off by default, on from the start with the cheat word (yippeeyahoo), as SDLPoP's megahit.
 *    It is not saved (SDLPoP does not save cheats_enabled either); the frontend makes the change at the next game step
 *    and a replay records it. SDLPoP's CHEATS pause item (commented out there, "TODO: Add a cheats menu, where you can
 *    choose a cheat from a list?") is made: shown only with the cheats on (SDLPoP's `required`: left out, the one pause
 *    item that is not greyed out instead), a page laid out as the settings (CHEATS / BACK, the list on the right, the
 *    help line) listing PoP2's cheat keys (the `cheats` table below: shell.c's cheat_keys, 0823:0528, and Alt+N's cheat
 *    rule) with their keys, as the CONTROLS page shows keys; choosing one closes the menu and types its key into the
 *    game (as SDLPoP's menu passes its Ctrl+ keys on). While a replay plays back, both are greyed out (the recording
 *    decides).
 *  - Settings: SDLPoP2.ini's sections mapped onto SDLPoP's pages: GENERAL (the menu, the info screen, sound, music, volume,
 *    the sound device, the controller), GAMEPLAY (the cheats, quicksave, its penalty, replays, the intro, the story
 *    scenes), VISUALS (fullscreen, 4:3, integer scaling, the scaling method), MODS ([CustomGameplay], skip_title,
 *    "Customize level..." -> [Level N], "Customize guard skill..." -> [Skill N], the latter an SDLPoP2 page made like
 *    the level page), CONTROLS (the key_* keys). SDLPoP's PoP1-only settings (bug fixes, fading, lighting, hardware acceleration, the copy
 *    protection level) have no counterpart; there is no setting that skips or moves PoP2's copy protection.
 *    The settings that change the game (the ones a replay holds: settings_write_gameplay) are disabled (SDLPoP's
 *    `required`) while a replay is recorded or played back; otherwise they apply at once, where the game next reads
 *    them (SDLPoP's custom options do the same: start_* at the next new game, [Level N] at the next level start or
 *    restart). "Restore defaults..." restores the menu's settings (not those it cannot change now).
 *  - SDLPoP.cfg (a binary dump checked against the executable's CRC) is SDLPoP2.cfg, the menu's settings as SDLPoP2.ini
 *    text, with SDLPoP's rule: read after the ini unless the ini is newer.
 *  - The key redefinition dialog clears its inside (SDLPoP draws it over the settings' text). */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "overlay_menu.h"
#include "controller.h"
#include "../source/text.h"
#include "../source/render.h"

/* ---- SDLPoP2: SDLPoP's types and facilities, on SDLPoP2's ---- */
typedef uint8_t byte;
typedef int8_t sbyte;
typedef uint16_t word;
typedef uint32_t dword;
typedef qrect rect_type;   /* (the same order: top, left, bottom, right) */
typedef int bool_type;
#define COUNT(array) (int)(sizeof(array) / sizeof(array[0]))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
enum { halign_left = -1, halign_center = 0, halign_right = 1 };
enum { valign_top = -1, valign_middle = 0, valign_bottom = 1 };
enum { color_0_black = 0, color_7_lightgray = 7, color_8_darkgray = 8, color_15_brightwhite = 15 };
enum { blitters_0_no_transp = 0 };
enum key_modifiers { WITH_SHIFT = 0x8000, WITH_CTRL = 0x4000, WITH_ALT = 0x2000 };
static const rect_type screen_rect = {0, 0, 200, 320};
/* SDLPoP's rect_bottom_text (193, 70, 202, 250) is PoP2's status-line message area */
#define rect_bottom_text rect_status

static overlay_menu_host host;
static pop2_settings *S;            /* host.settings */

/* the overlay's colours: SDLPoP's palette (VGA_PALETTE_DEFAULT, 6-bit << 2), then colours with alpha as they are used */
typedef struct rgba_type { byte r, g, b, a; } rgba_type;
static rgba_type overlay_colors[256];
static int overlay_color_count;
static const byte vga_palette_default[16][3] = {
	{0x00, 0x00, 0x00}, {0x00, 0x00, 0x2A}, {0x00, 0x2A, 0x00}, {0x00, 0x2A, 0x2A},
	{0x2A, 0x00, 0x00}, {0x2A, 0x00, 0x2A}, {0x2A, 0x15, 0x00}, {0x2A, 0x2A, 0x2A},
	{0x15, 0x15, 0x15}, {0x15, 0x15, 0x3F}, {0x15, 0x3F, 0x15}, {0x15, 0x3F, 0x3F},
	{0x3F, 0x15, 0x15}, {0x3F, 0x15, 0x3F}, {0x3F, 0x3F, 0x15}, {0x3F, 0x3F, 0x3F},
};
static int map_rgba(int r, int g, int b, int a)   /* (SDL_MapRGBA) */
{
	if (a == 0) r = g = b = 0;
	for (int i = 0; i < overlay_color_count; i++) {
		rgba_type* c = &overlay_colors[i];
		if (c->r == r && c->g == g && c->b == b && c->a == a) return i;
	}
	if (overlay_color_count >= 256) return 0;
	overlay_colors[overlay_color_count] = (rgba_type) {(byte) r, (byte) g, (byte) b, (byte) a};
	return overlay_color_count++;
}

/* the surfaces: SDLPoP's overlay_surface, and (under it) the screen's bottom text line */
static gport* overlay_surface;
static gport* bottom_text_surface;
static gport* current_target_surface;

/* the fonts: textstate.ptr_font */
enum { hc_font = 0 /* PoP2's font 0 */, hc_small_font = 0xF5F5 /* (an id no FONT resource has) */ };
static struct { word ptr_font; } textstate = { hc_font };
extern byte hc_small_font_data[];
extern byte arrowhead_up_image_data[];
extern byte arrowhead_down_image_data[];
extern byte arrowhead_left_image_data[];
extern byte arrowhead_right_image_data[];
typedef byte image_type;   /* IMAGE_DATA: height, width, flags (words), then 1-bit rows */
static image_type* arrowhead_up_image;
static image_type* arrowhead_down_image;
static image_type* arrowhead_left_image;
static image_type* arrowhead_right_image;

static int image_word(const byte* p) { return p[0] | p[1] << 8; }
static int calc_stride(const byte* image_data) {
	int width = image_word(image_data + 2);
	int flags = image_word(image_data + 4);
	int depth = ((flags >> 12) & 7) + 1;
	return (depth * width + 7) / 8;
}
/* seg009.c: load_font_character_offsets (the hardcoded font's offsets are filled in at run time) */
static void load_font_character_offsets(byte* data) {
	int n_chars = data[1] - data[0] + 1;
	byte* pos = data + 10 + n_chars * 2;
	for (int index = 0; index < n_chars; ++index) {
		int offset = (int)(pos - data);
		data[10 + index * 2] = (byte) offset;
		data[10 + index * 2 + 1] = (byte) (offset >> 8);
		int image_bytes = image_word(pos) * calc_stride(pos);
		pos += 6 + image_bytes;
	}
}

static void with_target(void) { the_port = current_target_surface; }
static void without_target(gport* saved) { the_port = saved; }

// seg009:37E8
static void draw_rect(const rect_type* rect, int color) {
	gport* saved = the_port; with_target();
	gfx_fill_rect(color, rect);
	without_target(saved);
}
static const rect_type* method_5_rect(const rect_type* rect, int blit, byte color) {
	(void) blit;
	draw_rect(rect, color);
	return rect;
}
static void draw_rect_with_alpha(const rect_type* rect, byte color, byte alpha) {
	const byte* c = vga_palette_default[color];
	draw_rect(rect, map_rgba(c[0] << 2, c[1] << 2, c[2] << 2, alpha));
}
static void draw_rect_contours(const rect_type* rect, byte color) {
	gport* saved = the_port; with_target();
	int saved_fg = the_port->fg; the_port->fg = color;
	gfx_frame_rect(rect);
	the_port->fg = (int16_t) saved_fg;
	without_target(saved);
}
// seg009:39CE
static rect_type* shrink2_rect(rect_type* target_rect, const rect_type* source_rect, int delta_x, int delta_y) {
	target_rect->top    = (int16_t) (source_rect->top    + delta_y);
	target_rect->left   = (int16_t) (source_rect->left   + delta_x);
	target_rect->bottom = (int16_t) (source_rect->bottom - delta_y);
	target_rect->right  = (int16_t) (source_rect->right  - delta_x);
	return target_rect;
}
// seg009:04FF (show_text: draw_text, clipped to the rectangle, with the text library's 194C:64FE justification, the
// same rules as SDLPoP's draw_text)
static void show_text_with_color(const rect_type* rect_ptr, int x_align, int y_align, const char* text, int color) {
	gport* saved = the_port; with_target();
	char buffer[512];
	snprintf(buffer, sizeof(buffer), "%s", text);
	for (char* p = buffer; *p; ++p) if (*p == '\n') *p = '\r';   /* (the library's line break) */
	qrect saved_clip = the_port->clip;
	qrect clip = *rect_ptr;
	if (clip.top < saved_clip.top) clip.top = saved_clip.top;
	if (clip.left < saved_clip.left) clip.left = saved_clip.left;
	if (clip.bottom > saved_clip.bottom) clip.bottom = saved_clip.bottom;
	if (clip.right > saved_clip.right) clip.right = saved_clip.right;
	the_port->clip = clip;
	int saved_fg = the_port->fg; word saved_font = the_port->font;
	the_port->fg = (int16_t) color;
	gfx_text_font(textstate.ptr_font);
	gfx_text_box_str(buffer, y_align, x_align, rect_ptr);
	the_port->fg = (int16_t) saved_fg; the_port->font = saved_font; the_port->clip = saved_clip;
	without_target(saved);
}
// seg009:403F
static int get_line_width(const char* text, int length) {
	gport* saved = the_port; with_target();
	word saved_font = the_port->font;
	gfx_text_font(textstate.ptr_font);
	int width = gfx_text_width(text, length);
	the_port->font = saved_font;
	without_target(saved);
	return width;
}
static void draw_image_with_blending(image_type* image, int xpos, int ypos) {
	int height = image_word(image), width = image_word(image + 2), stride = calc_stride(image);
	gport* saved = the_port; with_target();
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			if (!(image[6 + y * stride + (x >> 3)] & (0x80 >> (x & 7)))) continue;   /* (color key 0) */
			rect_type pixel = {(int16_t) (ypos + y), (int16_t) (xpos + x), (int16_t) (ypos + y + 1), (int16_t) (xpos + x + 1)};
			gfx_fill_rect(color_15_brightwhite, &pixel);   /* (the images' colour 1 is white) */
		}
	}
	without_target(saved);
}
// seg008:2644
static void display_text_bottom(const char* text) {
	gport* saved_target = current_target_surface;
	current_target_surface = bottom_text_surface;
	draw_rect(&screen_rect, map_rgba(0, 0, 0, 0));
	draw_rect(&rect_bottom_text, color_0_black);
	word saved_font = textstate.ptr_font;
	textstate.ptr_font = hc_font;
	show_text_with_color(&rect_bottom_text, halign_center, valign_bottom, text, color_15_brightwhite);
	textstate.ptr_font = saved_font;
	current_target_surface = saved_target;
}

/* the dialog (seg009: copyprot_dialog, made from dialog_settings and dialog_rect_1) */
typedef struct dialog_settings_type {
	short top_border, left_border, bottom_border, right_border, shadow_bottom, shadow_right, outer_border;
} dialog_settings_type;
typedef struct dialog_type {
	const dialog_settings_type* settings;
	rect_type text_rect;
	rect_type peel_rect;
} dialog_type;
static const dialog_settings_type dialog_settings = {4, 4, 4, 4, 3, 4, 1};
static dialog_type copyprot_dialog_ = { &dialog_settings, {60, 56, 124, 264}, {0, 0, 0, 0} };
static dialog_type* copyprot_dialog = &copyprot_dialog_;
// seg009:0BE7
static void calc_dialog_peel_rect(dialog_type* dialog) {
	const dialog_settings_type* settings = dialog->settings;
	dialog->peel_rect.left = (int16_t) (dialog->text_rect.left - settings->left_border);
	dialog->peel_rect.top = (int16_t) (dialog->text_rect.top - settings->top_border);
	dialog->peel_rect.right = (int16_t) (dialog->text_rect.right + settings->right_border + settings->shadow_right);
	dialog->peel_rect.bottom = (int16_t) (dialog->text_rect.bottom + settings->bottom_border + settings->shadow_bottom);
}
// seg009:09F0
static void dialog_method_2_frame(dialog_type* dialog) {
	rect_type rect;
	short shadow_right = dialog->settings->shadow_right;
	short shadow_bottom = dialog->settings->shadow_bottom;
	short bottom_border = dialog->settings->bottom_border;
	short outer_border = dialog->settings->outer_border;
	short peel_top = dialog->peel_rect.top;
	short peel_left = dialog->peel_rect.left;
	short peel_bottom = dialog->peel_rect.bottom;
	short peel_right = dialog->peel_rect.right;
	short text_top = dialog->text_rect.top;
	short text_left = dialog->text_rect.left;
	short text_bottom = dialog->text_rect.bottom;
	short text_right = dialog->text_rect.right;
	// Draw outer border
	rect = (rect_type) { peel_top, peel_left, (int16_t) (peel_bottom - shadow_bottom), (int16_t) (peel_right - shadow_right) };
	draw_rect(&rect, color_0_black);
	// Draw shadow (right)
	rect = (rect_type) { text_top, (int16_t) (peel_right - shadow_right), peel_bottom, peel_right };
	draw_rect(&rect, color_8_darkgray /*dialog's shadow*/);
	// Draw shadow (bottom)
	rect = (rect_type) { (int16_t) (peel_bottom - shadow_bottom), text_left, peel_bottom, peel_right };
	draw_rect(&rect, color_8_darkgray /*dialog's shadow*/);
	// Draw inner border (left)
	rect = (rect_type) { (int16_t) (peel_top + outer_border), (int16_t) (peel_left + outer_border), text_bottom, text_left };
	draw_rect(&rect, color_15_brightwhite);
	// Draw inner border (top)
	rect = (rect_type) { (int16_t) (peel_top + outer_border), text_left, text_top, (int16_t) (text_right + dialog->settings->right_border - outer_border) };
	draw_rect(&rect, color_15_brightwhite);
	// Draw inner border (right)
	rect.top = text_top;
	rect.left =  text_right;
	rect.bottom = (int16_t) (text_bottom + bottom_border - outer_border);           // (rect.right stays the same)
	draw_rect(&rect, color_15_brightwhite);
	// Draw inner border (bottom)
	rect = (rect_type) { text_bottom, (int16_t) (peel_left + outer_border), (int16_t) (text_bottom + bottom_border - outer_border), text_right };
	draw_rect(&rect, color_15_brightwhite);
}

// types.h: names lists
#define MAX_OPTION_VALUE_NAME_LENGTH 20
typedef struct key_value_type {
	char key[MAX_OPTION_VALUE_NAME_LENGTH];
	int value;
} key_value_type;
typedef struct names_list_type {
	byte type; // 0 = names list, 1 = key/value pair list
	union {
		struct {
			const char (* data)[][MAX_OPTION_VALUE_NAME_LENGTH];
			word count;
		} names;
		struct {
			const key_value_type* data;
			word count;
		} kv_pairs;
	};
} names_list_type;
#define NAMES_LIST(listname, ...) static const char listname[][MAX_OPTION_VALUE_NAME_LENGTH] = __VA_ARGS__; \
static names_list_type listname##_list = {.type=0, .names = {&listname, COUNT(listname)}}
#define KEY_VALUE_LIST(listname, ...) static const key_value_type listname[] = __VA_ARGS__; \
static names_list_type listname##_list = {.type=1, .kv_pairs= {listname, COUNT(listname)}}

/* SDLPoP's input state (data.h) */
static bool_type have_mouse_input;
static bool_type have_keyboard_or_controller_input;
static int mouse_x, mouse_y;
static bool_type mouse_moved;
static bool_type mouse_clicked;
static bool_type mouse_button_clicked_right;
static bool_type pressed_enter;
static bool_type escape_key_suppressed;
static int menu_control_scroll_y;
static sbyte is_menu_shown;
static bool_type is_joyst_mode;
static int last_key_scancode;       /* the key of this pass (read_key) */
static int last_any_key_scancode;   /* for showmessage_any_key */

/* SDLPoP2: the events between two video frames, queued for draw_menu's passes (SDLPoP's process_events fills
 * last_key_scancode / last_any_key_scancode as the events come) */
#define KEY_QUEUE_SIZE 32
static struct { int key, any_key; } key_queue[KEY_QUEUE_SIZE];
static int key_queue_count;
static int pending_mouse_x, pending_mouse_y, pending_mouse_clicked, pending_mouse_clicked_right, pending_scroll_y;
static byte key_held[SDL_NUM_SCANCODES];
static int menu_action, menu_action_key; static word menu_action_mod;
static dword frame_counter;              /* video frames while the menu shows (the controller's repeat) */
static bool_type joystick_read_this_frame;
static int replaying_replay;
static int gameplay_settings_editable;   /* 0 while a replay is recorded or played back */
static int quicksave_allowed;            /* enable_quicksave, not while replaying */
static int restart_allowed;              /* not while replaying */
static int cheats_enabled;               /* SDLPoP's cheats_enabled: the game's DS:10C2 (shell_cheats) as the menu shows it */
static int cheats_editable;              /* SDLPoP2: "Enable cheats" and the CHEATS page: not while replaying */
static int cheat_key_chosen;             /* SDLPoP2: the CHEATS page's key for the game (OVERLAY_MENU_CHEAT) */

static void play_menu_sound(int sound_id) {
	/* SDLPoP2: SDLPoP plays PoP1's sounds here (play_sound + play_next_sound); the game's sound is frozen while the
	   menu shows, so there is no sound. */
	(void) sound_id;
}
enum { sound_10_sword_vs_sword = 10, sound_21_loose_shake_2 = 21, sound_22_loose_shake_3 = 22 };

static void load_arrowhead_images(void) {
	// SDLPoP2: the images are drawn from their data (draw_image_with_blending); nothing to decode.
	if (arrowhead_up_image == NULL) {
		arrowhead_up_image = arrowhead_up_image_data;
	}
	if (arrowhead_down_image == NULL) {
		arrowhead_down_image = arrowhead_down_image_data;
	}
	if (arrowhead_left_image == NULL) {
		arrowhead_left_image = arrowhead_left_image_data;
	}
	if (arrowhead_right_image == NULL) {
		arrowhead_right_image = arrowhead_right_image_data;
	}
}

#define MAX_MENU_ITEM_LENGTH 32

typedef struct pause_menu_item_type pause_menu_item_type;
struct pause_menu_item_type {
	int id;
	pause_menu_item_type* previous;
	pause_menu_item_type* next;
	const int* required;
	char text[MAX_MENU_ITEM_LENGTH];
};

enum pause_menu_item_ids {
	PAUSE_MENU_RESUME,
	PAUSE_MENU_CHEATS,
	PAUSE_MENU_SAVE_GAME,
	PAUSE_MENU_LOAD_GAME,
	PAUSE_MENU_RESTART_LEVEL,
	PAUSE_MENU_SETTINGS,
	PAUSE_MENU_RESTART_GAME,
	PAUSE_MENU_QUIT_GAME,
	SETTINGS_MENU_GENERAL,
	SETTINGS_MENU_GAMEPLAY,
	SETTINGS_MENU_VISUALS,
	SETTINGS_MENU_MODS,
	SETTINGS_MENU_LEVEL_CUSTOMIZATION,
	SETTINGS_MENU_BACK,
	SETTINGS_MENU_CONTROLS,
	SETTINGS_MENU_SKILL_CUSTOMIZATION, // SDLPoP2
	SETTINGS_MENU_CHEATS, // SDLPoP2: the CHEATS page's list
	CHEATS_MENU_BACK, // SDLPoP2
};

static pause_menu_item_type pause_menu_items[] = {
		{.id = PAUSE_MENU_RESUME,        .text = "RESUME"},
		// SDLPoP: "TODO: Add a cheats menu, where you can choose a cheat from a list?" (commented out); SDLPoP2: made
		{.id = PAUSE_MENU_CHEATS,        .text = "CHEATS", .required = &cheats_enabled},
		// SDLPoP2: the frontend's F6 / F9, with enable_quicksave (not while a replay plays back)
		{.id = PAUSE_MENU_SAVE_GAME,     .text = "QUICKSAVE (F6)", .required = &quicksave_allowed},
		{.id = PAUSE_MENU_LOAD_GAME,     .text = "QUICKLOAD (F9)", .required = &quicksave_allowed},
		{.id = PAUSE_MENU_RESTART_LEVEL, .text = "RESTART LEVEL", .required = &restart_allowed},
		{.id = PAUSE_MENU_SETTINGS,      .text = "SETTINGS"},
		{.id = PAUSE_MENU_RESTART_GAME,  .text = "RESTART GAME", .required = &restart_allowed},
		{.id = PAUSE_MENU_QUIT_GAME,     .text = "QUIT GAME"},
};

static int hovering_pause_menu_item = PAUSE_MENU_RESUME;
static pause_menu_item_type* next_pause_menu_item;
static pause_menu_item_type* previous_pause_menu_item;
static int drawn_menu;
static byte pause_menu_alpha;
static int current_dialog_box;
static const char* current_dialog_text;
static word menu_current_level = 1;
static word menu_current_skill = 0; // SDLPoP2
static bool_type need_close_menu;

enum menu_dialog_ids {
	DIALOG_NONE,
	DIALOG_RESTORE_DEFAULT_SETTINGS,
	DIALOG_CONFIRM_QUIT,
	DIALOG_SELECT_LEVEL,
	DIALOG_SELECT_SKILL,   // SDLPoP2
	DIALOG_REDEFINE_KEY,   // SDLPoP2: redefine_key's showmessage_any_key
};

static pause_menu_item_type settings_menu_items[] = {
		{.id = SETTINGS_MENU_GENERAL, .text = "GENERAL"},
		{.id = SETTINGS_MENU_GAMEPLAY, .text = "GAMEPLAY"},
		{.id = SETTINGS_MENU_VISUALS, .text = "VISUALS"},
		{.id = SETTINGS_MENU_MODS, .text = "MODS"},
		{.id = SETTINGS_MENU_CONTROLS, .text = "CONTROLS"},
		{.id = SETTINGS_MENU_BACK, .text = "BACK"},
};
// SDLPoP2: the CHEATS page, laid out as the settings page (its left part)
static pause_menu_item_type cheats_menu_items[] = {
		{.id = SETTINGS_MENU_CHEATS, .text = "CHEATS"},
		{.id = CHEATS_MENU_BACK, .text = "BACK"},
};
static int active_settings_subsection = 0;
static int highlighted_settings_subsection = 0;
static int scroll_position = 0;
static int menu_control_y;
static int menu_control_x;
static int menu_control_back;

enum menu_setting_style_ids {
	SETTING_STYLE_TOGGLE,
	SETTING_STYLE_NUMBER,
	SETTING_STYLE_TEXT_ONLY,
	SETTING_STYLE_KEY,
	SETTING_STYLE_CHEAT, // SDLPoP2: an entry of the CHEATS page (its key shown as SETTING_STYLE_KEY's)
};

enum menu_setting_number_type_ids {
	SETTING_BYTE  = 0,
	SETTING_SBYTE = 1,
	SETTING_WORD  = 2,
	SETTING_SHORT = 3,
	//SETTING_DWORD = 4,
	SETTING_INT   = 5,
};

enum setting_ids {
	SETTING_RESET_ALL_SETTINGS,
	SETTING_SHOW_MENU_ON_PAUSE,
	SETTING_ENABLE_INFO_SCREEN,
	SETTING_ENABLE_SOUND,
	SETTING_ENABLE_MUSIC,
	SETTING_VOLUME,
	SETTING_SOUND_DEVICE,
	SETTING_ENABLE_CONTROLLER,
	SETTING_ENABLE_CONTROLLER_RUMBLE,
	SETTING_JOYSTICK_THRESHOLD,
	SETTING_JOYSTICK_ONLY_HORIZONTAL,
	SETTING_FULLSCREEN,
	SETTING_USE_CORRECT_ASPECT_RATIO,
	SETTING_USE_INTEGER_SCALING,
	SETTING_SCALING_TYPE,
	SETTING_ENABLE_CHEATS,
	SETTING_ENABLE_QUICKSAVE,
	SETTING_ENABLE_QUICKSAVE_PENALTY,
	SETTING_ENABLE_REPLAY,
	SETTING_ENABLE_INTRO,
	SETTING_ENABLE_STORY_SCENES,
	SETTING_START_MINUTES_LEFT,
	SETTING_TICKS_PER_MINUTE,
	SETTING_START_HITP,
	SETTING_MAX_HITP_ALLOWED,
	SETTING_FIRST_LEVEL,
	SETTING_SKIP_TITLE,
	SETTING_SKIP_LEVEL_REDUCED_MINUTES,
	SETTING_BASE_SPEED,
	SETTING_FIGHT_SPEED,
	SETTING_LEVEL_SETTINGS,
	SETTING_SWORD_TYPE,
	SETTING_SKILL_SETTINGS,
	SETTING_STRIKEPROB,
	SETTING_RESTRIKEPROB,
	SETTING_BLOCKPROB,
	SETTING_IMPBLOCKPROB,
	SETTING_ADVPROB,
	SETTING_REFRACTIMER,
	SETTING_KEY_LEFT,
	SETTING_KEY_RIGHT,
	SETTING_KEY_UP,
	SETTING_KEY_DOWN,
	SETTING_KEY_UPLEFT,
	SETTING_KEY_UPRIGHT,
	SETTING_KEY_DOWNLEFT,
	SETTING_KEY_DOWNRIGHT,
	SETTING_KEY_SHIFT,
	SETTING_KEY_CTRL,
	SETTING_CHEAT_FIRST, // SDLPoP2: the CHEATS page's entries (SETTING_CHEAT_FIRST + their index in `cheats`)
};

typedef struct setting_type {
	int index;
	int id;
	int previous, next;
	byte style;
	byte number_type;
	void* linked;
	const int* required;
	int min, max; // for 'number'-style settings
	char text[64];
	char explanation[400];
	names_list_type* names_list;
	// SDLPoP2: where the setting is in pop2_settings (offset + 1; 0: none), its place in SDLPoP2.ini ("Section/key"),
	// the ini's names of its values (names-list settings), and whether it changes the game (a replay holds it)
	size_t link;
	const char* ini;
	const char* const* ini_values;
	byte gameplay;
} setting_type;
#define LINK(field) .link = offsetof(pop2_settings, field) + 1

static const char* const bool_ini_values[] = {"false", "true"};
static const char* const scaling_ini_values[] = {"sharp", "fuzzy", "blurry"};
static const char* const sound_device_ini_values[] = {"speaker", "digital", "fm", "fm_digital"};
NAMES_LIST(sound_device_setting_names, {"PC speaker", "Digital", "FM", "FM + digital",});

static setting_type general_settings[] = {
		{.id = SETTING_SHOW_MENU_ON_PAUSE, .style = SETTING_STYLE_TOGGLE, LINK(enable_pause_menu), .ini = "General/enable_pause_menu",
				.text = "Enable pause menu",
				.explanation = "Show the in-game menu when you pause the game.\n"
						"If disabled, you can still bring up the menu by pressing Backspace."},
		{.id = SETTING_ENABLE_INFO_SCREEN, .style = SETTING_STYLE_TOGGLE, LINK(enable_info_screen), .ini = "AdditionalFeatures/enable_info_screen",
				.text = "Enable info screen (F1)",
				.explanation = "Show the SDLPoP2 information screen (a summary of the keys) when you press F1."},
		{.id = SETTING_ENABLE_SOUND, .style = SETTING_STYLE_TOGGLE, LINK(enable_sounds), .ini = "General/enable_sounds",
				.text = "Enable sound",
				.explanation = "Turn sound on or off.\n(The sound effects: the digitized sounds; with the PC speaker, all of its sounds.)"},
		{.id = SETTING_ENABLE_MUSIC, .style = SETTING_STYLE_TOGGLE, LINK(enable_music), .ini = "General/enable_music",
				.text = "Enable music",
				.explanation = "Turn music on or off."},
		{.id = SETTING_VOLUME, .style = SETTING_STYLE_NUMBER, .number_type = SETTING_INT,
				LINK(volume), .min = 0, .max = 15, .ini = "General/volume",
				.text = "Volume",
				.explanation = "The volume when the sound is on, from 0 (silent) to 15 (full).\n"
						"The game's Alt+S still turns the sound on and off."},
		{.id = SETTING_SOUND_DEVICE, .style = SETTING_STYLE_NUMBER, .number_type = SETTING_INT, .max = 3,
				LINK(sound_device), .names_list = &sound_device_setting_names_list,
				.ini = "General/sound_device", .ini_values = sound_device_ini_values,
				.text = "Sound device",
				.explanation = "FM + digital - Sound Blaster Pro: FM music and digitized sounds.\n"
						"FM - Music only.\nDigital - Digitized sounds only.\nPC speaker - The PC speaker.\n"
						"Note: This requires a restart."},
		{.id = SETTING_ENABLE_CONTROLLER, .style = SETTING_STYLE_TOGGLE, LINK(enable_controller), .ini = "Controller/enable_controller",
				.text = "Enable controller",
				.explanation = "Play with a game controller (SDL's game controllers: plugged in or unplugged at any time)."},
		{.id = SETTING_ENABLE_CONTROLLER_RUMBLE, .style = SETTING_STYLE_TOGGLE, LINK(controller_rumble), .ini = "Controller/controller_rumble",
				.text = "Enable controller rumble",
				.explanation = "If using a controller with a rumble motor, provide haptic feedback when the kid is hurt."},
		{.id = SETTING_JOYSTICK_THRESHOLD, .style = SETTING_STYLE_NUMBER, .number_type = SETTING_INT,
				LINK(joystick_threshold), .min = 0, .max = INT16_MAX, .ini = "Controller/joystick_threshold",
				.text = "Joystick threshold",
				.explanation = "Joystick 'dead zone' sensitivity threshold."},
		{.id = SETTING_JOYSTICK_ONLY_HORIZONTAL, .style = SETTING_STYLE_TOGGLE, LINK(joystick_only_horizontal), .ini = "Controller/joystick_only_horizontal",
				.text = "Horizontal joystick movement only",
				.explanation = "Use joysticks for horizontal movement only, not all-directional. "
						"This may make the game easier to control for some controllers."},
		{.id = SETTING_RESET_ALL_SETTINGS, .style = SETTING_STYLE_TEXT_ONLY,
				.text = "Restore defaults...", .explanation = "Revert all settings to the default state."},
};

NAMES_LIST(scaling_type_setting_names, {"Sharp", "Fuzzy", "Blurry",});

static int integer_scaling_possible =
#if SDL_VERSION_ATLEAST(2,0,5) // SDL_RenderSetIntegerScale
	1
#else
	0
#endif
;

static setting_type visuals_settings[] = {
		{.id = SETTING_FULLSCREEN, .style = SETTING_STYLE_TOGGLE, LINK(start_fullscreen), .ini = "General/start_fullscreen",
				.text = "Start fullscreen",
				.explanation = "Start the game in fullscreen mode.\nYou can also toggle fullscreen by pressing Alt+Enter."},
		{.id = SETTING_USE_CORRECT_ASPECT_RATIO, .style = SETTING_STYLE_TOGGLE, LINK(use_correct_aspect_ratio), .ini = "General/use_correct_aspect_ratio",
				.text = "Use 4:3 aspect ratio",
				.explanation = "Render the game in the originally intended 4:3 aspect ratio."
				               "\nNB. Works best using a high resolution."},
		{.id = SETTING_USE_INTEGER_SCALING, .style = SETTING_STYLE_TOGGLE, LINK(use_integer_scaling), .ini = "General/use_integer_scaling",
				.required = &integer_scaling_possible,
				.text = "Use integer scaling",
				.explanation = "Enable pixel perfect scaling. That is, make all pixels the same size by forcing integer scale factors.\n"
						"Combining with 4:3 aspect ratio requires at least 1600x1200."
						"\nYou need to compile with SDL 2.0.5 or newer to enable this."},
		{.id = SETTING_SCALING_TYPE, .style = SETTING_STYLE_NUMBER, .number_type = SETTING_INT, .max = 2,
				LINK(scaling_type), .names_list = &scaling_type_setting_names_list,
				.ini = "General/scaling_type", .ini_values = scaling_ini_values,
				.text = "Scaling method",
				.explanation = "Sharp - Use nearest neighbour resampling.\n"
						"Fuzzy - First upscale to double size, then use smooth scaling.\n"
						"Blurry - Use smooth scaling."},
};

static setting_type gameplay_settings[] = {
		// SDLPoP2: not a setting of SDLPoP2.ini (not saved, as SDLPoP's): the game's DS:10C2 (overlay_menu_cheats)
		{.id = SETTING_ENABLE_CHEATS, .style = SETTING_STYLE_TOGGLE, .linked = &cheats_enabled, .required = &cheats_editable,
				.text = "Enable cheats",
				.explanation = "Turn cheats on or off (yippeeyahoo: on at the start).\n"
						"Also, display the CHEATS option on the pause menu."},
		{.id = SETTING_ENABLE_QUICKSAVE, .style = SETTING_STYLE_TOGGLE, LINK(enable_quicksave), .ini = "AdditionalFeatures/enable_quicksave",
				.text = "Enable quicksave",
				.explanation = "Enable quicksave/load feature.\nPress F6 to quicksave, F9 to quickload."},
		{.id = SETTING_ENABLE_QUICKSAVE_PENALTY, .style = SETTING_STYLE_TOGGLE, LINK(enable_quicksave_penalty),
				.ini = "AdditionalFeatures/enable_quicksave_penalty", .gameplay = 1, .required = &gameplay_settings_editable,
				.text = "Quicksave time penalty",
				.explanation = "Try to let time run out when quickloading (similar to dying).\n"
						"Actually, the 'remaining time' will still be restored, "
						"but a penalty (up to one minute) will be applied."},
		{.id = SETTING_ENABLE_REPLAY, .style = SETTING_STYLE_TOGGLE, LINK(enable_replay), .ini = "AdditionalFeatures/enable_replay",
				.text = "Enable replays",
				.explanation = "Enable recording/replay feature.\n"
						"Start the program with --record NAME to record, with --replay NAME to play back."},
		{.id = SETTING_ENABLE_INTRO, .style = SETTING_STYLE_TOGGLE, LINK(enable_intro),
				.ini = "General/enable_intro", .gameplay = 1, .required = &gameplay_settings_editable,
				.text = "Play the intro",
				.explanation = "Play the intro (the story scenes of the title sequence).\n"
						"Without it the title goes on to the credits and the demos."},
		{.id = SETTING_ENABLE_STORY_SCENES, .style = SETTING_STYLE_TOGGLE, LINK(enable_story_scenes),
				.ini = "General/enable_story_scenes", .gameplay = 1, .required = &gameplay_settings_editable,
				.text = "Play the story scenes",
				.explanation = "Play the story scenes between the levels (and the time-out and ending scenes).\n"
						"A skipped scene counts as played to its end."},
};

static setting_type mods_settings[] = {
		{.id = SETTING_LEVEL_SETTINGS, .style = SETTING_STYLE_TEXT_ONLY,
				.text = "Customize level...",
				.explanation = "Change level-specific options (the prince's sword)."},
		{.id = SETTING_SKILL_SETTINGS, .style = SETTING_STYLE_TEXT_ONLY,
				.text = "Customize guard skill...",
				.explanation = "Change the guards' fighting skills (strike, block and advance probabilities)."},
		{.id = SETTING_START_MINUTES_LEFT, .style = SETTING_STYLE_NUMBER, .required = &gameplay_settings_editable,
				LINK(start_minutes_left), .number_type = SETTING_INT, .min = 1, .max = INT16_MAX,
				.ini = "CustomGameplay/start_minutes_left", .gameplay = 1,
				.text = "Starting minutes left",
				.explanation = "Starting minutes left. (default = 75)"},
		{.id = SETTING_TICKS_PER_MINUTE, .style = SETTING_STYLE_NUMBER, .required = &gameplay_settings_editable,
				LINK(ticks_per_minute), .number_type = SETTING_INT, .min = 1, .max = UINT16_MAX,
				.ini = "CustomGameplay/ticks_per_minute", .gameplay = 1,
				.text = "Clock ticks per minute",
				.explanation = "Game clock ticks per minute. One tick is 1/12 second.\n(default = 719, 59.92 seconds)"},
		{.id = SETTING_START_HITP, .style = SETTING_STYLE_NUMBER, .required = &gameplay_settings_editable,
				LINK(start_hitp), .number_type = SETTING_INT, .min = 1, .max = 127,
				.ini = "CustomGameplay/start_hitp", .gameplay = 1,
				.text = "Starting hitpoints",
				.explanation = "Starting hitpoints. (default = 3)"},
		{.id = SETTING_MAX_HITP_ALLOWED, .style = SETTING_STYLE_NUMBER, .required = &gameplay_settings_editable,
				LINK(max_hitp_allowed), .number_type = SETTING_INT, .min = 1, .max = 127,
				.ini = "CustomGameplay/max_hitp_allowed", .gameplay = 1,
				.text = "Max hitpoints allowed",
				.explanation = "Maximum number of hitpoints you can get. (default = 12)"},
		{.id = SETTING_FIRST_LEVEL, .style = SETTING_STYLE_NUMBER, .required = &gameplay_settings_editable,
				LINK(first_level), .number_type = SETTING_INT, .min = 1, .max = 14,
				.ini = "CustomGameplay/first_level", .gameplay = 1,
				.text = "First level",
				.explanation = "Level that will be loaded when starting a new game."
						"\n(default = 1)"},
		{.id = SETTING_SKIP_TITLE, .style = SETTING_STYLE_TOGGLE, .required = &gameplay_settings_editable,
				LINK(skip_title), .ini = "General/skip_title", .gameplay = 1,
				.text = "Skip title sequence",
				.explanation = "Always skip the title sequence: the first level will be loaded immediately."
						"\n(default = OFF)"},
		{.id = SETTING_SKIP_LEVEL_REDUCED_MINUTES, .style = SETTING_STYLE_NUMBER, .required = &gameplay_settings_editable,
				LINK(skip_level_reduced_minutes), .number_type = SETTING_INT, .min = 1, .max = INT16_MAX,
				.ini = "CustomGameplay/skip_level_reduced_minutes", .gameplay = 1,
				.text = "Minutes left after Alt+N used",
				.explanation = "Number of minutes left after Alt+N is used in non-cheat mode.\n"
						"(default = 15)"},
		{.id = SETTING_BASE_SPEED, .style = SETTING_STYLE_NUMBER, .required = &gameplay_settings_editable,
				LINK(base_speed), .number_type = SETTING_INT, .min = 1, .max = 255,
				.ini = "CustomGameplay/base_speed", .gameplay = 1,
				.text = "Base speed",
				.explanation = "Game speed when not fighting (delay between frames in 1/60 seconds). Smaller is faster.\n(default = 5)"},
		{.id = SETTING_FIGHT_SPEED, .style = SETTING_STYLE_NUMBER, .required = &gameplay_settings_editable,
				LINK(fight_speed), .number_type = SETTING_INT, .min = 1, .max = 255,
				.ini = "CustomGameplay/fight_speed", .gameplay = 1,
				.text = "Fight speed",
				.explanation = "Game speed when fighting (delay between frames in 1/60 seconds). Smaller is faster.\n(default = 6)"},
};

KEY_VALUE_LIST(sword_type_setting_names, {{"None", 0}});
static const char* const sword_type_ini_values[] = {"none", "1", "2"};

static setting_type level_settings[] = {
		{.id = SETTING_LEVEL_SETTINGS, .style = SETTING_STYLE_TEXT_ONLY,
				.text = "Customize another level...",
				.explanation = "Select another level to customize."},
		{.id = SETTING_SWORD_TYPE, .style = SETTING_STYLE_NUMBER, .required = &gameplay_settings_editable,
				.names_list = &sword_type_setting_names_list,
				.linked = NULL /* depends on which level */, .number_type = SETTING_BYTE, .max = 2,
				.ini = "Level/sword_type", .ini_values = sword_type_ini_values, .gameplay = 1,
				.text = "Sword type",
				.explanation = "The prince's sword on this level: none, 1 or 2 (2: the reach is 5 pixels shorter).\n"
						"A restarted level keeps the one of its checkpoint."},
};

// SDLPoP2: the guard skills' page, made like the level page
static setting_type skill_settings[] = {
		{.id = SETTING_SKILL_SETTINGS, .style = SETTING_STYLE_TEXT_ONLY,
				.text = "Customize another skill...",
				.explanation = "Select another guard skill to customize."},
		{.id = SETTING_STRIKEPROB, .style = SETTING_STYLE_NUMBER, .required = &gameplay_settings_editable,
				.linked = NULL, .number_type = SETTING_WORD, .max = UINT8_MAX, .ini = "Skill/strikeprob", .gameplay = 1,
				.text = "Strike probability",
				.explanation = "Probability of striking, from 0 to 255."},
		{.id = SETTING_RESTRIKEPROB, .style = SETTING_STYLE_NUMBER, .required = &gameplay_settings_editable,
				.linked = NULL, .number_type = SETTING_WORD, .max = UINT8_MAX, .ini = "Skill/restrikeprob", .gameplay = 1,
				.text = "Restrike probability",
				.explanation = "Probability of striking right after blocking, from 0 to 255."},
		{.id = SETTING_BLOCKPROB, .style = SETTING_STYLE_NUMBER, .required = &gameplay_settings_editable,
				.linked = NULL, .number_type = SETTING_WORD, .max = UINT8_MAX, .ini = "Skill/blockprob", .gameplay = 1,
				.text = "Block probability",
				.explanation = "Probability of blocking, from 0 to 255."},
		{.id = SETTING_IMPBLOCKPROB, .style = SETTING_STYLE_NUMBER, .required = &gameplay_settings_editable,
				.linked = NULL, .number_type = SETTING_WORD, .max = UINT8_MAX, .ini = "Skill/impblockprob", .gameplay = 1,
				.text = "Impaired block probability",
				.explanation = "Probability of blocking within 4 ticks of the guard's last block, from 0 to 255."},
		{.id = SETTING_ADVPROB, .style = SETTING_STYLE_NUMBER, .required = &gameplay_settings_editable,
				.linked = NULL, .number_type = SETTING_WORD, .max = UINT8_MAX, .ini = "Skill/advprob", .gameplay = 1,
				.text = "Advance probability",
				.explanation = "Probability of going into hit range, from 0 to 255."},
		{.id = SETTING_REFRACTIMER, .style = SETTING_STYLE_NUMBER, .required = &gameplay_settings_editable,
				.linked = NULL, .number_type = SETTING_WORD, .max = UINT16_MAX, .ini = "Skill/refractimer", .gameplay = 1,
				.text = "Refractory timer",
				.explanation = "Refractory period after being hit (ticks before the guard strikes again)."},
};

static setting_type controls_settings[] = {
		{.id = SETTING_KEY_LEFT, .style = SETTING_STYLE_KEY, .required = NULL,
				LINK(keys[KEY_LEFT]), .number_type = SETTING_INT, .ini = "General/key_left",
				.text = "Left",
				.explanation = ""},
		{.id = SETTING_KEY_RIGHT, .style = SETTING_STYLE_KEY, .required = NULL,
				LINK(keys[KEY_RIGHT]), .number_type = SETTING_INT, .ini = "General/key_right",
				.text = "Right",
				.explanation = ""},
		{.id = SETTING_KEY_UP, .style = SETTING_STYLE_KEY, .required = NULL,
				LINK(keys[KEY_UP]), .number_type = SETTING_INT, .ini = "General/key_up",
				.text = "Up",
				.explanation = ""},
		{.id = SETTING_KEY_DOWN, .style = SETTING_STYLE_KEY, .required = NULL,
				LINK(keys[KEY_DOWN]), .number_type = SETTING_INT, .ini = "General/key_down",
				.text = "Down",
				.explanation = ""},
		{.id = SETTING_KEY_UPLEFT, .style = SETTING_STYLE_KEY, .required = NULL,
				LINK(keys[KEY_UPLEFT]), .number_type = SETTING_INT, .ini = "General/key_upleft",
				.text = "Jump left",
				.explanation = ""},
		{.id = SETTING_KEY_UPRIGHT, .style = SETTING_STYLE_KEY, .required = NULL,
				LINK(keys[KEY_UPRIGHT]), .number_type = SETTING_INT, .ini = "General/key_upright",
				.text = "Jump right",
				.explanation = ""},
		{.id = SETTING_KEY_DOWNLEFT, .style = SETTING_STYLE_KEY, .required = NULL,
				LINK(keys[KEY_DOWNLEFT]), .number_type = SETTING_INT, .ini = "General/key_downleft",
				.text = "Down left",
				.explanation = ""},
		{.id = SETTING_KEY_DOWNRIGHT, .style = SETTING_STYLE_KEY, .required = NULL,
				LINK(keys[KEY_DOWNRIGHT]), .number_type = SETTING_INT, .ini = "General/key_downright",
				.text = "Down right",
				.explanation = ""},
		{.id = SETTING_KEY_SHIFT, .style = SETTING_STYLE_KEY, .required = NULL,
				LINK(keys[KEY_SHIFT]), .number_type = SETTING_INT, .ini = "General/key_shift",
				.text = "Action",
				.explanation = ""},
		{.id = SETTING_KEY_CTRL, .style = SETTING_STYLE_KEY, .required = NULL,
				LINK(keys[KEY_CTRL]), .number_type = SETTING_INT, .ini = "General/key_ctrl",
				.text = "Sword, cast",
				.explanation = ""},
};

// SDLPoP2: the CHEATS page: PoP2's cheats (shell.c: cheat_keys, 0823:0528, and Alt+N's rule with the cheats on),
// one line each: the key (a DOS keystroke code: overlay_menu_key_label shows it), what the action does with it, the
// line on the page, the help line.
enum cheat_action_ids {
	CHEAT_ACTION_TYPE_KEY, // the key typed into the game (OVERLAY_MENU_CHEAT, the menu closes)
};
typedef struct cheat_type {
	word key;
	byte action;
	const char* text;
	const char* explanation;
} cheat_type;
static const cheat_type cheats[] = {
		{0x3100, CHEAT_ACTION_TYPE_KEY, "Skip to the next level",
				"Any level, the clock not cut (without cheats: up to level 3).\n"
				"The copy protection is still asked before level 3."},
		{'+', CHEAT_ACTION_TYPE_KEY, "One more minute", "One minute more on the clock."},
		{'-', CHEAT_ACTION_TYPE_KEY, "One minute less", "One minute less on the clock (not below one)."},
		{'T', CHEAT_ACTION_TYPE_KEY, "One more hit point", "One more hit point, and one more at most (up to MODS: Max hitpoints allowed)."},
		{'K', CHEAT_ACTION_TYPE_KEY, "One hit point less", "The prince loses a hit point (at none he dies)."},
		{'g', CHEAT_ACTION_TYPE_KEY, "Opponent: one more hit point", "The prince's opponent gets one more hit point, and one more at most."},
		{'k', CHEAT_ACTION_TYPE_KEY, "Kill the room's characters",
				"Every character in the room dies (skeletons collapse, the heads turn away)."},
		{'r', CHEAT_ACTION_TYPE_KEY, "Revive the prince", "A dead prince lives again."},
		{'W', CHEAT_ACTION_TYPE_KEY, "Feather fall", "The prince falls slowly for a while."},
		{'I', CHEAT_ACTION_TYPE_KEY, "Upside down", "The screen upside down (again: back)."},
		{'R', CHEAT_ACTION_TYPE_KEY, "Show the room number", "The room's number on the status line."},
		{'S', CHEAT_ACTION_TYPE_KEY, "Count a spirit turn",
				"Temple levels and level 14: the spirit's turn counter to its end "
				"(it leaves with more than 4 hit points, else death)."},
		{0x3D00, CHEAT_ACTION_TYPE_KEY, "Demo player on / off", "The game's demo player on or off (PLAYER ON / PLAYER OFF)."},
};
static setting_type cheats_settings[COUNT(cheats)];   // (from `cheats`: init_cheats_settings)

typedef struct settings_area_type {
	setting_type* settings;
	int setting_count;
} settings_area_type;

static settings_area_type general_settings_area = { .settings = general_settings, .setting_count = COUNT(general_settings)};
static settings_area_type gameplay_settings_area = { .settings = gameplay_settings, .setting_count = COUNT(gameplay_settings)};
static settings_area_type visuals_settings_area = { .settings = visuals_settings, .setting_count = COUNT(visuals_settings)};
static settings_area_type mods_settings_area = { .settings = mods_settings, .setting_count = COUNT(mods_settings)};
static settings_area_type level_settings_area = { .settings = level_settings, .setting_count = COUNT(level_settings)};
static settings_area_type skill_settings_area = { .settings = skill_settings, .setting_count = COUNT(skill_settings)};
static settings_area_type controls_settings_area = { .settings = controls_settings, .setting_count = COUNT(controls_settings)};
static settings_area_type cheats_settings_area = { .settings = cheats_settings, .setting_count = COUNT(cheats_settings)}; // SDLPoP2

static settings_area_type* get_settings_area(int menu_item_id) {
	switch(menu_item_id) {
		default:
			return NULL;
		case SETTINGS_MENU_GENERAL:
			return &general_settings_area;
		case SETTINGS_MENU_GAMEPLAY:
			return &gameplay_settings_area;
		case SETTINGS_MENU_VISUALS:
			return &visuals_settings_area;
		case SETTINGS_MENU_MODS:
			return &mods_settings_area;
		case SETTINGS_MENU_LEVEL_CUSTOMIZATION:
			return &level_settings_area;
		case SETTINGS_MENU_SKILL_CUSTOMIZATION:
			return &skill_settings_area;
		case SETTINGS_MENU_CONTROLS:
			return &controls_settings_area;
		case SETTINGS_MENU_CHEATS: // SDLPoP2
			return &cheats_settings_area;
	}
}
static settings_area_type* const all_settings_areas[] = {   // SDLPoP2 (saving, restoring the defaults)
	&general_settings_area, &visuals_settings_area, &gameplay_settings_area, &mods_settings_area, &controls_settings_area,
};

static void init_pause_menu_items(pause_menu_item_type* first_item, int item_count) {
	if (item_count > 0) {
		for (int i = 0; i < item_count; ++i) {
			pause_menu_item_type* item = first_item + i;
			item->previous = (first_item + MAX(0, i-1));
			item->next = (first_item + MIN(item_count-1, i+1));
		}
		pause_menu_item_type* last_item = first_item + (item_count-1);
		first_item->previous = last_item;
		last_item->next = first_item;
	}
}

static void init_settings_list(setting_type* first_setting, int setting_count) {
	if (setting_count > 0) {
		for (int i = 0; i < setting_count; ++i) {
			setting_type* item = first_setting + i;
			item->index = i;
			item->previous = (first_setting + MAX(0, i-1))->id;
			item->next = (first_setting + MIN(setting_count-1, i+1))->id;
			if (item->link) item->linked = (char*) S + (item->link - 1);   // SDLPoP2: the frontend's settings
		}
//		setting_type* last_item = first_setting + (setting_count-1);
//		first_setting->previous = last_item->id;
//		last_item->next = first_setting->id;
	}
}

// SDLPoP2: the CHEATS page's entries, from the `cheats` table
static void init_cheats_settings(void) {
	for (int i = 0; i < COUNT(cheats); ++i) {
		setting_type* setting = &cheats_settings[i];
		setting->id = SETTING_CHEAT_FIRST + i;
		setting->style = SETTING_STYLE_CHEAT;
		setting->linked = (void*) &cheats[i];
		setting->required = &cheats_editable;
		snprintf(setting->text, sizeof(setting->text), "%s", cheats[i].text);
		snprintf(setting->explanation, sizeof(setting->explanation), "%s", cheats[i].explanation);
	}
}

void overlay_menu_key_label(int code, char* out, size_t n) {
	static const char scan_letters[] =   // PC scan codes 0x10..0x32: the letters (Alt+letter is scan << 8)
		"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0QWERTYUIOP\0\0\0\0ASDFGHJKL\0\0\0\0\0ZXCVBNM";
	if (n == 0) return;
	if (code > 0 && code < 0x100) {
		if (code >= 'a' && code <= 'z') snprintf(out, n, "%c", code - 'a' + 'A');
		else if (code >= 'A' && code <= 'Z') snprintf(out, n, "Shift+%c", code);
		else snprintf(out, n, "%c", code);
		return;
	}
	int scan = code >> 8;
	if (scan >= 0x3B && scan <= 0x44) snprintf(out, n, "F%d", scan - 0x3B + 1);
	else if (scan >= 0x10 && scan < (int) sizeof(scan_letters) - 1 && scan_letters[scan]) snprintf(out, n, "Alt+%c", scan_letters[scan]);
	else snprintf(out, n, "0x%04X", code);
}

static void clear_menu_controls(void);
static void process_additional_menu_input(void);
static int key_test_paused_menu(int key);

static void init_menu(void) {
	load_arrowhead_images();

	init_pause_menu_items(pause_menu_items, COUNT(pause_menu_items));
	init_pause_menu_items(settings_menu_items, COUNT(settings_menu_items));
	init_pause_menu_items(cheats_menu_items, COUNT(cheats_menu_items)); // SDLPoP2

	init_settings_list(general_settings, COUNT(general_settings));
	init_settings_list(visuals_settings, COUNT(visuals_settings));
	init_settings_list(gameplay_settings, COUNT(gameplay_settings));
	init_settings_list(mods_settings, COUNT(mods_settings));
	init_settings_list(level_settings, COUNT(level_settings));
	init_settings_list(skill_settings, COUNT(skill_settings));
	init_settings_list(controls_settings, COUNT(controls_settings));
	init_cheats_settings(); // SDLPoP2
	init_settings_list(cheats_settings, COUNT(cheats_settings));
}

static bool_type is_mouse_over_rect(const rect_type* rect) {
	return (mouse_x >= rect->left && mouse_x < rect->right && mouse_y >= rect->top && mouse_y < rect->bottom);
}

// Maps the cursor position into a coordinate between (0,0) and (320,200) and sets mouse_x, mouse_y and mouse_moved.
// SDLPoP2: from the mouse events (SDL gives them in the renderer's logical size: 320 x 240 with the 4:3 aspect ratio)
static void read_mouse_state(void) {
	int last_mouse_x = mouse_x;
	int last_mouse_y = mouse_y;
	mouse_x = pending_mouse_x;
	mouse_y = pending_mouse_y;
	mouse_moved = (last_mouse_x != mouse_x || last_mouse_y != mouse_y);
}
static void mouse_position_from_event(int x, int y) {
	int logical_width = 0, logical_height = 0;
	if (host.renderer) SDL_RenderGetLogicalSize(host.renderer, &logical_width, &logical_height);
	if (logical_width <= 0 || logical_height <= 0) { logical_width = SCREEN_W; logical_height = SCREEN_H; }
	pending_mouse_x = x * SCREEN_W / logical_width;
	pending_mouse_y = y * SCREEN_H / logical_height;
}

static rect_type explanation_rect = {170, 20, 200, 300};
static int highlighted_setting_id = SETTING_ENABLE_INFO_SCREEN;
static int controlled_area = 0; // Whether the focus is on the left (0) or right (1) part of the screen in the Settings menu.
static int next_setting_id = 0; // For navigating up/down.
static int previous_setting_id = 0;
static bool_type at_scroll_up_boundary; // When navigating up using keyboard/controller, whether we also need to scroll up
static bool_type at_scroll_down_boundary; // When navigating down using keyboard/controller, whether we also need to scroll down

static void enter_settings_subsection(int settings_menu_id) {
	settings_area_type* settings_area = get_settings_area(settings_menu_id);
	if (active_settings_subsection != settings_menu_id) {
		highlighted_setting_id = settings_area->settings[0].id;
	}
	active_settings_subsection = settings_menu_id;
	highlighted_settings_subsection = settings_menu_id;
	if (!mouse_clicked) hovering_pause_menu_item = 0;
	controlled_area = 1;
	scroll_position = 0;

	// Special case: for the level customization submenu, the linked variables should depend on menu_current_level.
	// So we need to initialize them now.
	if (settings_menu_id == SETTINGS_MENU_LEVEL_CUSTOMIZATION) {
		for (int i = 0; i < settings_area->setting_count; ++i) {
			setting_type* setting = &settings_area->settings[i];
			switch(setting->id) {
				default: break;
				case SETTING_SWORD_TYPE:
					setting->linked = &S->sword_type[menu_current_level];
					break;
			}
		}
	}
	// SDLPoP2: the same for the guard skill submenu, with menu_current_skill
	if (settings_menu_id == SETTINGS_MENU_SKILL_CUSTOMIZATION) {
		for (int i = 0; i < settings_area->setting_count; ++i) {
			setting_type* setting = &settings_area->settings[i];
			switch(setting->id) {
				default: break;
				case SETTING_STRIKEPROB:   setting->linked = &S->strikeprob[menu_current_skill]; break;
				case SETTING_RESTRIKEPROB: setting->linked = &S->restrikeprob[menu_current_skill]; break;
				case SETTING_BLOCKPROB:    setting->linked = &S->blockprob[menu_current_skill]; break;
				case SETTING_IMPBLOCKPROB: setting->linked = &S->impblockprob[menu_current_skill]; break;
				case SETTING_ADVPROB:      setting->linked = &S->advprob[menu_current_skill]; break;
				case SETTING_REFRACTIMER:  setting->linked = &S->refractimer[menu_current_skill]; break;
			}
		}
	}
}

static void leave_settings_subsection(void) {
	if (active_settings_subsection == SETTINGS_MENU_LEVEL_CUSTOMIZATION || active_settings_subsection == SETTINGS_MENU_SKILL_CUSTOMIZATION) {
		enter_settings_subsection(SETTINGS_MENU_MODS);
	} else {
		// Go back to the top level of the settings menu.
		controlled_area = 0;
		hovering_pause_menu_item = active_settings_subsection;
		active_settings_subsection = 0;
		highlighted_settings_subsection = 0;
	}
}

static void reset_paused_menu(void) {
	drawn_menu = 0;
	controlled_area = 0;
	hovering_pause_menu_item = PAUSE_MENU_RESUME;
}

static void pause_menu_clicked(pause_menu_item_type* item) {
	//printf("Clicked option %s\n", item->text);
	play_menu_sound(sound_22_loose_shake_3);
	switch(item->id) {
		default: break;
		case PAUSE_MENU_RESUME:
			need_close_menu = 1;
			break;
		case PAUSE_MENU_CHEATS:
			// SDLPoP2: the CHEATS page (drawn as the settings page), the list in focus at the entry chosen last
			drawn_menu = 2;
			hovering_pause_menu_item = SETTINGS_MENU_CHEATS;
			enter_settings_subsection(SETTINGS_MENU_CHEATS);
			scroll_position = MAX(0, highlighted_setting_id - SETTING_CHEAT_FIRST - 8);
			break;
		case PAUSE_MENU_SAVE_GAME:
			// SDLPoP2: the frontend's F6 (shell_quicksave: while playing, at the next game tick)
			menu_action = OVERLAY_MENU_QUICKSAVE;
			need_close_menu = 1;
			break;
		case PAUSE_MENU_LOAD_GAME:
			// SDLPoP2: the frontend's F9 (shell_quickload silences the sound device)
			menu_action = OVERLAY_MENU_QUICKLOAD;
			need_close_menu = 1;
			break;
		case PAUSE_MENU_RESTART_LEVEL:
			// SDLPoP: last_key_scancode = Ctrl+A, which closes the menu; SDLPoP2: PoP2's Alt+A
			menu_action = OVERLAY_MENU_RESTART_LEVEL;
			need_close_menu = 1;
			break;
		case PAUSE_MENU_SETTINGS:
			drawn_menu = 1;
			hovering_pause_menu_item = SETTINGS_MENU_GENERAL;
			highlighted_settings_subsection = SETTINGS_MENU_GENERAL;
			active_settings_subsection = 0;
			controlled_area = 0;
			break;
		case PAUSE_MENU_RESTART_GAME:
			// SDLPoP: Ctrl+R; SDLPoP2: PoP2's Alt+R (to the title)
			menu_action = OVERLAY_MENU_RESTART_GAME;
			need_close_menu = 1;
			break;
		case PAUSE_MENU_QUIT_GAME:
			current_dialog_box = DIALOG_CONFIRM_QUIT;
			current_dialog_text = "Quit SDLPoP2?";
			break;
		case SETTINGS_MENU_GENERAL:
		case SETTINGS_MENU_GAMEPLAY:
		case SETTINGS_MENU_VISUALS:
		case SETTINGS_MENU_MODS:
		case SETTINGS_MENU_CONTROLS:
		case SETTINGS_MENU_CHEATS: // SDLPoP2
			enter_settings_subsection(item->id);
			break;
		case CHEATS_MENU_BACK: // SDLPoP2
			reset_paused_menu();
			active_settings_subsection = highlighted_settings_subsection = 0;
			hovering_pause_menu_item = PAUSE_MENU_CHEATS;
			break;
		case SETTINGS_MENU_BACK:
			reset_paused_menu();
			hovering_pause_menu_item = PAUSE_MENU_SETTINGS;
			break;
	}
	clear_menu_controls(); // prevent "click-through" because the screen changes
}

static void draw_pause_menu_item(pause_menu_item_type* item, rect_type* parent, int* y_offset, int inactive_text_color) {
	rect_type text_rect = *parent;
	text_rect.top += *y_offset;
	int text_color = inactive_text_color;

	// SDLPoP2: an unavailable item (quicksave off, a replay playing) stays in its place, greyed out and inert
	// (SDLPoP leaves it out); navigation skips it as in SDLPoP
	if (item->required != NULL && *item->required == 0) {
		if (hovering_pause_menu_item == item->id) hovering_pause_menu_item = PAUSE_MENU_RESUME;
		if (item->id == PAUSE_MENU_CHEATS) return; // skip this item (disabled): SDLPoP's way, for CHEATS (cheats off)
		show_text_with_color(&text_rect, halign_center, valign_top, item->text, color_7_lightgray);   /* (as SDLPoP's disabled settings) */
		*y_offset += 13;
		return;
	}

	rect_type selection_box = text_rect;
	selection_box.bottom = selection_box.top + 8;
	selection_box.top -= 3;

	bool_type highlighted = (hovering_pause_menu_item == item->id);
	if (have_mouse_input && is_mouse_over_rect(&selection_box)) {
		hovering_pause_menu_item = item->id;
		highlighted = 1;
	}

	if (highlighted) {
		previous_pause_menu_item = item->previous;
		next_pause_menu_item = item->next;
		// Skip over disabled items (such as the CHEATS menu in non-cheat mode)
		if (previous_pause_menu_item->required != NULL) {
			while (*previous_pause_menu_item->required == 0) {
				previous_pause_menu_item = previous_pause_menu_item->previous;
				if (previous_pause_menu_item->required == NULL) break;
			}
		}
		if (next_pause_menu_item->required != NULL) {
			while (*next_pause_menu_item->required == 0) {
				next_pause_menu_item = next_pause_menu_item->next;
				if (next_pause_menu_item->required == NULL) break;
			}
		}
		text_color = color_15_brightwhite;
		draw_rect_contours(&selection_box, color_7_lightgray);

		if (mouse_clicked) {
			if (is_mouse_over_rect(&selection_box)) {
				pause_menu_clicked(item);
			}
		} else if (pressed_enter && (drawn_menu == 0 || (drawn_menu >= 1 && controlled_area == 0))) { // (SDLPoP2: 2 the CHEATS page)
			pause_menu_clicked(item);
		}

	}
	show_text_with_color(&text_rect, halign_center, valign_top, item->text, text_color);
	*y_offset += 13;

}

static void draw_pause_menu(void) {
	pause_menu_alpha = 120;
	draw_rect_with_alpha(&screen_rect, color_0_black, pause_menu_alpha);
	draw_rect_with_alpha(&rect_bottom_text, color_0_black, 0); // Transparent so that the text "GAME PAUSED" is visible.
	rect_type pause_rect_outer = {0, 110, 192, 210};
	rect_type pause_rect_inner;
	shrink2_rect(&pause_rect_inner, &pause_rect_outer, 5, 5);

	if (!have_mouse_input) {
		if (menu_control_y == 1) {
			play_menu_sound(sound_21_loose_shake_2);
			hovering_pause_menu_item = next_pause_menu_item->id;
		} else if (menu_control_y == -1) {
			play_menu_sound(sound_21_loose_shake_2);
			hovering_pause_menu_item = previous_pause_menu_item->id;
		}
	}

	int y_offset = 50;
	for (int i = 0; i < COUNT(pause_menu_items); ++i) {
		draw_pause_menu_item(&pause_menu_items[i], &pause_rect_inner, &y_offset, color_15_brightwhite);
	}
}

static bool_type were_settings_changed;

// SDLPoP2: the frontend applies what changed (SDLPoP's turn_setting_on_off does it itself: SDL_SetWindowFullscreen,
// apply_aspect_ratio, turn_sound_on_off, ...); a changed setting of the game's installs the settings (the core reads
// them through pop2_settings_game)
static int setting_apply_group(int setting_id) {
	switch (setting_id) {
		default: return 0;
		case SETTING_FULLSCREEN: return OVERLAY_MENU_APPLY_FULLSCREEN;
		case SETTING_USE_CORRECT_ASPECT_RATIO: case SETTING_USE_INTEGER_SCALING: case SETTING_SCALING_TYPE: return OVERLAY_MENU_APPLY_VIDEO;
		case SETTING_ENABLE_SOUND: case SETTING_ENABLE_MUSIC: case SETTING_VOLUME: return OVERLAY_MENU_APPLY_AUDIO;
		case SETTING_SHOW_MENU_ON_PAUSE: case SETTING_ENABLE_CONTROLLER: case SETTING_ENABLE_CONTROLLER_RUMBLE:
		case SETTING_JOYSTICK_THRESHOLD: case SETTING_JOYSTICK_ONLY_HORIZONTAL: return OVERLAY_MENU_APPLY_CONTROLLER;
		case SETTING_KEY_LEFT: case SETTING_KEY_RIGHT: case SETTING_KEY_UP: case SETTING_KEY_DOWN: case SETTING_KEY_UPLEFT:
		case SETTING_KEY_UPRIGHT: case SETTING_KEY_DOWNLEFT: case SETTING_KEY_DOWNRIGHT: case SETTING_KEY_SHIFT:
		case SETTING_KEY_CTRL: return OVERLAY_MENU_APPLY_KEYS;
		case SETTING_ENABLE_CHEATS: return OVERLAY_MENU_APPLY_CHEATS;
	}
}
static void apply_setting(setting_type* setting) {
	if (setting->gameplay) pop2_settings_game = S;
	int what = setting_apply_group(setting->id);
	if (what && host.apply) host.apply(what);
}

static void turn_setting_on_off(setting_type* setting, byte new_state) {
	if (setting->id != SETTING_ENABLE_CHEATS) were_settings_changed = 1; // SDLPoP2: (the cheats: nothing to save)
	if (setting->linked != NULL) {
		*(int*)(setting->linked) = new_state;
	}
	apply_setting(setting);
}

static void turn_setting_on_off_with_sound(setting_type* setting, byte new_state) {
	play_menu_sound(sound_10_sword_vs_sword);
	turn_setting_on_off(setting, new_state);

}

static int get_setting_value(setting_type* setting) {
	int value = 0;
	if (setting->linked != NULL) {
		switch(setting->number_type) {
			default:
			case SETTING_BYTE:
				value = *(byte*) setting->linked;
				break;
			case SETTING_SBYTE:
				value = *(sbyte*) setting->linked;
				break;
			case SETTING_WORD:
				value = *(word*) setting->linked;
				break;
			case SETTING_SHORT:
				value = *(short*) setting->linked;
				break;
			case SETTING_INT:
				value = *(int*) setting->linked;
				break;
		}
		if (setting->id == SETTING_SWORD_TYPE && value == SWORD_NONE) value = 0;   // SDLPoP2: none, 1, 2
	}
	return value;
}

static void set_setting_value(setting_type* setting, int value) {
	if (setting->linked != NULL) {
		if (setting->id == SETTING_SWORD_TYPE && value == 0) value = SWORD_NONE;   // SDLPoP2
		switch(setting->number_type) {
			default:
			case SETTING_BYTE:
				*(byte*) setting->linked = (byte) value;
				break;
			case SETTING_SBYTE:
				*(sbyte*) setting->linked = (sbyte) value;
				break;
			case SETTING_WORD:
				*(word*) setting->linked = (word) value;
				break;
			case SETTING_SHORT:
				*(short*) setting->linked = (short) value;
				break;
			case SETTING_INT:
				*(int*) setting->linked = value;
				break;
		}
		apply_setting(setting);
	}
}

static void increase_setting(setting_type* setting, int old_value) {
	int new_value;
	if (setting->id == SETTING_JOYSTICK_THRESHOLD) {
		new_value = ((old_value / 1000) + 1) * 1000; // Nearest higher multiple of 1000.
	} else {
		new_value = old_value + 1;
	}
	if (setting->linked != NULL && new_value <= setting->max) {
		were_settings_changed = 1;
		set_setting_value(setting, new_value);
	}
}

static void decrease_setting(setting_type* setting, int old_value) {
	int new_value;
	if (setting->id == SETTING_JOYSTICK_THRESHOLD) {
		new_value = (((old_value+999) / 1000) - 1) * 1000; // Nearest lower multiple of 1000.
	} else {
		new_value = old_value - 1;
	}
	if (setting->linked != NULL && new_value >= setting->min) {
		were_settings_changed = 1;
		set_setting_value(setting, new_value);
	}
}


static void draw_setting_explanation(setting_type* setting) {
	// SDLPoP2: a setting of the game's, disabled while a replay is recorded or played back, says so
	char text[512];
	if (setting->gameplay && !gameplay_settings_editable) {
		snprintf(text, sizeof(text), "%s\n(Not while a replay is %s.)", setting->explanation, replaying_replay ? "played back" : "recorded");
	} else if (setting->required == &cheats_editable && !cheats_editable) {   // (the cheats: the recording's)
		snprintf(text, sizeof(text), "%s\n(Not while a replay is played back.)", setting->explanation);
	} else {
		snprintf(text, sizeof(text), "%s", setting->explanation);
	}
	show_text_with_color(&explanation_rect, halign_center, valign_top, text, color_7_lightgray);
}

static char* print_setting_value_(setting_type* setting, int value, char* buffer, size_t buffer_size) {
	bool_type has_name = 0;
	names_list_type* list = setting->names_list;
	size_t max_len = MIN(MAX_OPTION_VALUE_NAME_LENGTH, buffer_size);
	if (list != NULL) {
		if (list->type == 0 && value >= 0 && value < list->names.count) {
			snprintf(buffer, max_len, "%s", (*(list->names.data))[value]);
			has_name = 1;
		} else if (list->type == 1) {
			for (int i = 0; i < list->kv_pairs.count; ++i) {
				const key_value_type* kv_pair = list->kv_pairs.data + i;
				if (value == kv_pair->value) {
					snprintf(buffer, max_len, "%s", kv_pair->key);
					has_name = 1;
					break;
				}
			}
		}
	}
	if (!has_name) {
		snprintf(buffer, buffer_size, "%d", value);
	}
	return buffer;
}
#define print_setting_value(setting, value) print_setting_value_(setting, value, value_text_buffer, sizeof(value_text_buffer))

static char* redefined_key;   // SDLPoP2: the key dialog (redefine_key)
static setting_type* redefined_setting;
static void redefine_key(setting_type* setting);

static void draw_setting(setting_type* setting, rect_type* parent, int* y_offset, int inactive_text_color) {
	char value_text_buffer[32];
	rect_type text_rect = *parent;
	text_rect.top += *y_offset;
	int text_color = inactive_text_color;
	int selected_color = color_15_brightwhite;
	int unselected_color = color_7_lightgray;

	rect_type setting_box = text_rect;
	setting_box.top -= 5;
	setting_box.bottom = setting_box.top + 15;
	setting_box.left -= 10;
	setting_box.right += 10;

	if (mouse_clicked && is_mouse_over_rect(&setting_box)) {
		highlighted_setting_id = setting->id;
		controlled_area = 1;
	}

	if (highlighted_setting_id == setting->id) {
		next_setting_id = setting->next;
		previous_setting_id = setting->previous;
		at_scroll_up_boundary = (setting->index == scroll_position);
		at_scroll_down_boundary = (setting->index == scroll_position + 8);

		draw_rect(&setting_box, map_rgba(55, 55, 55, 255));
		rect_type left_side_of_setting_box = setting_box;
		left_side_of_setting_box.left = setting_box.left - 2;
		left_side_of_setting_box.right = setting_box.left;
		draw_rect(&left_side_of_setting_box, color_15_brightwhite);
		draw_setting_explanation(setting);
	}

	bool_type disabled = 0;
	if (setting->required != NULL) {
		disabled = !(*setting->required);
	}
	if (disabled) {
		text_color = color_7_lightgray;
	}

	show_text_with_color(&text_rect, halign_left, valign_top, setting->text, text_color);

	if (setting->style == SETTING_STYLE_TOGGLE && !disabled) {
		bool_type setting_enabled = 1;
		if (setting->linked != NULL) {
			setting_enabled = *(int*)setting->linked;
		}

		// Toggling the setting: either by clicking on "ON" or "OFF", or by pressing left/right.
		if (highlighted_setting_id == setting->id) {
			if (mouse_clicked) {
				if (!setting_enabled) {
					rect_type ON_hitbox = setting_box;
					ON_hitbox.left = setting_box.right - 22;
					if (is_mouse_over_rect(&ON_hitbox)) {
						turn_setting_on_off_with_sound(setting, 1);
						setting_enabled = 0;
					}
				} else {
					rect_type OFF_hitbox = setting_box;
					OFF_hitbox.left = setting_box.right - 49;
					OFF_hitbox.right = setting_box.right - 22;
					if (is_mouse_over_rect(&OFF_hitbox)) {
						turn_setting_on_off_with_sound(setting, 0);
						setting_enabled = 1;
					}
				}
			} else if (setting_enabled && menu_control_x < 0) {
				turn_setting_on_off_with_sound(setting, 0);
				setting_enabled = 0;
			} else if (!setting_enabled && menu_control_x > 0) {
				turn_setting_on_off_with_sound(setting, 1);
				setting_enabled = 1;
			}
		}

		int OFF_color = (setting_enabled) ? unselected_color : selected_color;
		int ON_color = (setting_enabled) ? selected_color : unselected_color;
		show_text_with_color(&text_rect, halign_right, valign_top, "ON", ON_color);
		text_rect.right -= 15;
		show_text_with_color(&text_rect, halign_right, valign_top, "OFF", OFF_color);

	} else if (setting->style == SETTING_STYLE_NUMBER && !disabled) {
		int value = get_setting_value(setting);
		if (highlighted_setting_id == setting->id) {
			if (mouse_clicked) {

				rect_type right_hitbox = {setting_box.top, (int16_t) (text_rect.right - 5), setting_box.bottom, (int16_t) (text_rect.right + 10)};
				if (is_mouse_over_rect(&right_hitbox)) {
					increase_setting(setting, value);
				} else {
					char* value_text = print_setting_value(setting, value);
					int value_text_width = get_line_width(value_text, (int)strlen(value_text));
					rect_type left_hitbox = right_hitbox;
					left_hitbox.left -= (value_text_width + 10);
					left_hitbox.right -= (value_text_width + 5);
					if (is_mouse_over_rect(&left_hitbox)) {
						decrease_setting(setting, value);
					}
				}

			} else if (menu_control_x > 0) {
				increase_setting(setting, value);
			} else if (menu_control_x < 0) {
				decrease_setting(setting, value);
			}
		}

		value = get_setting_value(setting); // May have been updated.
		char* value_text = print_setting_value(setting, value);
		show_text_with_color(&text_rect, halign_right, valign_top, value_text, selected_color);

		if (highlighted_setting_id == setting->id) {
			int value_text_width = get_line_width(value_text, (int)strlen(value_text));
			draw_image_with_blending(arrowhead_right_image, text_rect.right + 2, text_rect.top);
			draw_image_with_blending(arrowhead_left_image, text_rect.right - value_text_width - 6, text_rect.top);
		}

	} else if (setting->style == SETTING_STYLE_CHEAT) {
		// SDLPoP2: an entry of the CHEATS page: its key on the right (as the CONTROLS page's keys); chosen, the menu
		// closes and the key goes to the game
		const cheat_type* cheat = (const cheat_type*) setting->linked;
		char key_text[32];
		overlay_menu_key_label(cheat->key, key_text, sizeof(key_text));
		show_text_with_color(&text_rect, halign_right, valign_top, key_text, disabled ? unselected_color : selected_color);
		if (highlighted_setting_id == setting->id && !disabled) {
			if (pressed_enter || (mouse_clicked && is_mouse_over_rect(&setting_box))) {
				play_menu_sound(sound_22_loose_shake_3);
				switch (cheat->action) {
					default:
					case CHEAT_ACTION_TYPE_KEY:
						menu_action = OVERLAY_MENU_CHEAT;
						cheat_key_chosen = cheat->key;
						need_close_menu = 1;
						break;
				}
			}
		}

	} else if (setting->style == SETTING_STYLE_KEY && !disabled) {
		// SDLPoP2: the key is SDLPoP2.ini's SDL scancode name
		int value = SDL_GetScancodeFromName((const char*) setting->linked);
		char value_text[256];
		snprintf(value_text, sizeof(value_text), "%s (%d)", SDL_GetScancodeName((SDL_Scancode) value), value);
		show_text_with_color(&text_rect, 1, -1, value_text, selected_color);

	} else {
		// show text only
		if (highlighted_setting_id == setting->id && (setting->required == NULL || *setting->required != 0)) {
			if (pressed_enter || (mouse_clicked && is_mouse_over_rect(&setting_box))) {
				if (setting->id == SETTING_RESET_ALL_SETTINGS) {
					play_menu_sound(sound_22_loose_shake_3);
					current_dialog_box = DIALOG_RESTORE_DEFAULT_SETTINGS;
					current_dialog_text = "Restore all settings to their default values?";
				} else if (setting->id == SETTING_LEVEL_SETTINGS) {
					play_menu_sound(sound_22_loose_shake_3);
					current_dialog_box = DIALOG_SELECT_LEVEL;
				} else if (setting->id == SETTING_SKILL_SETTINGS) {   // SDLPoP2
					play_menu_sound(sound_22_loose_shake_3);
					current_dialog_box = DIALOG_SELECT_SKILL;
				}
			}

		}
	}

	*y_offset += 15;
}

// Handle the redefining of keyboard controls separately from drawing the menu.
// Otherwise the menu items under the current one will not be drawn until the dialog is closed.
static void handle_setting(setting_type* setting, rect_type* parent, int* y_offset, int inactive_text_color) {
	(void) inactive_text_color;
	rect_type text_rect = *parent;
	text_rect.top += *y_offset;

	rect_type setting_box = text_rect;
	setting_box.top -= 5;
	setting_box.bottom = setting_box.top + 15;
	setting_box.left -= 10;
	setting_box.right += 10;

	bool_type disabled = 0;
	if (setting->required != NULL) {
		disabled = !(*setting->required);
	}

	if (setting->style == SETTING_STYLE_KEY && !disabled) {
		if (highlighted_setting_id == setting->id) {
			if (pressed_enter || (mouse_clicked && is_mouse_over_rect(&setting_box))) {
				// SDLPoP2: the dialog waits for the key in the next passes (the key dialog); were_settings_changed there
				redefine_key(setting);
			}
		}
	}

	*y_offset += 15;
}

static void menu_scroll(int y) {
	settings_area_type* current_settings_area = get_settings_area(active_settings_subsection);
	if (current_settings_area != NULL) {
		int max_scroll = MAX(0, current_settings_area->setting_count - 9);
		if (drawn_menu >= 1 && controlled_area == 1) { // (SDLPoP2: 2 the CHEATS page)
			if (y < 0 && scroll_position > 0) {
				--scroll_position;
			} else if (y > 0 && scroll_position < max_scroll) {
				++scroll_position;
			}
		}
	}
}

static void draw_settings_area(settings_area_type* settings_area) {
	if (settings_area == NULL) return;
	rect_type settings_area_rect = {0, 80, 170, 320};
	shrink2_rect(&settings_area_rect, &settings_area_rect, 20, 20);

	int start_y_offset = 0;

	// The MODS subsection with level specific settings is a special case:
	// We want to display which level we are editing, and start drawing slightly lower.
	if (active_settings_subsection == SETTINGS_MENU_LEVEL_CUSTOMIZATION) {
		start_y_offset = 15;
		char level_text[16];
		snprintf(level_text, sizeof(level_text), "LEVEL %d", menu_current_level);
		show_text_with_color(&settings_area_rect, halign_center, valign_top, level_text, color_15_brightwhite);
	}
	// SDLPoP2: the same for the guard skill page
	if (active_settings_subsection == SETTINGS_MENU_SKILL_CUSTOMIZATION) {
		start_y_offset = 15;
		char skill_text[16];
		snprintf(skill_text, sizeof(skill_text), "SKILL %d", menu_current_skill);
		show_text_with_color(&settings_area_rect, halign_center, valign_top, skill_text, color_15_brightwhite);
	}

	int y_offset = start_y_offset;
	int num_drawn_settings = 0;
	for (int i = 0; (i < settings_area->setting_count) && (num_drawn_settings < 9); ++i) {
		if (i >= scroll_position) {
			++num_drawn_settings;
			draw_setting(&settings_area->settings[i], &settings_area_rect, &y_offset, color_15_brightwhite);
		}
	}

	y_offset = start_y_offset;
	num_drawn_settings = 0;
	for (int i = 0; (i < settings_area->setting_count) && (num_drawn_settings < 9); ++i) {
		if (i >= scroll_position) {
			++num_drawn_settings;
			handle_setting(&settings_area->settings[i], &settings_area_rect, &y_offset, color_15_brightwhite);
		}
	}

	if (scroll_position > 0) {
		draw_image_with_blending(arrowhead_up_image, 200, 10);
	}
	if (scroll_position + num_drawn_settings < settings_area->setting_count) {
		draw_image_with_blending(arrowhead_down_image, 200, 151);
	}

	// Draw a scroll bar if needed.
	// It's not clickable yet, it just shows where you are in the list.
	if (num_drawn_settings < settings_area->setting_count) {
		const int scrollbar_width = 2;
		rect_type scrollbar_rect = {
			.top = (int16_t) (settings_area_rect.top - 5), .bottom = settings_area_rect.bottom,
			.left = (int16_t) (settings_area_rect.right + 10 - scrollbar_width), .right = (int16_t) (settings_area_rect.right + 10)
		};
		method_5_rect(&scrollbar_rect, blitters_0_no_transp, color_8_darkgray);

		int scrollbar_height = scrollbar_rect.bottom - scrollbar_rect.top;
		rect_type scrollbar_slider_rect = {
			.top = (int16_t) (scrollbar_rect.top + scroll_position * scrollbar_height / settings_area->setting_count),
			.bottom = (int16_t) (scrollbar_rect.top + (scroll_position + num_drawn_settings) * scrollbar_height / settings_area->setting_count),
			.left = scrollbar_rect.left, .right = scrollbar_rect.right
		};
		method_5_rect(&scrollbar_slider_rect, blitters_0_no_transp, color_7_lightgray);
	}
}

static void draw_settings_menu(void) {
	settings_area_type* settings_area = get_settings_area(active_settings_subsection);
	pause_menu_alpha = (settings_area == NULL) ? 220 : 255;
	draw_rect_with_alpha(&screen_rect, color_0_black, pause_menu_alpha);

	rect_type pause_rect_outer = {0, 10, 192, 80};
	rect_type pause_rect_inner;
	shrink2_rect(&pause_rect_inner, &pause_rect_outer, 5, 5);

	if (!have_mouse_input) {
		bool_type hovering_item_changed = 0;
		if (controlled_area == 0) {
			int old_hovering_item_id = hovering_pause_menu_item;
			if (menu_control_y == 1) {
				hovering_pause_menu_item = next_pause_menu_item->id;
			} else if (menu_control_y == -1) {
				hovering_pause_menu_item = previous_pause_menu_item->id;
			}
			if (old_hovering_item_id != hovering_pause_menu_item) {
				hovering_item_changed = 1;
			}
		} else if (controlled_area == 1) {
			// settings area
			int old_highlighted_setting_id = highlighted_setting_id;

			// Why does the global variable contain the ID instead of the index?...
			// Find the index from the ID.
			settings_area_type* current_settings_area = get_settings_area(active_settings_subsection);
			int highlighted_setting_index = -1;
			for (int i = 0; i < current_settings_area->setting_count; i++) {
				if (highlighted_setting_id == current_settings_area->settings[i].id) {
					highlighted_setting_index = i;
					break;
				}
			}

			int last = current_settings_area->setting_count - 1;
			int max_scroll = MAX(0, current_settings_area->setting_count - 9);

			if (menu_control_y > 0) {
				// DOWN
				highlighted_setting_index += menu_control_y;
				if (highlighted_setting_index > last) highlighted_setting_index = last;

				// With Page Down, try to leave the selection in the same row visually.
				if (menu_control_y > +1) scroll_position += menu_control_y;

			} else if (menu_control_y < 0) {
				// UP
				highlighted_setting_index += menu_control_y;
				if (highlighted_setting_index < 0) highlighted_setting_index = 0;

				// With Page Up, try to leave the selection in the same row visually.
				if (menu_control_y < -1) scroll_position += menu_control_y;

			}

			if (menu_control_y != 0) {
				// We check both directions in both cases, to scroll the highlighted row back into sight even if the user scrolled it out of sight (with the mouse wheel).
				if (highlighted_setting_index - 8 > scroll_position) scroll_position = highlighted_setting_index - 8;
				if (highlighted_setting_index < scroll_position) scroll_position = highlighted_setting_index;
				if (scroll_position > max_scroll) scroll_position = max_scroll;
				if (scroll_position < 0) scroll_position = 0;
			}

			// Find the ID from the index.
			if (highlighted_setting_index < 0) highlighted_setting_index = 0;   // SDLPoP2: (not found)
			highlighted_setting_id = current_settings_area->settings[highlighted_setting_index].id;

			if (old_highlighted_setting_id != highlighted_setting_id) {
				hovering_item_changed = 1;
			}
		}
		if (hovering_item_changed) {
			play_menu_sound(sound_21_loose_shake_2);
		}
	}

	// SDLPoP2: the CHEATS page has its own left part
	pause_menu_item_type* left_items = (drawn_menu == 2) ? cheats_menu_items : settings_menu_items;
	int left_item_count = (drawn_menu == 2) ? COUNT(cheats_menu_items) : COUNT(settings_menu_items);
	int y_offset = 50;
	for (int i = 0; i < left_item_count; ++i) {
		pause_menu_item_type* item = &left_items[i];
		int text_color = (highlighted_settings_subsection == item->id) ? color_15_brightwhite : color_7_lightgray;
		draw_pause_menu_item(&left_items[i], &pause_rect_inner, &y_offset, text_color);
	}

	draw_settings_area(settings_area);
}

enum dialog_button_ids {
	DIALOG_BUTTON_CANCEL,
	DIALOG_BUTTON_OK,
};

static void set_options_to_default(void);

static void confirmation_dialog_result(int which_dialog, int button) {
	if (button == DIALOG_BUTTON_OK) {
		if (which_dialog == DIALOG_RESTORE_DEFAULT_SETTINGS) {
			play_menu_sound(sound_10_sword_vs_sword);
			were_settings_changed = 1;
			set_options_to_default();
			// SDLPoP2: the frontend applies them all (SDLPoP: integer scaling, lighting, aspect ratio, sound, music)
			if (host.apply) host.apply(OVERLAY_MENU_APPLY_VIDEO | OVERLAY_MENU_APPLY_AUDIO | OVERLAY_MENU_APPLY_KEYS | OVERLAY_MENU_APPLY_CONTROLLER);
		} else if (which_dialog == DIALOG_CONFIRM_QUIT) {
			// SDLPoP: last_key_scancode = Ctrl+Q; key_test_quit(); SDLPoP2: the frontend ends the program
			menu_action = OVERLAY_MENU_QUIT;
			need_close_menu = 1;
		}
	} else {
		play_menu_sound(sound_22_loose_shake_3);
	}
}

static rect_type cancel_text_rect = {104, 162,  118,  212};
static rect_type cancel_highlight_rect = {103, 162,  116,  212};
static rect_type ok_text_rect = {104, 108,  118,  158};
static rect_type ok_highlight_rect = {103, 108,  116,  158};

// SDLPoP2: draw_confirmation_dialog's loop, one pass at a time (the dialog's locals kept between passes).
// 0: the dialog is done.
static int highlighted_button = DIALOG_BUTTON_OK;
static int old_highlighted_button = -1;
static int draw_confirmation_dialog(int which_dialog, const char* text) {
	{
		key_test_paused_menu(last_key_scancode);
		process_additional_menu_input();

		if (menu_control_back == 1) {
			confirmation_dialog_result(which_dialog, DIALOG_BUTTON_CANCEL);
			goto done;
		}

		if (have_mouse_input) {
			if (is_mouse_over_rect(&ok_highlight_rect)) {
				highlighted_button = DIALOG_BUTTON_OK;
			} else if (is_mouse_over_rect(&cancel_highlight_rect)) {
				highlighted_button = DIALOG_BUTTON_CANCEL;
			}
		}

		if (menu_control_x < 0) {
			highlighted_button = DIALOG_BUTTON_OK;
		} else if (menu_control_x > 0) {
			highlighted_button = DIALOG_BUTTON_CANCEL;
		} else if (mouse_clicked || pressed_enter) {
			confirmation_dialog_result(which_dialog, highlighted_button);
			goto done;
		}

		if (highlighted_button != old_highlighted_button) {
			old_highlighted_button = highlighted_button;
			// Need to redraw the dialog box.
			draw_rect(&screen_rect, color_0_black);
			draw_rect(&copyprot_dialog->peel_rect, color_0_black);
			dialog_method_2_frame(copyprot_dialog);
			rect_type rect;
			shrink2_rect(&rect, &copyprot_dialog->text_rect, 2, 1);
			rect.bottom -= 14;
			show_text_with_color(&rect, halign_center, valign_middle, text, color_15_brightwhite);

			rect_type* highlight_rect;
			int ok_text_color, cancel_text_color;
			if (highlighted_button == DIALOG_BUTTON_OK) {
				highlight_rect = &ok_highlight_rect;
				ok_text_color = color_15_brightwhite;
				cancel_text_color = color_7_lightgray;
			} else {
				highlight_rect = &cancel_highlight_rect;
				ok_text_color = color_7_lightgray;
				cancel_text_color = color_15_brightwhite;
			}
			draw_rect(highlight_rect, color_8_darkgray);
			show_text_with_color(&ok_text_rect, halign_center, valign_middle, "OK", ok_text_color);
			show_text_with_color(&cancel_text_rect, halign_center, valign_middle, "Cancel", cancel_text_color);
		}
		return 1;
	}
done:
	current_dialog_box = 0;
	clear_menu_controls();
	return 0;
}

// SDLPoP2: draw_select_level_dialog's loop, one pass at a time; also for the guard skills (DIALOG_SELECT_SKILL)
static int old_edited_level_number = -1;
static int draw_select_level_dialog(void) {
	bool_type skill = current_dialog_box == DIALOG_SELECT_SKILL;
	word* edited = skill ? &menu_current_skill : &menu_current_level;
	int min_number = skill ? 0 : 1, max_number = skill ? SETTINGS_SKILLS - 1 : SETTINGS_LEVELS;   // (SDLPoP: levels 0..15)
	{
		key_test_paused_menu(last_key_scancode);
		process_additional_menu_input();

		if (menu_control_back == 1) {
			menu_control_back = 0;
			play_menu_sound(sound_22_loose_shake_3);
			goto done;
		}

		if (menu_control_x < 0) {
			*edited = (word) MAX(min_number, *edited - 1);
		} else if (menu_control_x > 0) {
			*edited = (word) MIN(max_number, *edited + 1);
		} else if (mouse_clicked || pressed_enter) {
			enter_settings_subsection(skill ? SETTINGS_MENU_SKILL_CUSTOMIZATION : SETTINGS_MENU_LEVEL_CUSTOMIZATION);
			highlighted_settings_subsection = SETTINGS_MENU_MODS;
			play_menu_sound(sound_22_loose_shake_3);
			goto done;
		}

		if (*edited != old_edited_level_number) {
			word saved_font = textstate.ptr_font;
			textstate.ptr_font = hc_font;

			old_edited_level_number = *edited;
			// Need to redraw the dialog box.
			draw_rect(&screen_rect, color_0_black);
			draw_rect(&copyprot_dialog->peel_rect, color_0_black);
			dialog_method_2_frame(copyprot_dialog);
			rect_type rect;
			shrink2_rect(&rect, &copyprot_dialog->text_rect, 2, 1);
			rect.bottom -= 14;
			show_text_with_color(&rect, halign_center, valign_middle, skill ? "Customize guard skill..." : "Customize level...", color_15_brightwhite);
			rect_type input_rect = {104,   64,  118,  256};
			char level_text[8];
			snprintf(level_text, sizeof(level_text), "%d", *edited);
			show_text_with_color(&input_rect, halign_center, valign_middle, level_text, color_15_brightwhite);
			draw_image_with_blending(arrowhead_right_image, 175, input_rect.top + 3);
			draw_image_with_blending(arrowhead_left_image, 145 - 3, input_rect.top + 3);

			textstate.ptr_font = saved_font;
		}
		return 1;
	}
done:
	current_dialog_box = 0;
	clear_menu_controls();
	return 0;
}

// seg000.c: redefine_key and showmessage_any_key. SDLPoP2: the dialog is drawn now; the next passes wait for the key
// (draw_redefine_key_dialog). SDLPoP2 keeps the keys as SDL scancode names (SDLPoP2.ini's key_*).
static void redefine_key(setting_type* setting) {
	char message[256];
	snprintf(message, sizeof(message), "Redefining keys:\nPress key for \"%s\".\nOr press Esc to cancel.", setting->text);

	// Use the regular big font for the dialog instead of the small menu font.
	word saved_font = textstate.ptr_font;
	textstate.ptr_font = hc_font;

	// showmessage_any_key
	rect_type rect;
	draw_rect(&copyprot_dialog->peel_rect, color_0_black);   // SDLPoP2: (SDLPoP leaves the menu's text inside)
	dialog_method_2_frame(copyprot_dialog);
	shrink2_rect(&rect, &copyprot_dialog->text_rect, 2, 1);
	show_text_with_color(&rect, 0, 0, message, color_15_brightwhite);
	last_any_key_scancode = 0; // Don't leak the Enter keypress (which opened the dialog) into the dialog.

	// Switch back to the menu font.
	textstate.ptr_font = saved_font;

	redefined_setting = setting;
	redefined_key = (char*) setting->linked;
	current_dialog_box = DIALOG_REDEFINE_KEY;
}
static int draw_redefine_key_dialog(void) {
	int new_key = last_any_key_scancode; // Press any key to continue...
	last_any_key_scancode = 0;
	if (new_key == 0) return 1;
	if (new_key != SDL_SCANCODE_ESCAPE) {
		char before[32];
		snprintf(before, sizeof(before), "%s", redefined_key);
		snprintf(redefined_key, sizeof(S->keys[0]), "%s", SDL_GetScancodeName((SDL_Scancode) new_key));
		// This should be in redefine_key()?
		if (strcmp(before, redefined_key) != 0) were_settings_changed = 1;
		apply_setting(redefined_setting);
	}
	current_dialog_box = 0;
	clear_menu_controls();
	return 0;
}

static int need_full_menu_redraw_count;
static int started_dialog_box;   // SDLPoP2: the dialog whose loop runs

static void menu_was_closed(void);

// SDLPoP2: one pass of draw_menu's loop (SDLPoP: while (!need_close_menu) { ... }); overlay_menu_frame runs the passes.
static void draw_menu_pass(void) {
	gport* saved_target_surface = current_target_surface;
	current_target_surface = overlay_surface;

	// (clear_menu_controls, process_events: done by overlay_menu_frame)
	if (current_dialog_box != DIALOG_NONE) {
		// SDLPoP2: the dialog's loop runs instead of process_key (it reads the keys itself); its locals start afresh
		if (current_dialog_box != started_dialog_box) {
			started_dialog_box = current_dialog_box;
			highlighted_button = DIALOG_BUTTON_OK;
			old_highlighted_button = -1;
			old_edited_level_number = -1;
		}
		int still_open;
		if (current_dialog_box == DIALOG_SELECT_LEVEL || current_dialog_box == DIALOG_SELECT_SKILL) {
			still_open = draw_select_level_dialog();
		} else if (current_dialog_box == DIALOG_REDEFINE_KEY) {
			still_open = draw_redefine_key_dialog();
		} else {
			still_open = draw_confirmation_dialog(current_dialog_box, current_dialog_text);
		}
		if (still_open) goto out;
		started_dialog_box = DIALOG_NONE;
		current_dialog_box = DIALOG_NONE;
		clear_menu_controls();
	} else {
		// process_key
		int key = key_test_paused_menu(last_key_scancode);
		if (key != 0) {
			goto out; // Menu was forcefully closed, for example by pressing Ctrl+A.
		}
		process_additional_menu_input();
	}

	if (is_menu_shown == 1) {
		is_menu_shown = -1; // reset the menu if the menu is drawn for the first time
		need_full_menu_redraw_count = 2;
		reset_paused_menu();
	}
	if (menu_control_back == 1) {
		play_menu_sound(sound_22_loose_shake_3);
		if (drawn_menu == 1) {
			if (controlled_area == 1) {
				leave_settings_subsection();
			} else {
				reset_paused_menu(); // Go back to the top level pause menu.
				hovering_pause_menu_item = PAUSE_MENU_SETTINGS;
			}
		} else if (drawn_menu == 2) { // SDLPoP2: the CHEATS page, as the settings page
			if (controlled_area == 1) {
				leave_settings_subsection();
			} else {
				reset_paused_menu();
				hovering_pause_menu_item = PAUSE_MENU_CHEATS;
			}
		} else {
			need_close_menu = 1; // Close the menu.
			goto out;
		}
	}

	if (menu_control_scroll_y != 0) {
		menu_scroll(menu_control_scroll_y);
	}

	if (have_mouse_input || have_keyboard_or_controller_input) {
		// The menu is updated+drawn within the same routine, so redrawing may be necessary after the first time.
		// TODO: Maybe in the future fully separate updating from drawing?
		need_full_menu_redraw_count = 2;
	} else {
		if (need_full_menu_redraw_count == 0) {
			goto out; // Don't redraw if there is no input to process (save CPU cycles).
		}
	}

	{
		word saved_font = textstate.ptr_font;
		textstate.ptr_font = hc_small_font;
		if (drawn_menu == 0) {
			draw_pause_menu();
		} else if (drawn_menu == 1 || drawn_menu == 2) { // (SDLPoP2: 2 the CHEATS page, drawn as the settings)
			draw_settings_menu();
		}
		textstate.ptr_font = saved_font;
	}

	--need_full_menu_redraw_count;
out:
	current_target_surface = saved_target_surface;
}

static void clear_menu_controls(void) {
	pressed_enter = 0;
	mouse_moved = 0;
	mouse_clicked = 0;
	mouse_button_clicked_right = 0;
	have_mouse_input = 0;
	have_keyboard_or_controller_input = 0;
	menu_control_x = 0;
	menu_control_y = 0;
	menu_control_back = 0;
	menu_control_scroll_y = 0;
}

static void process_additional_menu_input(void) {
	read_mouse_state();
	have_keyboard_or_controller_input = (menu_control_x || menu_control_y || menu_control_back || pressed_enter);
	have_mouse_input = (mouse_moved || mouse_clicked || mouse_button_clicked_right || menu_control_scroll_y);

	if (host.window == NULL) return;
	dword flags = SDL_GetWindowFlags(host.window);
	if (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) {
		if (have_mouse_input) {
			SDL_ShowCursor(SDL_ENABLE);
		} else if (have_keyboard_or_controller_input) {
			SDL_ShowCursor(SDL_DISABLE);
		}
	} else {
		SDL_ShowCursor(SDL_ENABLE);
	}
}

static bool_type joy_ABXY_buttons_released;
static bool_type joy_xy_released;
static dword joy_xy_timeout_counter;
static bool_type joy_menu_button_released;   // SDLPoP2: button_menu (SDLPoP: Start / Back -> Backspace)

#define CB(button) (1u << (button))
static int key_test_paused_menu(int key) {
	menu_control_x = 0;
	menu_control_y = 0;
	menu_control_back = 0;

	if (mouse_button_clicked_right) {
		menu_control_back = 1; // Can use RMB to close menus.
	}

	is_joyst_mode = controller_count() > 0;
	if (is_joyst_mode && !joystick_read_this_frame) {   // SDLPoP2: once per video frame
		joystick_read_this_frame = 1;
		int joy_axis_x, joy_axis_y;
		dword held = controller_held(&joy_axis_x, &joy_axis_y);
		int joy_x = 0;
		int joy_y = 0;
		if (held & CB(SDL_CONTROLLER_BUTTON_DPAD_LEFT))
			joy_x = -1;
		else if (held & CB(SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
			joy_x = 1;
		if (held & CB(SDL_CONTROLLER_BUTTON_DPAD_UP))
			joy_y = -1;
		else if (held & CB(SDL_CONTROLLER_BUTTON_DPAD_DOWN))
			joy_y = 1;
		int y_threshold = 14000;
		int x_threshold = 26000; // Less sensitive, to prevent accidentally changing a setting.
		if (joy_axis_y < -y_threshold) {
			joy_y = -1;
		} else if (joy_axis_y > y_threshold) {
			joy_y = 1;
		} else if (joy_axis_x < -x_threshold) {
			joy_x = -1;
		} else if (joy_axis_x > x_threshold) {
			joy_x = 1;
		}

		// SDLPoP2: button_menu, pressed: SDLPoP's Start / Back give Backspace (close the menu, go back)
		if (!(held & controller_menu_buttons())) {
			joy_menu_button_released = 1;
		} else if (joy_menu_button_released) {
			joy_menu_button_released = 0;
			if (key == 0) key = SDL_SCANCODE_BACKSPACE;
		}

		dword needed_timeout = 7; // Delay for hold-down repeated input. (SDLPoP2: 0.1 s in video frames)
		if (joy_x == 0 && joy_y == 0) {
			joy_xy_released = 1;
			joy_xy_timeout_counter = 0;
		} else {
			if (joy_xy_released) {
				needed_timeout = 21; // The delay is longer for the first repetition. (0.3 s)
				joy_xy_released = 0;
			}
			dword current_counter = frame_counter;
			if (current_counter > joy_xy_timeout_counter) {
				menu_control_x = joy_x;
				menu_control_y = joy_y;
				joy_xy_timeout_counter = current_counter + needed_timeout;
				return 0; // cancel other input.
			}
		}

		if (!(held & CB(SDL_CONTROLLER_BUTTON_A)) && !(held & CB(SDL_CONTROLLER_BUTTON_Y)) && !(held & CB(SDL_CONTROLLER_BUTTON_B))) {
			joy_ABXY_buttons_released = 1;
		} else if (joy_ABXY_buttons_released) {
			joy_ABXY_buttons_released = 0;
			if (held & CB(SDL_CONTROLLER_BUTTON_A)) {
				key = SDL_SCANCODE_RETURN;
			} else if (held & CB(SDL_CONTROLLER_BUTTON_B)) {
				key = SDL_SCANCODE_ESCAPE;
			}
		}
	}

	// remap (SDLPoP2: the key_* keys of SDLPoP2.ini)
	if (key != 0 && key == (int) SDL_GetScancodeFromName(S->keys[KEY_UP])) key = SDL_SCANCODE_UP; else
	if (key != 0 && key == (int) SDL_GetScancodeFromName(S->keys[KEY_DOWN])) key = SDL_SCANCODE_DOWN; else
	if (key != 0 && key == (int) SDL_GetScancodeFromName(S->keys[KEY_LEFT])) key = SDL_SCANCODE_LEFT; else
	if (key != 0 && key == (int) SDL_GetScancodeFromName(S->keys[KEY_RIGHT])) key = SDL_SCANCODE_RIGHT; else
	/*nothing*/;

	switch(key) {
		default:
			if (key & (WITH_CTRL | WITH_ALT)) {   // SDLPoP2: PoP2's commands are Alt+ keys (and Ctrl+Q)
				need_close_menu = 1;
				menu_action = OVERLAY_MENU_KEY;
				menu_action_key = key & 0x1FFF;
				menu_action_mod = (word) (((key & WITH_CTRL) ? KMOD_LCTRL : 0) | ((key & WITH_ALT) ? KMOD_LALT : 0) | ((key & WITH_SHIFT) ? KMOD_LSHIFT : 0));
				return key; // Allow Ctrl+R, etc.
			} else {
				break;
			}
		case SDL_SCANCODE_UP:
			menu_control_y = -1;
			break;
		case SDL_SCANCODE_DOWN:
			menu_control_y = 1;
			break;
		case SDL_SCANCODE_PAGEUP:
			menu_control_y = -9;
			break;
		case SDL_SCANCODE_PAGEDOWN:
			menu_control_y = +9;
			break;
		case SDL_SCANCODE_HOME:
			menu_control_y = -1000;
			break;
		case SDL_SCANCODE_END:
			menu_control_y = +1000;
			break;
		case SDL_SCANCODE_RIGHT:
			menu_control_x = 1;
			break;
		case SDL_SCANCODE_LEFT:
			menu_control_x = -1;
			break;
		case SDL_SCANCODE_RETURN:
		case SDL_SCANCODE_SPACE:
			pressed_enter = 1;
			break;
		case SDL_SCANCODE_ESCAPE:
		case SDL_SCANCODE_BACKSPACE:
			menu_control_back = 1;
			break;
		case SDL_SCANCODE_F6:
		case SDL_SCANCODE_F6 | WITH_SHIFT:
			if (quicksave_allowed) menu_action = OVERLAY_MENU_QUICKSAVE;   // SDLPoP2: the frontend's F6
			need_close_menu = 1;
			break;
		case SDL_SCANCODE_F9:
		case SDL_SCANCODE_F9 | WITH_SHIFT:
			if (quicksave_allowed) menu_action = OVERLAY_MENU_QUICKLOAD;
			need_close_menu = 1;
			break;
	}
	return 0;
}

// SDLPoP2: set_options_to_default for the menu's settings (not those it cannot change now)
static void set_options_to_default(void) {
	pop2_settings defaults;
	settings_defaults(&defaults);
	for (int a = 0; a < COUNT(all_settings_areas); ++a) {
		settings_area_type* area = all_settings_areas[a];
		for (int i = 0; i < area->setting_count; ++i) {
			setting_type* setting = &area->settings[i];
			if (!setting->link) continue;
			if (setting->gameplay && !gameplay_settings_editable) continue;
			size_t size = setting->style == SETTING_STYLE_KEY ? sizeof(S->keys[0]) : sizeof(int);
			memcpy((char*) S + setting->link - 1, (char*) &defaults + setting->link - 1, size);
		}
	}
	if (gameplay_settings_editable) {   // [Level N], [Skill N]
		memcpy(S->sword_type, defaults.sword_type, sizeof(S->sword_type));
		memcpy(S->strikeprob, defaults.strikeprob, sizeof(S->strikeprob));
		memcpy(S->restrikeprob, defaults.restrikeprob, sizeof(S->restrikeprob));
		memcpy(S->blockprob, defaults.blockprob, sizeof(S->blockprob));
		memcpy(S->impblockprob, defaults.impblockprob, sizeof(S->impblockprob));
		memcpy(S->advprob, defaults.advprob, sizeof(S->advprob));
		memcpy(S->refractimer, defaults.refractimer, sizeof(S->refractimer));
		pop2_settings_game = S;
	}
}

/* ---- SDLPoP2.cfg (SDLPoP: SDLPoP.cfg, save_ingame_settings / load_ingame_settings) ---- */
static void write_setting_value(FILE* f, const setting_type* setting, const void* linked) {
	if (setting->style == SETTING_STYLE_KEY) {
		fprintf(f, "%s\n", (const char*) linked);
		return;
	}
	int value;
	if (setting->number_type == SETTING_WORD) value = *(const word*) linked;
	else if (setting->number_type == SETTING_BYTE) value = *(const byte*) linked;
	else value = *(const int*) linked;
	if (setting->id == SETTING_SWORD_TYPE) value = value == SWORD_NONE ? 0 : value;
	if (setting->style == SETTING_STYLE_TOGGLE) fprintf(f, "%s\n", bool_ini_values[value != 0]);
	else if (setting->ini_values) fprintf(f, "%s\n", setting->ini_values[value]);
	else fprintf(f, "%d\n", value);
}
static void write_settings(FILE* f, setting_type* settings, int count, char* section, const pop2_settings* s, int number, const char* only_section) {
	for (int i = 0; i < count; ++i) {
		setting_type* setting = &settings[i];
		if (setting->ini == NULL) continue;
		const char* slash = strchr(setting->ini, '/');
		if (only_section != NULL && (strncmp(setting->ini, only_section, (size_t)(slash - setting->ini)) != 0 || only_section[slash - setting->ini] != '\0')) continue;
		char name[64];
		snprintf(name, sizeof(name), "%.*s", (int)(slash - setting->ini), setting->ini);
		if (number >= 0) snprintf(name + strlen(name), sizeof(name) - strlen(name), " %d", number);
		const void* linked;
		const char* base = (const char*) s;
		if (setting->link) {
			linked = base + setting->link - 1;
		} else {
			if (number < 0) continue;
			switch (setting->id) {
				default: continue;
				case SETTING_SWORD_TYPE: linked = &s->sword_type[number]; break;
				case SETTING_STRIKEPROB: linked = &s->strikeprob[number]; break;
				case SETTING_RESTRIKEPROB: linked = &s->restrikeprob[number]; break;
				case SETTING_BLOCKPROB: linked = &s->blockprob[number]; break;
				case SETTING_IMPBLOCKPROB: linked = &s->impblockprob[number]; break;
				case SETTING_ADVPROB: linked = &s->advprob[number]; break;
				case SETTING_REFRACTIMER: linked = &s->refractimer[number]; break;
			}
		}
		if (strcmp(section, name) != 0) {
			fprintf(f, "\n[%s]\n", name);
			snprintf(section, 64, "%s", name);
		}
		fprintf(f, "%s = ", slash + 1);
		write_setting_value(f, setting, linked);
	}
}
int overlay_menu_save_cfg(const pop2_settings* s, const char* cfg_path) {
	if (cfg_path == NULL || cfg_path[0] == '\0') return 0;
	FILE* f = fopen(cfg_path, "w");
	if (f == NULL) return 0;
	fprintf(f, "; SDLPoP2.cfg: the settings of SDLPoP2's in-game menu (SDLPoP2.ini's syntax). They are read after SDLPoP2.ini\n"
	           "; unless SDLPoP2.ini is newer. Delete this file to go back to SDLPoP2.ini's settings.\n");
	char section[64] = "";
	static const char* const sections[] = {"General", "AdditionalFeatures", "Controller", "CustomGameplay"};   // (the ini's order)
	for (int k = 0; k < COUNT(sections); ++k) {
		for (int a = 0; a < COUNT(all_settings_areas); ++a) {
			write_settings(f, all_settings_areas[a]->settings, all_settings_areas[a]->setting_count, section, s, -1, sections[k]);
		}
	}
	for (int level = 1; level <= SETTINGS_LEVELS; ++level) {
		write_settings(f, level_settings, COUNT(level_settings), section, s, level, NULL);
	}
	for (int skill = 0; skill < SETTINGS_SKILLS; ++skill) {
		write_settings(f, skill_settings, COUNT(skill_settings), section, s, skill, NULL);
	}
	return fclose(f) == 0;
}
static void save_ingame_settings(void) {
	if (!overlay_menu_save_cfg(S, host.cfg_path) && host.cfg_path[0]) {
		fprintf(stderr, "sdlpop2: cannot write %s: the menu's settings are not saved\n", host.cfg_path);
	}
}
int overlay_menu_load_cfg(pop2_settings* s, const char* cfg_path, const char* ini_path, settings_warn_fn warn) {
	// We want the SDLPoP.cfg file (in-game menu settings) to override the SDLPoP.ini file,
	// but ONLY if the .ini file wasn't modified since the last time the .cfg file was saved!
	struct stat st_ini, st_cfg;
	if (cfg_path == NULL || cfg_path[0] == '\0') return 0;
	if (stat(cfg_path, &st_cfg) != 0) return 0;
	if (ini_path != NULL && ini_path[0] && stat(ini_path, &st_ini) == 0) {
		if (st_ini.st_mtime > st_cfg.st_mtime) {
			// SDLPoP.ini is newer than SDLPoP.cfg, so just go with the .ini configuration
			return 0;
		}
	}
	// If there is a SDLPoP.cfg file, let it override the settings
	return settings_load(s, cfg_path, warn);
}

static void menu_was_closed(void) {
	is_menu_shown = 0;
	escape_key_suppressed = (key_held[SDL_SCANCODE_BACKSPACE] || key_held[SDL_SCANCODE_ESCAPE]);
	if (were_settings_changed) {
		save_ingame_settings();
		were_settings_changed = 0;
	}
	// In fullscreen mode, hide the mouse cursor (because it is only needed in the menu).
	if (host.window != NULL) {
		dword flags = SDL_GetWindowFlags(host.window);
		if (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) {
			SDL_ShowCursor(SDL_DISABLE);
		} else {
			SDL_ShowCursor(SDL_ENABLE);
		}
	}
}

/* ---- SDLPoP2: the frontend's entry points ---- */
void overlay_menu_init(const overlay_menu_host* menu_host) {
	host = *menu_host;
	S = host.settings;
	overlay_color_count = 0;
	for (int i = 0; i < 16; ++i) {
		map_rgba(vga_palette_default[i][0] << 2, vga_palette_default[i][1] << 2, vga_palette_default[i][2] << 2, 255);
	}
	if (overlay_surface == NULL) overlay_surface = port_new(&rect_screen);
	if (bottom_text_surface == NULL) bottom_text_surface = port_new(&rect_screen);
	current_target_surface = overlay_surface;
	static bool_type font_loaded;
	if (!font_loaded) {
		load_font_character_offsets(hc_small_font_data);
		font_loaded = 1;
	}
	gfx_add_font(hc_small_font, hc_small_font_data);
	calc_dialog_peel_rect(copyprot_dialog);
	init_menu();
	next_pause_menu_item = previous_pause_menu_item = &pause_menu_items[0];   // SDLPoP2: (before the first drawing)
	is_menu_shown = 0;
	current_dialog_box = DIALOG_NONE;
}

int overlay_menu_is_open(void) { return is_menu_shown != 0; }

static void track_key(const SDL_Event* e) {
	if (e->type != SDL_KEYDOWN && e->type != SDL_KEYUP) return;
	SDL_Scancode scancode = e->key.keysym.scancode;
	if ((int) scancode >= SDL_NUM_SCANCODES) return;
	key_held[scancode] = e->type == SDL_KEYDOWN;
	// Prevent repeated keystrokes opening/closing the menu as long as the key is held down.
	if (e->type == SDL_KEYUP && (scancode == SDL_SCANCODE_BACKSPACE || scancode == SDL_SCANCODE_ESCAPE)) {
		escape_key_suppressed = 0;
	}
}

void overlay_menu_open(int recording, int replaying) {
	if (S == NULL || is_menu_shown) return;
	replaying_replay = replaying;
	gameplay_settings_editable = !recording && !replaying;
	quicksave_allowed = S->enable_quicksave && !replaying;
	restart_allowed = !replaying;
	cheats_enabled = shell_cheats();   // (the game's: the cheat word, a toggle, a quickload)
	cheats_editable = !replaying;
	// seg000.c (process_key, play_level_2): is_paused = 1, is_menu_shown = 1; display_text_bottom("GAME PAUSED")
	is_menu_shown = 1;
	display_text_bottom("GAME PAUSED");
	// draw_menu:
	escape_key_suppressed = (key_held[SDL_SCANCODE_BACKSPACE] || key_held[SDL_SCANCODE_ESCAPE]);
	need_close_menu = 0;
	key_queue_count = 0;
	pending_mouse_clicked = pending_mouse_clicked_right = pending_scroll_y = 0;
	int stick_x, stick_y;
	joy_menu_button_released = !(controller_count() > 0 && (controller_held(&stick_x, &stick_y) & controller_menu_buttons()));   // (the button that opened it)
	gport* saved = current_target_surface;
	current_target_surface = overlay_surface;
	draw_rect(&screen_rect, map_rgba(0, 0, 0, 0));
	current_target_surface = saved;
}

int overlay_menu_open_event(const SDL_Event* e, int can_open) {
	if (S == NULL) return 0;
	if (e->type == SDL_KEYDOWN || e->type == SDL_KEYUP) {
		track_key(e);
		if (e->type == SDL_KEYUP) return 0;
		SDL_Scancode scancode = e->key.keysym.scancode;
		if (escape_key_suppressed &&
				(scancode == SDL_SCANCODE_BACKSPACE || (S->enable_pause_menu && scancode == SDL_SCANCODE_ESCAPE))
		) {
			return 2; // Prevent repeated keystrokes opening/closing the menu as long as the key is held down.
		}
		if (!can_open || e->key.repeat) return 0;
		if (e->key.keysym.mod & (KMOD_CTRL | KMOD_ALT)) return 0;
		// seg000.c process_key: Esc (also with Shift) pauses, with the menu when enable_pause_menu; Backspace: the menu
		if ((scancode == SDL_SCANCODE_ESCAPE && S->enable_pause_menu) || scancode == SDL_SCANCODE_BACKSPACE) {
			return 1;
		}
		return 0;
	}
	if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT && can_open) {
		// seg009.c process_events: the left mouse button (the menu not shown) is Backspace
		mouse_position_from_event(e->button.x, e->button.y);
		return 1;
	}
	return 0;
}

int overlay_menu_event(const SDL_Event* e) {
	switch (e->type) {
		case SDL_KEYDOWN: {
			track_key(e);
			SDL_Scancode scancode = e->key.keysym.scancode;
			word modifier = e->key.keysym.mod;
			if (escape_key_suppressed &&
					(scancode == SDL_SCANCODE_BACKSPACE || (S->enable_pause_menu && scancode == SDL_SCANCODE_ESCAPE))
			) {
				return 1; // Prevent repeated keystrokes opening/closing the menu as long as the key is held down.
			}
			if (key_queue_count >= KEY_QUEUE_SIZE) return 1;
			int key = 0;
			switch (scancode) {
				// Keys that are ignored by themselves:
				case SDL_SCANCODE_LCTRL:
				case SDL_SCANCODE_LSHIFT:
				case SDL_SCANCODE_LALT:
				case SDL_SCANCODE_LGUI:
				case SDL_SCANCODE_RCTRL:
				case SDL_SCANCODE_RSHIFT:
				case SDL_SCANCODE_RALT:
				case SDL_SCANCODE_RGUI:
				case SDL_SCANCODE_CAPSLOCK:
				case SDL_SCANCODE_SCROLLLOCK:
				case SDL_SCANCODE_NUMLOCKCLEAR:
				case SDL_SCANCODE_APPLICATION:
				case SDL_SCANCODE_PRINTSCREEN:
				case SDL_SCANCODE_VOLUMEUP:
				case SDL_SCANCODE_VOLUMEDOWN:
				// Why are there two mute key codes?
				case SDL_SCANCODE_MUTE:
				case SDL_SCANCODE_AUDIOMUTE:
				case SDL_SCANCODE_PAUSE:
					break;

				default:
					key = scancode;
					if (modifier & KMOD_SHIFT) key |= WITH_SHIFT;
					if (modifier & KMOD_CTRL ) key |= WITH_CTRL ;
					if (modifier & KMOD_ALT  ) key |= WITH_ALT  ;
			}
			key_queue[key_queue_count].key = key;
			key_queue[key_queue_count].any_key = e->key.repeat ? 0 : (int) scancode; // for showmessage_any_key
			++key_queue_count;
			return 1;
		}
		case SDL_KEYUP:
			track_key(e);
			return 1;
		case SDL_MOUSEMOTION:
			mouse_position_from_event(e->motion.x, e->motion.y);
			return 1;
		case SDL_MOUSEBUTTONDOWN:
			mouse_position_from_event(e->button.x, e->button.y);
			switch(e->button.button) {
				case SDL_BUTTON_LEFT:
					pending_mouse_clicked = 1;
					break;
				case SDL_BUTTON_RIGHT:
				case SDL_BUTTON_X1: // 'Back' button (on mice that have these extra buttons).
					pending_mouse_clicked_right = 1;
					break;
				default: break;
			}
			return 1;
		case SDL_MOUSEBUTTONUP:
			return 1;
		case SDL_MOUSEWHEEL:
			pending_scroll_y = -e->wheel.y;
			return 1;
	}
	return 0;
}

int overlay_menu_frame(SDL_Scancode* key, uint16_t* mod) {
	menu_action = OVERLAY_MENU_NONE;
	if (!is_menu_shown) return menu_action;
	quicksave_allowed = S->enable_quicksave && !replaying_replay;
	++frame_counter;
	joystick_read_this_frame = 0;
	// draw_menu's loop: a pass for each key that came (at least one pass a frame)
	int count = key_queue_count;
	for (int i = 0; i == 0 || i < count; ++i) {
		clear_menu_controls();
		// process_events
		last_key_scancode = i < count ? key_queue[i].key : 0;
		last_any_key_scancode = i < count ? key_queue[i].any_key : 0;
		if (i == 0) {
			mouse_clicked = pending_mouse_clicked;
			mouse_button_clicked_right = pending_mouse_clicked_right;
			menu_control_scroll_y = pending_scroll_y;
		}
		draw_menu_pass();
		if (need_close_menu && current_dialog_box == DIALOG_NONE) break;
	}
	key_queue_count = 0;
	pending_mouse_clicked = pending_mouse_clicked_right = pending_scroll_y = 0;
	if (need_close_menu && current_dialog_box == DIALOG_NONE) {
		menu_was_closed();
	}
	if (menu_action == OVERLAY_MENU_KEY) {
		if (key) *key = (SDL_Scancode) menu_action_key;
		if (mod) *mod = menu_action_mod;
	}
	return menu_action;
}

void overlay_menu_state(int* page, const char** item, const char** subsection, const char** setting, int* dialog) {
	*page = drawn_menu;
	*item = *subsection = *setting = "";
	for (int i = 0; i < COUNT(pause_menu_items); ++i) if (pause_menu_items[i].id == hovering_pause_menu_item) *item = pause_menu_items[i].text;
	for (int i = 0; i < COUNT(settings_menu_items); ++i) {
		if (settings_menu_items[i].id == hovering_pause_menu_item) *item = settings_menu_items[i].text;
		if (settings_menu_items[i].id == active_settings_subsection) *subsection = settings_menu_items[i].text;
	}
	for (int i = 0; i < COUNT(cheats_menu_items); ++i) {
		if (cheats_menu_items[i].id == hovering_pause_menu_item) *item = cheats_menu_items[i].text;
		if (cheats_menu_items[i].id == active_settings_subsection) *subsection = cheats_menu_items[i].text;
	}
	if (active_settings_subsection == SETTINGS_MENU_LEVEL_CUSTOMIZATION) *subsection = "LEVEL";
	if (active_settings_subsection == SETTINGS_MENU_SKILL_CUSTOMIZATION) *subsection = "SKILL";
	settings_area_type* area = get_settings_area(active_settings_subsection);
	if (area != NULL && controlled_area == 1) {
		for (int i = 0; i < area->setting_count; ++i) if (area->settings[i].id == highlighted_setting_id) *setting = area->settings[i].text;
	}
	*dialog = current_dialog_box;
}

int overlay_menu_cheats(void) { return cheats_enabled; }
int overlay_menu_cheat_key(void) { return cheat_key_chosen; }

void overlay_menu_close(void) {
	if (is_menu_shown) menu_was_closed();
}

static void blend_port(const gport* port, uint32_t* argb) {
	for (int i = 0; i < SCREEN_W * SCREEN_H; ++i) {
		const rgba_type* c = &overlay_colors[port->bits[i]];
		if (c->a == 0) continue;
		if (c->a == 255) {
			argb[i] = 0xFF000000u | (uint32_t) c->r << 16 | (uint32_t) c->g << 8 | c->b;
			continue;
		}
		uint32_t d = argb[i];
		int a = c->a;
		int r = (c->r * a + (int)((d >> 16) & 0xFF) * (255 - a) + 127) / 255;
		int g = (c->g * a + (int)((d >> 8) & 0xFF) * (255 - a) + 127) / 255;
		int b = (c->b * a + (int)(d & 0xFF) * (255 - a) + 127) / 255;
		argb[i] = 0xFF000000u | (uint32_t) r << 16 | (uint32_t) g << 8 | (uint32_t) b;
	}
}
void overlay_menu_compose(uint32_t* argb) {
	if (!is_menu_shown) return;
	// update_screen / draw_overlay: the screen (with its bottom text), then the overlay surface blended over it
	blend_port(bottom_text_surface, argb);
	blend_port(overlay_surface, argb);
}



// Small font (hardcoded).
// The alphanumeric characters were adapted from the freeware font '04b_03' by Yuji Oshimoto. See: http://www.04.jp.org/

#define BINARY_8(b7,b6,b5,b4,b3,b2,b1,b0) ((b0) | ((b1)<<1) | ((b2)<<2) | ((b3)<<3) | ((b4)<<4) | ((b5)<<5) | ((b6)<<6) | ((b7)<<7))
#define BINARY_4(b7,b6,b5,b4) (((b4)<<4) | ((b5)<<5) | ((b6)<<6) | ((b7)<<7))
#define _ 0
#define WORD(x) (byte)(x), (byte)((x)>>8)
#define IMAGE_DATA(height, width, flags) WORD(height), WORD(width), WORD(flags)

byte hc_small_font_data[] = {

		32, 126, WORD(5), WORD(2), WORD(1), WORD(1),

		// offsets (will be initialized at run-time)
		WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), // 41
		WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), // 51
		WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), // 61
		WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), // 71
		WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), // 81
		WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), // 91
		WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), // 101
		WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), // 111
		WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), // 121
		WORD(0), WORD(0), WORD(0), WORD(0), WORD(0), // 126

		IMAGE_DATA(1, 3, 1), // space
		BINARY_4( _,_,_,_ ),

		IMAGE_DATA(5, 1, 1), // !
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( _,_,_,_ ),
		BINARY_4( 1,_,_,_ ),

		IMAGE_DATA(5, 3, 1), // "
		BINARY_4( 1,_,1,_ ),
		BINARY_4( 1,_,1,_ ),
		BINARY_4( _,_,_,_ ),
		BINARY_4( _,_,_,_ ),
		BINARY_4( _,_,_,_ ),

		IMAGE_DATA(5, 5, 1), // #
		BINARY_8( _,1,_,1,_,_,_,_ ),
		BINARY_8( 1,1,1,1,1,_,_,_ ),
		BINARY_8( _,1,_,1,_,_,_,_ ),
		BINARY_8( 1,1,1,1,1,_,_,_ ),
		BINARY_8( _,1,_,1,_,_,_,_ ),

		IMAGE_DATA(6, 3, 1), // $
		BINARY_4( _,1,_,_ ),
		BINARY_4( 1,1,1,_ ),
		BINARY_4( 1,1,_,_ ),
		BINARY_4( _,1,1,_ ),
		BINARY_4( 1,1,1,_ ),
		BINARY_4( _,1,_,_ ),

		IMAGE_DATA(5, 6, 1), // %
		BINARY_8( _,_,_,_,_,_,_,_ ),
		BINARY_8( 1,1,_,_,1,_,_,_ ),
		BINARY_8( 1,1,_,1,_,_,_,_ ),
		BINARY_8( _,_,1,_,1,1,_,_ ),
		BINARY_8( _,1,_,_,1,1,_,_ ),

		IMAGE_DATA(5, 5, 1), // &
		BINARY_8( _,1,1,_,_,_,_,_ ),
		BINARY_8( _,1,1,_,_,_,_,_ ),
		BINARY_8( 1,1,1,_,1,_,_,_ ),
		BINARY_8( 1,_,_,1,_,_,_,_ ),
		BINARY_8( _,1,1,_,1,_,_,_ ),

		IMAGE_DATA(2, 1, 1), // '
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,_,_,_ ),

		IMAGE_DATA(5, 3, 1), // (
		BINARY_4( _,1,_,_ ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( _,1,_,_ ),

		IMAGE_DATA(5, 3, 1), // )
		BINARY_4( _,1,_,_ ),
		BINARY_4( _,_,1,_ ),
		BINARY_4( _,_,1,_ ),
		BINARY_4( _,_,1,_ ),
		BINARY_4( _,1,_,_ ),

		IMAGE_DATA(4, 5, 1),
		BINARY_8( _,_,_,_,_,_,_,_ ), // *
		BINARY_8( 1,_,1,_,1,_,_,_ ),
		BINARY_8( _,1,1,1,_,_,_,_ ),
		BINARY_8( 1,_,1,_,1,_,_,_ ),

		IMAGE_DATA(4, 3, 1), // +
		BINARY_4( _,_,_,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( 1,1,1,_ ),
		BINARY_4( _,1,_,_ ),

		IMAGE_DATA(6, 2, 1), // ,
		BINARY_4( _,_,_,_ ),
		BINARY_4( _,_,_,_ ),
		BINARY_4( _,_,_,_ ),
		BINARY_4( _,_,_,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( 1,_,_,_ ),

		IMAGE_DATA(3, 3, 1), // -
		BINARY_4( _,_,_,_ ),
		BINARY_4( _,_,_,_ ),
		BINARY_4( 1,1,1,_ ),

		IMAGE_DATA(5, 1, 1), // .
		BINARY_4( _,_,_,_ ),
		BINARY_4( _,_,_,_ ),
		BINARY_4( _,_,_,_ ),
		BINARY_4( _,_,_,_ ),
		BINARY_4( 1,_,_,_ ),

		IMAGE_DATA(5, 4, 1), // /
		BINARY_4( _,_,_,_ ),
		BINARY_4( _,_,_,1 ),
		BINARY_4( _,_,1,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( 1,_,_,_ ),

		IMAGE_DATA(5, 4, 1), // 0
		BINARY_4( _,1,1,_ ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( _,1,1,_ ),

		IMAGE_DATA(5, 2, 1), // 1
		BINARY_4( 1,1,_,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( _,1,_,_ ),

		IMAGE_DATA(5, 4, 1), // 2
		BINARY_4( 1,1,1,_ ),
		BINARY_4( _,_,_,1 ),
		BINARY_4( _,1,1,_ ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,1,1,1 ),

		IMAGE_DATA(5, 4, 1), // 3
		BINARY_4( 1,1,1,_ ),
		BINARY_4( _,_,_,1 ),
		BINARY_4( _,1,1,_ ),
		BINARY_4( _,_,_,1 ),
		BINARY_4( 1,1,1,_ ),

		IMAGE_DATA(5, 4, 1), // 4
		BINARY_4( _,_,1,_ ),
		BINARY_4( _,1,1,_ ),
		BINARY_4( 1,_,1,_ ),
		BINARY_4( 1,1,1,1 ),
		BINARY_4( _,_,1,_ ),

		IMAGE_DATA(5, 4, 1), // 5
		BINARY_4( 1,1,1,1 ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,1,1,_ ),
		BINARY_4( _,_,_,1 ),
		BINARY_4( 1,1,1,_ ),

		IMAGE_DATA(5, 4, 1), // 6
		BINARY_4( _,1,1,_ ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,1,1,_ ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( _,1,1,_ ),

		IMAGE_DATA(5, 4, 1), // 7
		BINARY_4( 1,1,1,1 ),
		BINARY_4( _,_,_,1 ),
		BINARY_4( _,_,1,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( _,1,_,_ ),

		IMAGE_DATA(5, 4, 1), // 8
		BINARY_4( _,1,1,_ ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( _,1,1,_ ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( _,1,1,_ ),

		IMAGE_DATA(5, 4, 1), // 9
		BINARY_4( _,1,1,_ ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( _,1,1,1 ),
		BINARY_4( _,_,_,1 ),
		BINARY_4( _,1,1,_ ),

		IMAGE_DATA(5, 1, 1), // :
		BINARY_4( _,_,_,_ ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( _,_,_,_ ),
		BINARY_4( _,_,_,_ ),
		BINARY_4( 1,_,_,_ ),

		IMAGE_DATA(6, 2, 1), // ;
		BINARY_4( _,_,_,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( _,_,_,_ ),
		BINARY_4( _,_,_,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( 1,_,_,_ ),

		IMAGE_DATA(5, 3, 1), // <
		BINARY_4( _,_,1,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( _,_,1,_ ),

		IMAGE_DATA(4, 3, 1), // =
		BINARY_4( _,_,_,_ ),
		BINARY_4( 1,1,1,_ ),
		BINARY_4( _,_,_,_ ),
		BINARY_4( 1,1,1,_ ),

		IMAGE_DATA(5, 4, 1), // >
		BINARY_4( 1,_,_,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( _,_,1,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( 1,_,_,_ ),

		IMAGE_DATA(5, 4, 1), // ?
		BINARY_4( _,1,1,_ ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( _,_,1,_ ),
		BINARY_4( _,_,_,_ ),
		BINARY_4( _,_,1,_ ),

		IMAGE_DATA(6, 4, 1), // @
		BINARY_4( _,1,1,_ ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,1,1 ),
		BINARY_4( 1,_,1,1 ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( _,1,1,_ ),

		IMAGE_DATA(5, 4, 1), // A
		BINARY_4( _,1,1,_ ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,1,1,1 ),
		BINARY_4( 1,_,_,1 ),

		IMAGE_DATA(5, 4, 1), // B
		BINARY_4( 1,1,1,_ ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,1,1,_ ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,1,1,_ ),

		IMAGE_DATA(5, 4, 1), // C
		BINARY_4( _,1,1,1 ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( _,1,1,1 ),

		IMAGE_DATA(5, 4, 1), // D
		BINARY_4( 1,1,1,_ ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,1,1,_ ),

		IMAGE_DATA(5, 4, 1), // E
		BINARY_4( 1,1,1,1 ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,1,1,1 ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,1,1,1 ),

		IMAGE_DATA(5, 4, 1), // F
		BINARY_4( 1,1,1,1 ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,1,1,1 ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,_,_,_ ),

		IMAGE_DATA(5, 4, 1), // G
		BINARY_4( _,1,1,1 ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,_,1,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( _,1,1,1 ),

		IMAGE_DATA(5, 4, 1), // H
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,1,1,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),

		IMAGE_DATA(5, 3, 1), // I
		BINARY_4( 1,1,1,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( 1,1,1,_ ),

		IMAGE_DATA(5, 4, 1), // J
		BINARY_4( _,_,1,1 ),
		BINARY_4( _,_,_,1 ),
		BINARY_4( _,_,_,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( _,1,1,_ ),

		IMAGE_DATA(5, 4, 1), // K
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,1,_ ),
		BINARY_4( 1,1,_,_ ),
		BINARY_4( 1,_,1,_ ),
		BINARY_4( 1,_,_,1 ),

		IMAGE_DATA(5, 4, 1), // L
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,1,1,1 ),

		IMAGE_DATA(5, 5, 1), // M
		BINARY_8( 1,_,_,_,1,_,_,_ ),
		BINARY_8( 1,1,_,1,1,_,_,_ ),
		BINARY_8( 1,_,1,_,1,_,_,_ ),
		BINARY_8( 1,_,_,_,1,_,_,_ ),
		BINARY_8( 1,_,_,_,1,_,_,_ ),

		IMAGE_DATA(5, 4, 1), // N
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,1,_,1 ),
		BINARY_4( 1,_,1,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),

		IMAGE_DATA(5, 4, 1), // O
		BINARY_4( _,1,1,_ ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( _,1,1,_ ),

		IMAGE_DATA(5, 4, 1), // P
		BINARY_4( 1,1,1,_ ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,1,1,_ ),
		BINARY_4( 1,_,_,_ ),

		IMAGE_DATA(6, 4, 1), // Q
		BINARY_4( _,1,1,_ ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( _,1,1,_ ),
		BINARY_4( _,_,_,1 ),

		IMAGE_DATA(5, 4, 1), // R
		BINARY_4( 1,1,1,_ ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,1,1,_ ),
		BINARY_4( 1,_,_,1 ),

		IMAGE_DATA(5, 4, 1), // S
		BINARY_4( _,1,1,1 ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( _,1,1,_ ),
		BINARY_4( _,_,_,1 ),
		BINARY_4( 1,1,1,_ ),

		IMAGE_DATA(5, 3, 1), // T
		BINARY_4( 1,1,1,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( _,1,_,_ ),

		IMAGE_DATA(5, 4, 1), // U
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( _,1,1,_ ),

		IMAGE_DATA(5, 4, 1), // V
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,1,_ ),
		BINARY_4( 1,_,1,_ ),
		BINARY_4( _,1,_,_ ),

		IMAGE_DATA(5, 5, 1),
		BINARY_8( 1,_,_,_,1,_,_,_ ), // W
		BINARY_8( 1,_,1,_,1,_,_,_ ),
		BINARY_8( 1,_,1,_,1,_,_,_ ),
		BINARY_8( 1,_,1,_,1,_,_,_ ),
		BINARY_8( _,1,_,1,_,_,_,_ ),

		IMAGE_DATA(5, 4, 1), // X
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( _,1,1,_ ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),

		IMAGE_DATA(5, 4, 1), // Y
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( _,1,1,1 ),
		BINARY_4( _,_,_,1 ),
		BINARY_4( _,1,1,_ ),

		IMAGE_DATA(5, 3, 1), // Z
		BINARY_4( 1,1,1,_ ),
		BINARY_4( _,_,1,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,1,1,_ ),

		IMAGE_DATA(5, 2, 1), // [
		BINARY_4( 1,1,_,_ ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,1,_,_ ),

		IMAGE_DATA(5, 4, 1), // '\'
		BINARY_4( _,_,_,_ ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( _,_,1,_ ),
		BINARY_4( _,_,_,1 ),

		IMAGE_DATA(5, 4, 1), // ]
		BINARY_4( 1,1,_,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( 1,1,_,_ ),

		IMAGE_DATA(2, 3, 1), // ^
		BINARY_4( _,1,_,_ ),
		BINARY_4( 1,_,1,_ ),

		IMAGE_DATA(5, 3, 1), // _
		BINARY_4( _,_,_,_ ),
		BINARY_4( _,_,_,_ ),
		BINARY_4( _,_,_,_ ),
		BINARY_4( _,_,_,_ ),
		BINARY_4( 1,1,1,_ ),

		IMAGE_DATA(2, 2, 1), // `
		BINARY_4( 1,_,_,_ ),
		BINARY_4( _,1,_,_ ),

		IMAGE_DATA(5, 4, 1), // a
		BINARY_4( _,_,_,_ ),
		BINARY_4( _,1,1,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( _,1,1,1 ),

		IMAGE_DATA(5, 4, 1), // b
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,1,1,_ ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,1,1,_ ),

		IMAGE_DATA(5, 3, 1), // c
		BINARY_4( _,_,_,_ ),
		BINARY_4( _,1,1,_ ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( _,1,1,_ ),

		IMAGE_DATA(5, 4, 1), // d
		BINARY_4( _,_,_,1 ),
		BINARY_4( _,1,1,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( _,1,1,1 ),

		IMAGE_DATA(5, 4, 1), // e
		BINARY_4( _,_,_,_ ),
		BINARY_4( _,1,1,_ ),
		BINARY_4( 1,_,1,1 ),
		BINARY_4( 1,1,_,_ ),
		BINARY_4( _,1,1,1 ),

		IMAGE_DATA(5, 3, 1), // f
		BINARY_4( _,_,1,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( 1,1,1,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( _,1,_,_ ),

		IMAGE_DATA(7, 4, 1), // g
		BINARY_4( _,_,_,_ ),
		BINARY_4( _,1,1,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( _,1,1,1 ),
		BINARY_4( _,_,_,1 ),
		BINARY_4( _,1,1,_ ),

		IMAGE_DATA(5, 4, 1), // h
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,1,1,_ ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),

		IMAGE_DATA(5, 1, 1), // i
		BINARY_4( 1,_,_,_ ),
		BINARY_4( _,_,_,_ ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,_,_,_ ),

		IMAGE_DATA(7, 2, 1), // j
		BINARY_4( _,1,_,_ ),
		BINARY_4( _,_,_,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( 1,_,_,_ ),

		IMAGE_DATA(5, 4, 1), // k
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,1,_ ),
		BINARY_4( 1,1,1,_ ),
		BINARY_4( 1,_,_,1 ),

		IMAGE_DATA(5, 1, 1), // l
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,_,_,_ ),

		IMAGE_DATA(5, 5, 1), // m
		BINARY_8( _,_,_,_,_,_,_,_ ),
		BINARY_8( 1,1,1,1,_,_,_,_ ),
		BINARY_8( 1,_,1,_,1,_,_,_ ),
		BINARY_8( 1,_,1,_,1,_,_,_ ),
		BINARY_8( 1,_,1,_,1,_,_,_ ),

		IMAGE_DATA(5, 4, 1), // n
		BINARY_4( _,_,_,_ ),
		BINARY_4( 1,1,1,_ ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),

		IMAGE_DATA(5, 4, 1), // o
		BINARY_4( _,_,_,_ ),
		BINARY_4( _,1,1,_ ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( _,1,1,_ ),

		IMAGE_DATA(7, 4, 1), // p
		BINARY_4( _,_,_,_ ),
		BINARY_4( 1,1,1,_ ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,1,1,_ ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,_,_,_ ),

		IMAGE_DATA(7, 4, 1), // q
		BINARY_4( _,_,_,_ ),
		BINARY_4( _,1,1,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( _,1,1,1 ),
		BINARY_4( _,_,_,1 ),
		BINARY_4( _,_,_,1 ),

		IMAGE_DATA(5, 3, 1), // r
		BINARY_4( _,_,_,_ ),
		BINARY_4( 1,_,1,_ ),
		BINARY_4( 1,1,_,_ ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,_,_,_ ),

		IMAGE_DATA(5, 4, 1), // s
		BINARY_4( _,_,_,_ ),
		BINARY_4( _,1,1,1 ),
		BINARY_4( 1,1,_,_ ),
		BINARY_4( _,_,1,1 ),
		BINARY_4( 1,1,1,_ ),

		IMAGE_DATA(5, 3, 1), // t
		BINARY_4( _,1,_,_ ),
		BINARY_4( 1,1,1,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( _,_,1,_ ),

		IMAGE_DATA(5, 4, 1), // u
		BINARY_4( _,_,_,_ ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( _,1,1,1 ),

		IMAGE_DATA(5, 4, 1), // v
		BINARY_4( _,_,_,_ ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,1,_ ),
		BINARY_4( _,1,_,_ ),

		IMAGE_DATA(5, 5, 1), // w
		BINARY_8( _,_,_,_,_,_,_,_ ),
		BINARY_8( 1,_,1,_,1,_,_,_ ),
		BINARY_8( 1,_,1,_,1,_,_,_ ),
		BINARY_8( _,1,_,1,_,_,_,_ ),
		BINARY_8( _,1,_,1,_,_,_,_ ),

		IMAGE_DATA(5, 3, 1), // x
		BINARY_4( _,_,_,_ ),
		BINARY_4( 1,_,1,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( 1,_,1,_ ),

		IMAGE_DATA(7, 4, 1), // y
		BINARY_4( _,_,_,_ ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( 1,_,_,1 ),
		BINARY_4( _,1,1,1 ),
		BINARY_4( _,_,_,1 ),
		BINARY_4( _,1,1,_ ),

		IMAGE_DATA(5, 4, 1), // z
		BINARY_4( _,_,_,_ ),
		BINARY_4( 1,1,1,1 ),
		BINARY_4( _,_,1,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( 1,1,1,1 ),

		IMAGE_DATA(5, 4, 1), // {
		BINARY_4( _,_,1,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( 1,1,_,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( _,_,1,_ ),

		IMAGE_DATA(5, 1, 1), // |
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,_,_,_ ),
		BINARY_4( 1,_,_,_ ),

		IMAGE_DATA(5, 4, 1), // }
		BINARY_4( 1,_,_,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( _,1,1,_ ),
		BINARY_4( _,1,_,_ ),
		BINARY_4( 1,_,_,_ ),

		IMAGE_DATA(5, 4, 1), // ~
		BINARY_4( _,_,_,_ ),
		BINARY_4( _,1,_,1 ),
		BINARY_4( 1,_,1,_ ),
		BINARY_4( _,_,_,_ ),
		BINARY_4( _,_,_,_ ),


};

byte arrowhead_up_image_data[] = {
		IMAGE_DATA(4, 7, 1),
		BINARY_8( _,_,_,1,_,_,_,_ ),
		BINARY_8( _,_,1,1,1,_,_,_ ),
		BINARY_8( _,1,1,1,1,1,_,_ ),
		BINARY_8( 1,1,1,1,1,1,1,_ ),
};

byte arrowhead_down_image_data[] = {
		IMAGE_DATA(4, 7, 1),
		BINARY_8( 1,1,1,1,1,1,1,_ ),
		BINARY_8( _,1,1,1,1,1,_,_ ),
		BINARY_8( _,_,1,1,1,_,_,_ ),
		BINARY_8( _,_,_,1,_,_,_,_ ),
};

byte arrowhead_left_image_data[] = {
		IMAGE_DATA(5, 3, 1),
		BINARY_8( _,_,1,_,_,_,_,_ ),
		BINARY_8( _,1,1,_,_,_,_,_ ),
		BINARY_8( 1,1,1,_,_,_,_,_ ),
		BINARY_8( _,1,1,_,_,_,_,_ ),
		BINARY_8( _,_,1,_,_,_,_,_ ),
};

byte arrowhead_right_image_data[] = {
		IMAGE_DATA(5, 3, 1),
		BINARY_8( 1,_,_,_,_,_,_,_ ),
		BINARY_8( 1,1,_,_,_,_,_,_ ),
		BINARY_8( 1,1,1,_,_,_,_,_ ),
		BINARY_8( 1,1,_,_,_,_,_,_ ),
		BINARY_8( 1,_,_,_,_,_,_,_ ),
};


#undef _
#undef WORD
#undef IMAGE_DATA
#undef BINARY_8
#undef BINARY_4
