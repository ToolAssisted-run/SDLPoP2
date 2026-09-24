#pragma once
/* SDLPoP2.ini: the frontend's settings and the gameplay overrides (see the ini itself for every option).
 *
 * The parser and the defaults have no dependency on SDL. Every default reproduces the original game. The core and the
 * shell read the gameplay part only through `pop2_settings_game`, which is NULL unless a frontend installs a settings
 * structure (the headless core and the tests never do): at each original site the code reads
 * GAME_SETTING(field, original value), so with NULL the verified path is the original expression itself. */
#include <stdint.h>
#include <stdio.h>

enum { SCALING_SHARP, SCALING_FUZZY, SCALING_BLURRY };
enum { SOUND_DEVICE_SPEAKER = 0, SOUND_DEVICE_DIGITAL = 1, SOUND_DEVICE_FM = 2, SOUND_DEVICE_FM_DIGITAL = 3 };   /* = audio.h's caps */
/* the remappable controls: the PC scan codes the game reads (input.c's key table, 0823:1178) */
enum { KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN, KEY_UPLEFT, KEY_UPRIGHT, KEY_DOWNLEFT, KEY_DOWNRIGHT, KEY_SHIFT, KEY_CTRL, KEY_COUNT };
extern const char *const settings_key_ini_names[KEY_COUNT];   /* "key_left", ... */
extern const uint8_t settings_key_pc_scan[KEY_COUNT];          /* 0x4B left, 0x4D right, 0x48 up, 0x50 down, 0x47 Home, ... */
/* the controller's remappable buttons ([Controller]; the frontend's sdl/controller.c gives them the keys' meaning) */
enum { BUTTON_UP, BUTTON_DOWN, BUTTON_SHIFT, BUTTON_CTRL, BUTTON_MENU, BUTTON_RESTART, BUTTON_QUICKSAVE, BUTTON_QUICKLOAD,
       BUTTON_TIME, BUTTON_INFO, BUTTON_COUNT };
extern const char *const settings_button_ini_names[BUTTON_COUNT];   /* "button_up", ... */
#define SETTINGS_LEVELS 14
#define SETTINGS_SKILLS 12   /* DS:1BB6's tables have 12 words each */
#define SWORD_NONE 0xFF

typedef struct pop2_settings {
	/* [General] (the frontend) */
	int start_fullscreen;
	int window_width, window_height;          /* 0: auto */
	int use_correct_aspect_ratio;             /* 1: 4:3 (mode 13h on a CRT) */
	int use_integer_scaling;
	int scaling_type;                         /* SCALING_* */
	int enable_music, enable_sounds;
	int volume;                               /* 0..15: what "sound on" (the game's 15) plays at */
	int sound_device;                         /* SOUND_DEVICE_* (audio_init's caps) */
	char keys[KEY_COUNT][32];                 /* SDL scancode names ("Left", "Left Shift", ...) */
	/* [General], read by the shell (recorded in replays) */
	int enable_intro, enable_story_scenes, skip_title;
	/* [AdditionalFeatures] */
	int enable_quicksave;                     /* (frontend) F6 / F9 */
	int enable_quicksave_penalty;             /* (shell) a quickload costs a minute of game time */
	int enable_replay;                        /* (frontend) --record / --replay */
	char replays_folder[256];
	int random_seed_clock; uint32_t random_seed;   /* clock, or a number */
	int enable_info_screen;                   /* (frontend) F1 */
	/* [Controller] (the frontend) */
	int enable_controller;                    /* SDL game controllers drive the game (through the keys) */
	int controller_rumble;                    /* rumble when the prince is hurt */
	int joystick_threshold;                   /* 0..32767: the analog dead zone */
	int joystick_only_horizontal;             /* the stick gives left / right only (the D-pad all eight) */
	char gamecontrollerdb_file[256];          /* extra SDL controller mappings ("" none) */
	char buttons[BUTTON_COUNT][64];           /* SDL game controller button names, space separated ("none": no button) */
	/* [CustomGameplay] (the core and the shell) */
	int start_minutes_left;                   /* 75 (169B:0006) */
	int ticks_per_minute;                     /* 719 = 0x2CF (DS:5CEA) */
	int start_hitp;                           /* 3 (169B:0006) */
	int max_hitp_allowed;                     /* 12 (0823:0F16) */
	int skip_level_reduced_minutes;           /* 15 (Alt-N without the cheat word, 0823:02BE) */
	int first_level;                          /* 1 (0823:01CA) */
	int base_speed, fight_speed;              /* 5, 6 (169B:0BA6: frame_delay, 60 Hz ticks) */
	/* [Level N] */
	uint8_t sword_type[SETTINGS_LEVELS + 1];  /* by level number: SWORD_NONE, 1 or 2 (1286:0D06) */
	/* [Skill N]: the guards' per-skill tables (DS:1BB6.., DS:13D0) */
	uint16_t strikeprob[SETTINGS_SKILLS], restrikeprob[SETTINGS_SKILLS], blockprob[SETTINGS_SKILLS],
	         impblockprob[SETTINGS_SKILLS], advprob[SETTINGS_SKILLS], refractimer[SETTINGS_SKILLS];
} pop2_settings;

/* the gameplay overrides in effect: NULL (the default) = the original game */
extern const pop2_settings *pop2_settings_game;
#define GAME_SETTING(field, original) (pop2_settings_game ? pop2_settings_game->field : (original))

void settings_defaults(pop2_settings *s);
/* warn: called with a message for each unknown section / key / bad value (NULL: stderr) */
typedef void (*settings_warn_fn)(const char *msg);
int settings_load(pop2_settings *s, const char *path, settings_warn_fn warn);   /* 0: the file could not be read */
int settings_parse_text(pop2_settings *s, const char *text, const char *origin, settings_warn_fn warn);   /* -> the number of warnings */
/* the settings that change what the program does (the shell and the core), as ini text: for replay files */
void settings_write_gameplay(const pop2_settings *s, FILE *f);
int settings_gameplay_equal(const pop2_settings *a, const pop2_settings *b);
void settings_copy_gameplay(pop2_settings *dst, const pop2_settings *src);   /* (a replay's settings over the frontend's) */
