#pragma once
/* The story scenes (NIS: intro, between-level scenes, ending), played by the DOS game from overlay OVL00
 * (loaded at 2D3E..33B9). See docs/NIS.md for the formats and the addresses.
 *
 *   nis_open(dir, scene)       scene = the number the game passes to play_scene (0AAC:0274): 1..6 transitions
 *                              (TRANS.DAT), 7..11 story scenes, 20..28 the sultan's-daughter scenes (NIS.DAT 26001..26009)
 *   nis_step(screen, pal)      one video frame (VGA 70 Hz): screen = 320x200 palette indices, pal = 768 bytes of 6-bit
 *                              VGA DAC values, as the monitor shows them during the frame (the pixels as it begins,
 *                              the palette set at its retrace); returns 0 once the scene has ended
 *
 * The game's clock is the 60 Hz timer tick (DS:24E4); the model runs in CPU cycles of the reference machine (the
 * oracle's DOSBox-X, 22000 cycles/ms) with the PIT's interrupts (60 Hz, or 240 Hz while MIDI music plays) and a rough
 * cost for the scene's own work (see nis.c). nis_set_tick_source(fn) instead forces the tick count at each frame.
 * Music (MIDI cue points and ends) and digitized speech are modelled for their timing only; the sound callback reports
 * every start and stop (ids: SND resources of NISMIDI.DAT / NISDIGI.DAT / DIGISND.DAT). */
#include <stdint.h>

enum { NIS_SND_MUSIC = 1, NIS_SND_DIGI = 2, NIS_SND_STOP = 3 };
/* checkpoints (for synchronising with a capture of the game: the probe addresses in brackets) */
enum { NIS_EV_ANIM_FRAME = 1 /* 32D4:0852 */, NIS_EV_FADE /* 2631:037E */, NIS_EV_SOUND /* 194C:840E */,
       NIS_EV_TEXT /* 2D7D:01AF */, NIS_EV_DISSOLVE /* 33B9:0000 */, NIS_EV_PLAY_ANIM /* 32D4:0BE4 */,
       NIS_EV_SCENE /* 0AAC:0274 */, NIS_EV_CUE /* 194C:314B */, NIS_EV_SETPAL /* 194C:79A3 */,
       NIS_EV_DIS_STEP /* 33B9:0300 */ };
typedef void (*nis_event_fn)(int ev, uint32_t frame, void *user);
typedef void (*nis_sound_fn)(int kind, int id, void *user);
typedef uint32_t (*nis_tick_fn)(uint32_t frame, void *user);

/* The game engine's parts the transitions use (the rest of the game, src/render*.c):
 *   nis_room_fn    0AAC:0376 (level, room), called by transitions 2 (level 10, room 22) and 3 (level 14, room 1): load the
 *                  level and draw the room into pixels (the current port: the 320x200 screen, rowbytes 320), as the game
 *                  shows it (rows 0..191); the palette is not the hook's business (the scene sets its own)
 *   nis_kid        the kid as 0AAC:0442 draws him (transition 6): DS:5B37 facing (0: mirrored, 19 pixels more to the
 *                  left), DS:5B38 x and DS:5B3A y (image drawn at x - 0x89, y - 10), and DS:6116 (OVL14 37F0:01AA puts
 *                  the spirit's window (script 4210) at left = x6116 - 0x87, 4 pixels higher every play) */
typedef void (*nis_room_fn)(int level, int room, uint8_t *pixels, int rowbytes, void *user);
typedef struct { int facing, x, y, x6116; } nis_kid;
void nis_set_room_hook(nis_room_fn fn, void *user);
void nis_set_kid(const nis_kid *k);

#define NIS_INTRO (-1)                    /* the intro: scenes 7, 4 and 8 in a row (the music runs on from one to the next) */
int nis_open(const char *dir, int scene);
int nis_step(uint8_t *screen320x200, uint8_t *pal768);
void nis_close(void);
void nis_set_sound_callback(nis_sound_fn fn, void *user);
void nis_set_tick_source(nis_tick_fn fn, void *user);
void nis_set_event_callback(nis_event_fn fn, void *user);
void nis_abort(void);                     /* a key press: the scene fades out and ends (the game's 2797:009C) */
uint32_t nis_frame(void);                 /* frames stepped so far */
uint32_t nis_tick(void);                  /* the game tick of the current frame */
uint32_t nis_anim_frames(void);           /* animation frames shown so far (32D4:0852) */
