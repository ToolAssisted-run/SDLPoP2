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

/* The MIDI device (CONFIG.DAT's MIDI type, the setup's SETUP.CFG [MIDI] Type): 0x21 the Sound Blaster Pro's FM chip
 * (MSB_PRO.DRV + PRESET33.DEF, rendered here); 0x28 a Roland MT-32 / LAPC-1 / CM-32L and 0x29 a General MIDI device,
 * both on an MPU-401 (MMPU401.DRV): the MIDI bytes go to audio_midi_out for the frontend's synthesizer, and PRESETS.DEF
 * (PRESET40.DEF / PRESET41.DEF) is a MIDI piece of sysex (the MT-32's timbres) played at the start. The digitized sounds
 * (DIGI.DRV) and the level/scene sound numbers are the same whatever the MIDI device. */
enum { AUDIO_MIDI_FM = 0x21, AUDIO_MIDI_MT32 = 0x28, AUDIO_MIDI_GM = 0x29 };
/* audio_init with a MIDI type; presets: the PRESETS.DEF to use for types 0x28 / 0x29 (NULL: DIR/PRESETS.DEF when it is a
 * MIDI piece, else DIR/SNDDRVRS/PRESET40.DEF or PRESET41.DEF; none: no upload). Set audio_midi_out first: the init sends
 * bytes (volume, then the PRESETS.DEF piece's first events). */
int audio_init_midi(const char *dir, int caps, int midi_type, const char *presets);
/* every byte MMPU401.DRV writes to the MPU-401's data port (0x330, UART mode), with the audio time: the number of samples
 * audio_render has made since the init (the sample being made when an interrupt sends it; between two renders, the
 * next one). The bytes are complete MIDI messages in order (running status never used; sysex F0 .. F7 whole). */
extern void (*audio_midi_out)(uint8_t byte, uint64_t sample);
/* the PRESETS.DEF piece still plays (194C:2CE2 == 0): the game's start-up waits for it (2D3E:03EE), ~9.4 s for PRESET40 */
int audio_setup_playing(void);
int audio_add_file(const char *path);              /* one more sound DAT searched by id (e.g. NISDIGI.DAT, NISMIDI.DAT) */
int audio_add_scene_files(const char *dir);        /* the story scenes' DATs for the devices (NISIBM / NISDIGI / NISMIDI, or
                                                      NIS3VC.DAT for General MIDI, as 2797:0260 opens them); how many opened */
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
       AUDIO_T_TICK,      /* a MIDI timer interrupt (194C:2F10) */
       AUDIO_T_MPU };     /* a = byte written to the MPU-401: b = 0 the data port (MIDI), 1 the command port (FF reset, 3F UART) */
extern void (*audio_trace)(int what, int a, int b);
extern int audio_manual_clock;   /* tests: 1 = interrupts come only from the calls below, never from audio_render */
void audio_midi_irq(void);       /* 194C:2F10 */
void audio_speaker_irq(void);    /* 194C:377B */
void audio_digi_irq(void);       /* DIGI.DRV 0x526 for the last block of a transfer */
