/* The sound queue (1611), the ambient sounds (1611:03CC) and a timing model of the sound driver (194C).
 *
 * The game never waits for the driver, but it asks it whether a sound is still playing (194C:8426), and those answers
 * steer the death waits, the level end, a few room effects and the ambient sounds' random draws. The driver plays
 * digital sounds (DIGISND.DAT, 8-bit samples at 11000 Hz) on channel 0 and MIDI pieces (MIDISND.DAT: the ambient
 * sounds and all music) on channel 1; each channel remembers the last resource started on it and whether it is busy,
 * and "playing(id)" is the busy flag of the channel whose current resource is id (194C:33CE; id 0: any channel).
 * The model keeps each channel's sound and the time it ends; the time is a clock advanced by one frame period
 * (DS:24DE's reload, frame_delay ticks of 60 Hz) per pass of the main loop, replaceable by the platform
 * (sound_clock_hook). The two ids sets are disjoint; an id in neither file never plays. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "globals.h"
#include "dat.h"

uint16_t word_0882 = 0xFFFF, word_0884 = 0xFFFF;   /* DS:0882 / 0884: the effect / music started last */
uint8_t amb_state[2];                             /* DS:2B98 ambient on (DS:2085 & 2), 2B99 its variant group (0xFF: music) */
#define amb_cur (*(uint16_t *)tiles0)             /* DS:2B9A: the ambient sound (or music) started last */
uint8_t sound_caps = 3;                           /* DS:2085: 1 digital, 2 MIDI (3 at runtime) */
int sound_debug;                                  /* tests: 1 log starts, stops and waits, 2 log the queue (stderr) */
int sound_ambient_enabled = 1;                    /* tests: 0 leaves the ambient sounds (and their random draws) out */
int (*sound_query_hook)(int what, uint16_t res, int model);   /* tests: replace an answer (what: 0 playing, 1/2 death waits, 3/4 level end music / effect) */
uint32_t (*sound_clock_hook)(void);               /* platform: the time in microseconds */

/* driver model state (kept in savestates) */
typedef struct snd_channel { int16_t id; uint32_t end; } snd_channel;
snd_channel snd_ch[2] = {{-1, 0}, {-1, 0}};
uint32_t snd_time;                                /* the model's clock (us) */

/* resources: kind (1 digital, 2 MIDI) and length in microseconds by sound number */
#define NSND 0x200
static uint8_t snd_kind[NSND]; static uint32_t snd_len[NSND]; static int snd_loaded;
uint32_t snd_start_delay = 5000;   /* the pass's end (1611:04D0 after the tick and the drawing): this long after its start */

static uint32_t vlq(const uint8_t *b, int *i, int n) { uint32_t v = 0; while (*i < n) { uint8_t c = b[(*i)++]; v = v << 7 | (c & 0x7F); if (!(c & 0x80)) break; } return v; }
/* a MIDISND piece: byte 2 then a format-0 standard MIDI file, played by a 240 Hz interrupt that adds a 16.16 tick
 * increment (194C:2FB4: trunc(trunc(256e6 / 240) * 65536 / trunc(tempo * 256 / 480)), each division truncating) until
 * the end-of-track event's tick. The driver reads a tempo's three bytes as the high byte then a little-endian word
 * (194C:3171), so 0x0C3500 (75 bpm) plays as 0x0C0035. Returns microseconds. */
static uint32_t midi_inc(uint32_t tempo) { uint32_t q = (uint32_t)((uint64_t)tempo * 256 / 480); return q ? (uint32_t)((uint64_t)(256000000u / 240) * 65536 / q) : 0; }
static uint32_t midi_length(const uint8_t *b, int n)
{
	if (n < 23 || memcmp(b + 1, "MThd", 4) || memcmp(b + 15, "MTrk", 4)) return 0;
	int j = 23, end = n; uint8_t run = 0; uint64_t pos = 0, at = 0, irq = 0; uint32_t inc = midi_inc(500000);
	while (j < end) {
		at += (uint64_t)vlq(b, &j, end) << 16;   /* the event's tick (16.16); the counter reaches it on an interrupt */
		if (at > pos && inc) { uint64_t k = (at - pos + inc - 1) / inc; irq += k; pos += k * inc; }
		if (j >= end) break;
		uint8_t c = b[j];
		if (c == 0xFF) { if (j + 1 >= end || b[j + 1] == 0x2F) break; uint8_t ty = b[j + 1]; j += 2; uint32_t l = vlq(b, &j, end);
			if (ty == 0x51 && l == 3 && j + 3 <= end) inc = midi_inc((uint32_t)(b[j] << 16 | b[j + 2] << 8 | b[j + 1]));
			j += l; }
		else if (c == 0xF0 || c == 0xF7) { j++; j += vlq(b, &j, end); }
		else { if (c & 0x80) { run = c; j++; } j += ((run & 0xF0) == 0xC0 || (run & 0xF0) == 0xD0) ? 1 : 2; }
	}
	return (uint32_t)(irq * 1000000 / 240);
}
static void snd_load(void)
{
	if (snd_loaded) return;
	snd_loaded = 1;
	extern char glue_dir[400]; const char *dir = glue_dir[0] ? glue_dir : getenv("PRINCE2_DIR"); if (!dir) return;
	char p[512]; dat_file d; uint16_t n;
	snprintf(p, sizeof p, "%s/DIGISND.DAT", dir);
	if (dat_open(&d, p)) for (int i = 0; i < NSND; i++) { const uint8_t *r = dat_find(&d, "DNS", 10000 + i, &n); if (r && n >= 6) { unsigned rate = r[1] | r[2] << 8, len = r[4] | r[5] << 8; snd_kind[i] = 1; snd_len[i] = (uint32_t)((double)len * 1e6 / (rate ? rate : 11000)); } }
	/* packed samples (byte 3 0xFF, no length in the header; 0x20 0x26 0x2F 0x31 0x36 0x258): measured in the oracle */
	snd_len[0x31] = 1391000;    /* 97-98 frames (level 1) */
	snd_len[0x36] = 60000000;   /* level 13's moving wall: still playing after 286 frames when replaced (a loop, it seems) */
	snprintf(p, sizeof p, "%s/MIDISND.DAT", dir);
	if (dat_open(&d, p)) for (int i = 0; i < NSND; i++) { const uint8_t *r = dat_find(&d, "DNS", 10000 + i, &n); if (r && !snd_kind[i] && r[0] == 2) { snd_kind[i] = 2; snd_len[i] = midi_length(r, n); } }
}
uint32_t snd_midi_extra = 0, snd_digi_extra = 0;   /* driver latencies */
int sound_phase; int32_t amb_margin;   /* diagnostics: the MIDI channel's time left at the last ambient check */   /* 1 during the pass's end (1611:04D0 / 03CC), after the tick and the drawing */
static uint32_t now(void) { return sound_clock_hook ? sound_clock_hook() : snd_time + (sound_phase ? snd_start_delay : 0); }
static int snd_playing_any(int k) { return (int32_t)(snd_ch[k].end - now()) > 0; }
int snd_ch_id(int k) { return snd_playing_any(k) ? snd_ch[k].id : -1; }
int snd_ch_left(int k) { sound_phase = 1; int r = (int32_t)(snd_ch[k].end - now()); sound_phase = 0; return r; }
/* 194C:8426 with 10000 + n (n -1: no resource) */
static int snd_playing(int n)
{
	snd_load();
	if (n < 0 || n >= NSND || !snd_kind[n]) return 0;
	const snd_channel *c = &snd_ch[snd_kind[n] - 1];
	return c->id == n && (int32_t)(c->end - now()) > 0;
}
/* 194C:8092: the channel of the resource's kind plays it from the start */
static void snd_start(int n)
{
	snd_load();
	if (n < 0 || n >= NSND || !snd_kind[n]) return;
	snd_channel *c = &snd_ch[snd_kind[n] - 1];
	c->id = n; c->end = now() + snd_len[n] + (snd_kind[n] == 2 ? snd_midi_extra : snd_digi_extra);
	if (sound_debug & 1) fprintf(stderr, "snd start %X ch%d at %u until %u (%.1f frames)\n", n, snd_kind[n] - 1, now(), c->end, snd_len[n] / (1e6 / 70.086));
}
/* 194C:83D2 with 10000 + n; 0 stops everything */
static void snd_stop(int n)
{
	for (int k = 0; k < 2; k++) if (n == -10000 || snd_ch[k].id == n) { if ((int32_t)(snd_ch[k].end - now()) > 0) snd_ch[k].end = now(); }
}
void sound_stop_all(void) { snd_stop(-10000); }
void sound_194c_83d2(uint16_t res) { if (sound_debug & 1) fprintf(stderr, "snd stop %X at %u\n", res, now()); snd_stop(res == 0 ? -10000 : (int)res - 10000); }
/* one pass of the main loop done: frame_delay ticks of the 60 Hz frame timer (DS:24DE), one more when it ran late */
int sound_pass_late;
void sound_pass_done(void) { snd_time += (uint32_t)((frame_delay ? frame_delay : 5) + sound_pass_late) * 16667u; sound_pass_late = 0; }
static int ask(int what, uint16_t res, int model) { return sound_query_hook ? sound_query_hook(what, res, model) : model; }
int sound_playing(uint16_t res) { return ask(0, res, res == 0 ? (snd_ch[0].id >= 0 && snd_playing(snd_ch[0].id)) || (snd_ch[1].id >= 0 && snd_playing(snd_ch[1].id)) : snd_playing((int)res - 10000)); }
int sound_on(void) { return amb_state[0] != 0; }
int sound_digital(void) { return sound_caps & 1; }
int music_playing(void) { return ask(0, (uint16_t)(word_0884 + 10000), snd_playing((int16_t)word_0884)); }

/* 1611:01C6: queue an effect unless a higher-priority one (DS:0D5D, lower is higher) is queued, feather fall
 * (DS:5D36) runs, or the shadow (charid 1) is the one making it */
void play_sound(uint16_t n)
{
	if (word_087e != -1 && ds_byte(0x0D5D + 3 * n) > ds_byte(0x0D5D + 3 * (uint16_t)word_087e)) return;
	if (word_5d36 != 0 || Char.charid == 1) return;
	if (sound_debug & 2) fprintf(stderr, "Q %X\n", n);
	word_087e = (int16_t)n;
}
/* 1611:01A8: queue a music piece (the first one queued in a pass wins) */
void sound_1611_01a8(uint16_t n) { if (word_0880 == 0xFFFF) { if (sound_debug & 2) fprintf(stderr, "M %X\n", n); word_0880 = n; } }
/* 1611:0002: music by index (DS:0886) */
void seq_music_1611(uint8_t m) { sound_1611_01a8(ds_word(0x0886 + 2 * (int8_t)m)); }
/* 1611:0030: the prince's fall scream (the caller passes the room; level 1's sea rooms keep quiet) */
void fall_scream_room(uint8_t room) { if (Char.charid == 0 || level_kind == 5) { if (level_kind == 5 && (room == 0x13 || room == 0x10 || room == 0xF)) return; play_sound(1); } }
/* 1611:0284: the level-end music of the level's kind */
static int kind_music(void) { return level_kind == 1 ? -1 : level_kind == 2 ? 0x1E : level_kind == 4 ? 0x1D : 0x1C; }
/* 1611:02CE */
int level_end_sound_playing(void) { int m = kind_music(); return ask(3, (uint16_t)(m + 10000), m == -1 ? 0 : snd_playing(m)); }
/* 0AFF:1155 / 169B:0541: the effect playing holds the death wait and the level end, except sounds 4, 7 and 0x36 */
static int effect_holds(void) { return snd_playing((int16_t)word_0882) && word_0882 != 4 && word_0882 != 7 && word_0882 != 0x36; }
int death_sound_playing(int both) { if (sound_debug & 1) fprintf(stderr, "snd death wait(%d) at %u: 0882 %X playing %d, 0884 %X playing %d, ch0 %d until %u\n", both, now(), word_0882, snd_playing((int16_t)word_0882), word_0884, snd_playing((int16_t)word_0884), snd_ch[0].id, snd_ch[0].end); return ask(1 + both, 0, both ? snd_playing((int16_t)word_0882) || snd_playing((int16_t)word_0884) : effect_holds()); }
int level_end_effect_playing(void) { return ask(4, 0, effect_holds()); }

/* 1611:0582: the queued effect replaces the one playing: always when that one ended; else by its DS:0D5C flag
 * (0 never, 1 by a different sound, 2 by any) when the new one's priority is at least as high */
static int should_start(uint16_t cur, uint16_t nw)
{
	if (nw == 0xFFFF) return 0;
	if (!(sound_caps & 3) && word_0884 != 0xFFFF && snd_playing((int16_t)word_0884)) return 0;
	if (!snd_playing((int16_t)cur)) return 1;
	uint8_t f = ds_byte(0x0D5C + 3 * cur);
	if (!((f == 1 && cur != nw) || f == 2)) return 0;
	return ds_byte(0x0D5D + 3 * cur) >= ds_byte(0x0D5D + 3 * nw);
}
/* 1611:0826: music on the MIDI channel takes the ambient sound's place (variant 0xFF) */
static void music_ambient(uint16_t n) { if (amb_state[0] && n < NSND && snd_kind[n] == 2) { amb_cur = n; amb_state[1] = 0xFF; } }

/* 1611:0696: no ambient sound: the prince dead, the level-end music, a type-2 level's guard drawing its sword,
 * level 9 room 16, level 8 room 9 from column 8 */
static int ambient_silent(void)
{
	if ((int8_t)Kid.alive > 0 || level_end_sound_playing()) return 1;
	if (level.type == 2 && room_draws_sword_pub(chars[0].room) && (chars[0].f19 == 0x58 || chars[0].f19 == 0x66 || chars[0].f19 == 0xD8)) return 1;
	if (Kid.room == 0x10 && level_number == 9) return 1;
	return Kid.room == 9 && level_number == 8 && Kid.curr_col >= 8;
}
/* 1611:0700 (Char = the prince): time for a new ambient sound */
static int ambient_should_change(void)
{
	amb_margin = (int32_t)(snd_ch[1].end - now());
	if (!amb_state[0] || (int8_t)Char.alive >= 0 || ambient_silent()) return 0;
	if (!snd_playing(amb_cur)) return 1;
	if (amb_state[1] == 0xFF && (level_number != 8 || !snd_playing(0xFF))) return 0;
	int si = char_scan_31bc4(); uint8_t t = ds_byte(0x093A + level_kind);
	if (si && t == 0xFF) si = 0;
	if (si) return (int8_t)t > (int8_t)amb_state[1];
	if ((int8_t)t > (int8_t)amb_state[1] || t == 0xFF) return 0;
	if (find_spawn_pub(Char.curr_row, Char.room)) return 0;
	return !(level_number == 8 && snd_playing(0xFF));
}
/* 1611:0606: pieces for special places and opponents */
static int ambient_special(int di)
{
	if (di && level_number == 5 && (drawn_room == 10 || drawn_room == 7 || drawn_room == 12)) return 0x40;
	if (di && chars[0].charid == 0xC) return 0xC6;
	if (di && chars[0].charid == 0xA) return 0xC7;
	if (Kid.charid == 1 && (int8_t)Kid.f12 > 2) return 0x3C;
	if (level_number == 13 && (drawn_room == 4 || drawn_room == 0x1D)) return 0x3D;
	return -1;
}
/* 1611:03CC(DS:2B98): a new ambient piece, a random variant of the group of the prince's tile (level + 0x2B1B +
 * room*30 + tilepos) or of the level kind's fight group (DS:093A) while a live opponent is in his room */
static void ambient_update(void)
{
	if (Kid.room == 0) return;
	loadkid();
	if (!ambient_should_change() || ds_word(0x091E + 2 * level_kind) == 0) return;
	int di = char_scan_31bc4();
	if (di && ds_byte(0x093A + level_kind) == 0xFF) di = 0;
	int si = ambient_special(di); int8_t group = -1;
	if (si == -1) {
		group = di ? (int8_t)ds_byte(0x093A + level_kind) : (int8_t)((uint8_t *)&level)[0x2B1B + 30 * Char.room + (int8_t)(row_tilepos(Char.curr_row) + Char.curr_col)];
		uint16_t base = ds_word(ds_word(0x091E + 2 * level_kind) + 2 * group), cnt = ds_word(ds_word(0x092C + 2 * level_kind) + 2 * group);
		si = base + random_2751(cnt);
		if (amb_cur == si) si = (base + cnt == si) ? base : si + 1;
	}
	snd_start(si);
	amb_state[1] = (uint8_t)group; amb_cur = (uint16_t)si;
}
/* 169B:0AF3 / 0AFC: 1611:04D0 starts the queued effect and music, then the ambient sounds */
void ambient_sound(void)
{
	snd_load(); sound_phase = 1;
	if ((sound_debug & 1) && word_087e != -1) fprintf(stderr, "snd queued %X at %u (0882 %X playing %d)\n", word_087e, now(), word_0882, snd_playing((int16_t)word_0882));
	if (should_start(word_0882, (uint16_t)word_087e)) { snd_stop((int16_t)word_0882); word_0882 = (uint16_t)word_087e; snd_start((int16_t)word_0882); }
	word_087e = -1;
	if (word_0880 != 0xFFFF) { snd_stop((int16_t)word_0884); word_0884 = word_0880; snd_start((int16_t)word_0884); music_ambient(word_0880); word_0880 = 0xFFFF; }
	if (sound_ambient_enabled) ambient_update();
	sound_phase = 0;
}
/* 1611:02AC(DS:2B98) at the program's start */
void sound_init_ambient(void) { memset(amb_state, 0, 2); memset(tiles0, 0, 6); amb_state[0] = sound_caps & 2; }
int snd_len_of(int n) { snd_load(); return n >= 0 && n < NSND ? (int)snd_len[n] : -1; }   /* tests */
