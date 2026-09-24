#pragma once
/* SDLPoP's in-game overlay menu, transcribed from SDLPoP's src/menu.c (see overlay_menu.c): the pause menu (resume,
 * cheats (with the cheats on), quicksave, quickload, restart level, settings, restart game, quit) and the settings screen (general, gameplay,
 * visuals, mods with the level and guard skill pages, controls) over the game's picture, dimmed; keyboard, mouse and
 * game controllers. The settings are SDLPoP2.ini's (source/settings.h), edited in the frontend's pop2_settings, applied
 * live and saved to SDLPoP2.cfg when the menu closes (SDLPoP's SDLPoP.cfg mechanism).
 *
 * While the menu shows, the frontend does not step the shell (the game, its timers and its sound are frozen). The
 * menu draws into ports of its own (text.h's library, 320 x 200), composed over the game's picture by
 * overlay_menu_compose(): screen_buf and render_palette are never touched. */
#include <stddef.h>
#include <stdint.h>
#include <SDL.h>
#include "../source/shell.h"
#include "../source/settings.h"

/* what the frontend applies again when the menu changed a setting (overlay_menu_host.apply) */
enum { OVERLAY_MENU_APPLY_VIDEO = 1,        /* use_correct_aspect_ratio, use_integer_scaling, scaling_type */
       OVERLAY_MENU_APPLY_FULLSCREEN = 2,   /* start_fullscreen: the window goes to it now (SDLPoP's) */
       OVERLAY_MENU_APPLY_AUDIO = 4,        /* enable_sounds, enable_music, volume */
       OVERLAY_MENU_APPLY_KEYS = 8,         /* key_* */
       OVERLAY_MENU_APPLY_CONTROLLER = 16,  /* enable_controller, controller_rumble, joystick_*, enable_pause_menu */
       OVERLAY_MENU_APPLY_CHEATS = 32 };    /* "Enable cheats" (not a setting: the game's DS:10C2, overlay_menu_cheats) */
/* what overlay_menu_frame asks of the frontend */
enum { OVERLAY_MENU_NONE, OVERLAY_MENU_QUICKSAVE, OVERLAY_MENU_QUICKLOAD, OVERLAY_MENU_RESTART_LEVEL,
       OVERLAY_MENU_RESTART_GAME, OVERLAY_MENU_QUIT, OVERLAY_MENU_KEY, OVERLAY_MENU_CHEAT };

typedef struct overlay_menu_host {
	pop2_settings *settings;       /* the frontend's settings: the menu edits them in place */
	SDL_Window *window;            /* NULL: headless (tests) */
	SDL_Renderer *renderer;        /* (the mouse's coordinates: its logical size) */
	void (*apply)(int what);       /* OVERLAY_MENU_APPLY_* (NULL: nothing) */
	char cfg_path[1024];           /* SDLPoP2.cfg: where the menu saves its settings ("" : nowhere) */
} overlay_menu_host;

void overlay_menu_init(const overlay_menu_host *host);
/* the menu closed: every keyboard and mouse event, before the game gets it. can_open: the game is playing (the menu
 * opens only then). 1: the event opens the menu (Esc with enable_pause_menu, Backspace, the left mouse button, as in
 * SDLPoP): call overlay_menu_open; 2: the event is swallowed (Esc or Backspace still held from closing the menu);
 * 0: not the menu's */
int  overlay_menu_open_event(const SDL_Event *e, int can_open);
/* opens it (after overlay_menu_open_event's 1, or the controller's button_menu). recording / replaying: a replay is
 * being recorded / played back: the settings the replay holds (settings_write_gameplay's) cannot be changed then, and
 * while replaying the quicksave, quickload and restart items are not offered */
void overlay_menu_open(int recording, int replaying);
int  overlay_menu_is_open(void);
/* the menu open: every SDL event (keys, mouse); 1 when it was the menu's (controller events are not: 0) */
int  overlay_menu_event(const SDL_Event *e);
/* once per video frame while it is open: the keys, the mouse and the controllers (controller.h's controller_held) since
 * the last frame, then the menu drawn. -> OVERLAY_MENU_*; OVERLAY_MENU_KEY: *key / *mod, a key with Alt or Ctrl that
 * closed the menu, for the game (SDLPoP passes its Ctrl+ keys on); OVERLAY_MENU_CHEAT: an entry of the CHEATS page was
 * chosen (the menu closed): type overlay_menu_cheat_key() into the game (shell_input_type). The menu may have closed
 * (overlay_menu_is_open) */
int  overlay_menu_frame(SDL_Scancode *key, uint16_t *mod);
/* the cheats: "Enable cheats" (GAMEPLAY) as the menu shows it, from shell_cheats() when it opens; the frontend turns the
 * game's cheats on / off (shell_set_cheats) when OVERLAY_MENU_APPLY_CHEATS comes. Not saved: a runtime state. With the
 * cheats on the pause menu has CHEATS, a page of the cheat keys (each chosen: typed into the game) */
int  overlay_menu_cheats(void);
int  overlay_menu_cheat_key(void);   /* the DOS keystroke code of the CHEATS entry chosen (OVERLAY_MENU_CHEAT) */
/* a DOS keystroke code as the CHEATS page shows it: 'k' "K", 'K' "Shift+K", '+' "+", 0x3D00 "F3", 0x3100 "Alt+N" */
void overlay_menu_key_label(int code, char *out, size_t n);
/* (tests) the page (0 the pause menu, 1 the settings, 2 the cheats), the item under the cursor, the settings page shown ("GENERAL",
 * ..., "LEVEL", "SKILL"; "" none), the setting highlighted there ("" none), the dialog showing (0 none) */
void overlay_menu_state(int *page, const char **item, const char **subsection, const char **setting, int *dialog);
void overlay_menu_close(void);   /* (the program ends with the menu open) closed as by Resume: the settings saved */
/* the menu over the game's picture (ARGB, 320 x 200, what the frontend shows); nothing when it is closed */
void overlay_menu_compose(uint32_t *argb);

/* SDLPoP2.cfg: the menu's settings as SDLPoP2.ini text. Loading follows SDLPoP's SDLPoP.cfg rule: it is read (over
 * the ini's settings) unless the ini is newer. 1: read. */
int  overlay_menu_load_cfg(pop2_settings *s, const char *cfg_path, const char *ini_path, settings_warn_fn warn);
int  overlay_menu_save_cfg(const pop2_settings *s, const char *cfg_path);   /* 0: could not be written */
