#pragma once
/* The program around the game's tick: start-up, the title and demo loop, the level loop (story scenes, level loads,
 * restarts, the end of a level, time out, the ending and the hall of fame), the in-game keys and menus, pause, the
 * status-line messages and the recorded demos. docs/SHELL.md has the map to the DOS program (0823:0000 on).
 *
 * The code keeps the original's structure (blocking loops that wait for keys and timers); it runs as a coroutine that
 * shell_step() resumes for one video frame (the VGA's 70.086 Hz, the frames DOSBox shows and the oracle counts).
 * The game's own clock, the 60 Hz timer (DS:24DC countdowns, the tick's DS:24DE), is derived from the frames.
 * After each step the picture is in screen_buf / render_palette (render.h).
 *
 *   shell_init(dir, argc, argv)   the game files' directory and the DOS command line words (the cheat word
 *                                 "yippeeyahoo", "LEVELn", "NOSOUNDS", "NIS", "TREE", ...)
 *   shell_step(&input)            one video frame; SHELL_EXIT once the program has quit
 *
 * Input, per frame: the keys held (the keyboard interrupt's table, by scan code), the BIOS shift flags and the
 * keystrokes typed (DOS INT 21h/06 codes: ASCII, or the scan code << 8 for Alt+letter, arrows, F keys). */
#include <stdint.h>

typedef struct shell_input {
	uint8_t down[0x60];    /* held keys by PC scan code (set 1): 0x48 up, 0x4B left, 0x4D right, 0x50 down, 0x47 Home, 0x49 PgUp,
	                          0x4F End, 0x51 PgDn, 0x1C Enter, 0x39 space, 0x2A/0x36 shifts, 0x1D ctrl, 0x38 alt, ... */
	uint8_t shift_flags;   /* 0040:0017: 1 right shift, 2 left shift, 4 ctrl, 8 alt */
	uint16_t typed[8];     /* keystrokes this frame (the BIOS queue): 0x1B Esc, 0x0D Enter, 0x09 Tab, 0x20 space, letters;
	                          0x4800 up, 0x5000 down, 0x4B00 left, 0x4D00 right, 0x4700 Home, 0x4F00 End, 0x1E00 Alt-A, ... */
	int ntyped;
} shell_input;

enum { SHELL_RUNNING = 0, SHELL_EXIT = 1 };
enum shell_mode { SH_START, SH_TITLE, SH_SCENE, SH_MENU, SH_PLAY, SH_PAUSE, SH_DEMO, SH_QUIT };

int  shell_init(const char *game_dir, int argc, const char **argv);   /* 0 when the game files are missing */
int  shell_step(const shell_input *in);   /* (the frontend clears in->ntyped after each step) */
void shell_input_key(shell_input *in, int scan, int down, int ascii);   /* a key event into the frame's input */
int  shell_pc_scancode(int usb_hid);   /* SDL_Scancode (USB HID usage) -> PC scan code set 1 (0: none) */
int  shell_mode(void);            /* what the program is doing (enum shell_mode) */
int  shell_scene(void);           /* the story scene playing (0AAC:0274's number; -1 the intro 7-4-8; 0 none) */
void shell_nis_room(int level, int room, uint8_t *pixels, int rowbytes);   /* weak: 0AAC:0376 for transitions 2 / 3 */
int  shell_exit_code(void);
const char *shell_exit_message(void);   /* the text the DOS program prints when it quits (NULL: none) */
uint32_t shell_frame_count(void);
void shell_set_seed(uint32_t seed);   /* before shell_init: the random seed the program takes from the clock (DOS time()) */

/* sounds the shell starts outside the tick's sound queue go to sound.c's hooks (sound_start_hook(res - 10000), the SDL
 * frontend's audio_request(res)): 0xFFFE the error beep (and 169B:0B9C's "press a key" blink), 10000 + 0x10C the title
 * music, 10000 + 0xFD the hall of fame's, 9999 "unable to save" */
void shell_sound(int res);        /* 194C:840E(res) / 1611:053C(res - 10000) */
void shell_sound_stop(int res);   /* 194C:83D2(res); 0: everything */
void platform_sound_volume(int v);   /* weak: 194C:3380, 15 (sound on) or 0 (off): the frontend's audio_volume(v) */

/* inside the shell (the coroutine): the original's waits */
void sh_frame(void);              /* one video frame passes (the 60 Hz timer runs on) */
int  sh_key(void);                /* 2768:02CA: the next keystroke, 0 when none (then one frame passes) */
int  sh_poll(void);               /* 2797:009C: the current poll routine (DS:1F32): 0D5E:0390 in the menus, else any key */
int  sh_wait(int ticks, int timer);   /* 2797:0158: a timer runs down, or a key (sh_poll) comes: the key */
int  sh_wait_key(void);           /* 2797:01A8 */
void sh_wait_timer(int timer);    /* 2797:0134 */
extern uint16_t sh_timers[4];     /* DS:24DC.. the 60 Hz countdowns (24DE = the tick's frame_delay) */
void sh_quit(int code, const char *msg);   /* 2797:000A: the program ends */
int  sh_scene(int n);             /* (also for the core's 37F0:007C, the level-8 sword scene 6) 0AAC:0274: a story scene (NIS), a transition or the copy protection; 2 when a key cut it short */
int  sh_fade(int delay, uint16_t mask, const uint8_t *target);   /* 2631:037E: the palette banks in mask to target (NULL: black); a key stops it */
void sh_set_poll(int menu);       /* DS:1F32: 0D5E:0390 (menu) or 0823:10A0 (game) */

/* the demo player (15DB): recorded inputs for the prince and the guards */
int  demo_timing_check(void);     /* 15DB:000C: -1 when the recording ended */
