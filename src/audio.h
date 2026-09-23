/* Sound playback: the DOS game's sound library (194C) and its drivers (MIDI.DRV = Sound Blaster Pro FM on an OPL2/OPL3,
 * DIGI.DRV = Sound Blaster Pro DSP + DMA, the built-in PC speaker player), rendering PCM for a frontend.
 * See docs/AUDIO.md. Game code talks to it with resource ids (tag "SND", 10000 + sound number for the level sounds, 25000+
 * for the story scenes), like 194C:840E / 83D2 / 8426 / 8396 / 3380.
 *
 * Threading: nothing here locks; a frontend that renders on an audio thread (SDL) calls the other functions between
 * SDL_LockAudioDevice / SDL_UnlockAudioDevice. Requests take effect at the current audio time (the end of the last
 * audio_render). */
#pragma once
#include <stdint.h>

enum { AUDIO_CAP_DIGI = 1, AUDIO_CAP_MIDI = 2 };   /* DS:2085: 1 digitized sounds (DIGISND.DAT), 2 MIDI music on the FM chip
                                                      (MIDISND.DAT); 0 = PC speaker only (IBMSND.DAT). The DOS setup
                                                      (CONFIG.DAT: SB Pro, FM type 0x21) gives 3. */
int audio_init(const char *dir, int caps);         /* loads the level sound DATs of those devices and PRESETS.DEF; 0: failed */
int audio_add_file(const char *path);              /* one more sound DAT searched by id (e.g. NISDIGI.DAT, NISMIDI.DAT) */
void audio_shutdown(void);

int audio_request(uint16_t id);                    /* 194C:840E: start resource id (a playing copy restarts); 1: playing */
int audio_request_res(uint16_t id, const uint8_t *res, uint32_t len);   /* the same with the resource's bytes (copied) */
void audio_stop(uint16_t id);                      /* 194C:83D2: stop it (0: everything) */
void audio_release(uint16_t id);                   /* 194C:8396: a looping sound plays to its end (0: all) */
int audio_playing(uint16_t id);                    /* 194C:8426: still playing (0: any sound) */
void audio_volume(int v);                          /* 194C:3380: 0 (off) .. 15 (full); the game toggles 15 / 0 */
int audio_cue(void);                               /* DS:2087: the last cue mark (MIDI meta 7 / speaker code 2) */
const uint8_t *audio_resource(uint16_t id, uint32_t *len);   /* the prepared resource (packed samples unpacked), NULL if none */

/* mono 16-bit PCM at `rate` Hz; advances the audio time */
void audio_render(int16_t *pcm, int frames, int rate);
extern int audio_gain_fm, audio_gain_digi, audio_gain_speaker;   /* 256 = 1.0; defaults as DOSBox-X mixes a SB Pro 2: FM 384, DAC 256 */
extern int audio_dc_block;                         /* 1 (default): remove the FM output's DC offset (a ~14 Hz high-pass) */

/* tests: the driver's I/O as it happens (what: AUDIO_T_*) and direct interrupt entry points */
enum { AUDIO_T_OPL = 1,   /* a = register, b = value (MIDI.DRV 0x8AF) */
       AUDIO_T_MIDI,      /* a = status (type | channel), b = data1 | data2 << 8 (the sequencer's driver call 194C:3055) */
       AUDIO_T_DSP,       /* a = byte written to the DSP (DIGI.DRV 0x2D2), b = 0 */
       AUDIO_T_SPEAKER,   /* a = 0: b = speaker gate (port 61 bits 0-1); a = 1: b = PIT channel 2 divisor */
       AUDIO_T_DIGI_END,  /* the digitized sound's completion callback (194C:3422) */
       AUDIO_T_MIDI_END,  /* the MIDI piece's end callback (194C:34BA) */
       AUDIO_T_TICK };    /* a MIDI timer interrupt (194C:2F10) */
extern void (*audio_trace)(int what, int a, int b);
extern int audio_manual_clock;   /* tests: 1 = interrupts come only from the calls below, never from audio_render */
void audio_midi_irq(void);       /* 194C:2F10 */
void audio_speaker_irq(void);    /* 194C:377B */
void audio_digi_irq(void);       /* DIGI.DRV 0x526 for the last block of a transfer */
