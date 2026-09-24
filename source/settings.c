/* SDLPoP2.ini: defaults and the parser (settings.h). No SDL here: the frontend resolves the key names. */
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "settings.h"

const pop2_settings *pop2_settings_game;

const char *const settings_key_ini_names[KEY_COUNT] = {
	"key_left", "key_right", "key_up", "key_down", "key_upleft", "key_upright", "key_downleft", "key_downright", "key_shift", "key_ctrl",
};
const uint8_t settings_key_pc_scan[KEY_COUNT] = { 0x4B, 0x4D, 0x48, 0x50, 0x47, 0x49, 0x4F, 0x51, 0x2A, 0x1D };
static const char *const key_defaults[KEY_COUNT] = { "Left", "Right", "Up", "Down", "Home", "PageUp", "End", "PageDown", "Left Shift", "Left Ctrl" };
const char *const settings_button_ini_names[BUTTON_COUNT] = {
	"button_up", "button_down", "button_shift", "button_ctrl", "button_menu", "button_restart", "button_quicksave", "button_quickload",
	"button_time", "button_info",
};
/* SDLPoP's controller layout (Y up, A down, X and the triggers Shift) with PoP2's Ctrl on B */
static const char *const button_defaults[BUTTON_COUNT] = {
	"y", "a", "x lefttrigger righttrigger", "b", "start", "back", "leftshoulder", "rightshoulder", "rightstick", "none",
};

/* the game's tables (PRINCE.EXE's data segment: DS:1BB6 strike, restrike, block, impblock, advance; DS:13D0 refract) */
static const uint16_t def_strike[SETTINGS_SKILLS]   = { 75, 100, 75, 75, 75, 50, 100, 220, 0, 60, 40, 60 };
static const uint16_t def_restrike[SETTINGS_SKILLS] = { 0, 0, 0, 5, 5, 175, 20, 10, 0, 255, 255, 150 };
static const uint16_t def_block[SETTINGS_SKILLS]    = { 0, 150, 150, 200, 200, 255, 200, 250, 0, 255, 255, 255 };
static const uint16_t def_impblock[SETTINGS_SKILLS] = { 0, 75, 75, 100, 100, 145, 100, 250, 0, 145, 255, 175 };
static const uint16_t def_advance[SETTINGS_SKILLS]  = { 255, 200, 200, 200, 255, 255, 200, 0, 0, 255, 100, 100 };
static const uint16_t def_refract[SETTINGS_SKILLS]  = { 20, 20, 20, 20, 10, 10, 10, 10, 0, 10, 0, 0 };

void settings_defaults(pop2_settings *s)
{
	memset(s, 0, sizeof *s);   /* (whole: the structs compare with memcmp) */
	s->use_correct_aspect_ratio = 1; s->scaling_type = SCALING_SHARP;
	s->enable_music = s->enable_sounds = 1; s->volume = 15; s->sound_device = SOUND_DEVICE_FM_DIGITAL;
	for (int k = 0; k < KEY_COUNT; k++) snprintf(s->keys[k], sizeof s->keys[k], "%s", key_defaults[k]);
	s->enable_pause_menu = 1;   /* (SDLPoP's default) */
	s->enable_intro = s->enable_story_scenes = 1; s->skip_title = 0;
	s->enable_quicksave = s->enable_quicksave_penalty = s->enable_replay = 1;
	snprintf(s->replays_folder, sizeof s->replays_folder, "replays");
	s->random_seed_clock = 1; s->random_seed = 0;
	s->enable_info_screen = 1;
	s->enable_controller = s->controller_rumble = 1; s->joystick_threshold = 8000; s->joystick_only_horizontal = 0;
	for (int b = 0; b < BUTTON_COUNT; b++) snprintf(s->buttons[b], sizeof s->buttons[b], "%s", button_defaults[b]);
	s->start_minutes_left = 75; s->ticks_per_minute = 0x2CF; s->start_hitp = 3; s->max_hitp_allowed = 12;
	s->skip_level_reduced_minutes = 15; s->first_level = 1;
	s->base_speed = 5; s->fight_speed = 6;
	for (int l = 0; l <= SETTINGS_LEVELS; l++) s->sword_type[l] = l == 6 ? SWORD_NONE : (l == 7 || l == 8) ? 2 : 1;
	memcpy(s->strikeprob, def_strike, sizeof def_strike); memcpy(s->restrikeprob, def_restrike, sizeof def_restrike);
	memcpy(s->blockprob, def_block, sizeof def_block); memcpy(s->impblockprob, def_impblock, sizeof def_impblock);
	memcpy(s->advprob, def_advance, sizeof def_advance); memcpy(s->refractimer, def_refract, sizeof def_refract);
}

/* ---- the options ---- */
typedef enum { T_BOOL, T_INT, T_SIZE, T_STR, T_SCALING, T_SOUNDDEV, T_SEED, T_KEY } ftype;
typedef struct { const char *section, *key; ftype type; size_t off, size; int min, max; int gameplay; } field;
#define F(sec, name, t, lo, hi, g) { sec, #name, t, offsetof(pop2_settings, name), sizeof ((pop2_settings *)0)->name, lo, hi, g }
static const field fields[] = {
	F("General", start_fullscreen, T_BOOL, 0, 1, 0),
	F("General", window_width, T_SIZE, 0, 16384, 0),
	F("General", window_height, T_SIZE, 0, 16384, 0),
	F("General", use_correct_aspect_ratio, T_BOOL, 0, 1, 0),
	F("General", use_integer_scaling, T_BOOL, 0, 1, 0),
	F("General", scaling_type, T_SCALING, 0, 2, 0),
	F("General", enable_music, T_BOOL, 0, 1, 0),
	F("General", enable_sounds, T_BOOL, 0, 1, 0),
	F("General", volume, T_INT, 0, 15, 0),
	F("General", sound_device, T_SOUNDDEV, 0, 3, 0),
	F("General", enable_pause_menu, T_BOOL, 0, 1, 0),
	F("General", enable_intro, T_BOOL, 0, 1, 1),
	F("General", enable_story_scenes, T_BOOL, 0, 1, 1),
	F("General", skip_title, T_BOOL, 0, 1, 1),
	F("AdditionalFeatures", enable_quicksave, T_BOOL, 0, 1, 0),
	F("AdditionalFeatures", enable_quicksave_penalty, T_BOOL, 0, 1, 1),
	F("AdditionalFeatures", enable_replay, T_BOOL, 0, 1, 0),
	F("AdditionalFeatures", replays_folder, T_STR, 0, 0, 0),
	F("AdditionalFeatures", random_seed, T_SEED, 0, 0, 0),
	F("AdditionalFeatures", enable_info_screen, T_BOOL, 0, 1, 0),
	F("Controller", enable_controller, T_BOOL, 0, 1, 0),
	F("Controller", controller_rumble, T_BOOL, 0, 1, 0),
	F("Controller", joystick_threshold, T_INT, 0, 32767, 0),
	F("Controller", joystick_only_horizontal, T_BOOL, 0, 1, 0),
	F("Controller", gamecontrollerdb_file, T_STR, 0, 0, 0),
	F("CustomGameplay", start_minutes_left, T_INT, 1, 32767, 1),
	F("CustomGameplay", ticks_per_minute, T_INT, 1, 65535, 1),
	F("CustomGameplay", start_hitp, T_INT, 1, 127, 1),
	F("CustomGameplay", max_hitp_allowed, T_INT, 1, 127, 1),
	F("CustomGameplay", skip_level_reduced_minutes, T_INT, 1, 32767, 1),
	F("CustomGameplay", first_level, T_INT, 1, 14, 1),
	F("CustomGameplay", base_speed, T_INT, 1, 255, 1),
	F("CustomGameplay", fight_speed, T_INT, 1, 255, 1),
};
#define NFIELDS (int)(sizeof fields / sizeof fields[0])
static const char *const skill_keys[6] = { "strikeprob", "restrikeprob", "blockprob", "impblockprob", "advprob", "refractimer" };
static uint16_t *skill_table(pop2_settings *s, int k)
{
	uint16_t *t[6] = { s->strikeprob, s->restrikeprob, s->blockprob, s->impblockprob, s->advprob, s->refractimer };
	return t[k];
}

static void warn_default(const char *m) { fprintf(stderr, "%s\n", m); }
static int ieq(const char *a, const char *b) { while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) a++, b++; return !*a && !*b; }
static int parse_int(const char *v, long *out)
{
	char *end; long n = strtol(v, &end, 0);
	if (end == v || *end) return 0;
	*out = n; return 1;
}
static int parse_bool(const char *v, int *out)
{
	if (ieq(v, "true") || ieq(v, "yes") || ieq(v, "on") || !strcmp(v, "1")) { *out = 1; return 1; }
	if (ieq(v, "false") || ieq(v, "no") || ieq(v, "off") || !strcmp(v, "0")) { *out = 0; return 1; }
	return 0;
}
/* one field from its text; 0 on a bad value */
static int set_field(pop2_settings *s, const pop2_settings *d, const field *f, const char *v)
{
	char *p = (char *)s + f->off; const char *dp = (const char *)d + f->off;
	if (ieq(v, "default")) {
		memcpy(p, dp, f->size);
		if (f->type == T_SEED) { s->random_seed_clock = d->random_seed_clock; s->random_seed = d->random_seed; }
		return 1;
	}
	long n; int b;
	switch (f->type) {
	case T_BOOL: if (!parse_bool(v, &b)) return 0; *(int *)p = b; return 1;
	case T_SIZE: if (ieq(v, "auto")) { *(int *)p = 0; return 1; } /* fall through */
	case T_INT: if (!parse_int(v, &n) || n < f->min || n > f->max) return 0; *(int *)p = (int)n; return 1;
	case T_STR: snprintf(p, f->size, "%s", v); return 1;
	case T_SCALING:
		if (ieq(v, "sharp")) b = SCALING_SHARP; else if (ieq(v, "fuzzy")) b = SCALING_FUZZY; else if (ieq(v, "blurry")) b = SCALING_BLURRY; else return 0;
		*(int *)p = b; return 1;
	case T_SOUNDDEV:
		if (ieq(v, "fm_digital")) b = SOUND_DEVICE_FM_DIGITAL; else if (ieq(v, "fm")) b = SOUND_DEVICE_FM;
		else if (ieq(v, "digital")) b = SOUND_DEVICE_DIGITAL; else if (ieq(v, "speaker")) b = SOUND_DEVICE_SPEAKER; else return 0;
		*(int *)p = b; return 1;
	case T_SEED:
		if (ieq(v, "clock")) { s->random_seed_clock = 1; s->random_seed = 0; return 1; }
		{ char *end; unsigned long long u = strtoull(v, &end, 0); if (end == v || *end || u > 0xFFFFFFFFull || v[0] == '-') return 0;
		  s->random_seed_clock = 0; s->random_seed = (uint32_t)u; return 1; }
	case T_KEY: break;
	}
	return 0;
}
static char *trim(char *s)
{
	while (isspace((unsigned char)*s)) s++;
	char *e = s + strlen(s); while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
	return s;
}

int settings_parse_text(pop2_settings *s, const char *text, const char *origin, settings_warn_fn warn)
{
	if (!warn) warn = warn_default;
	pop2_settings d; settings_defaults(&d);
	char section[64] = ""; int sec_n = -1, nwarn = 0, lineno = 0;
	char msg[512];
	#define WARN(...) do { snprintf(msg, sizeof msg, __VA_ARGS__); warn(msg); nwarn++; } while (0)
	while (*text) {
		char line[1024]; size_t n = strcspn(text, "\n");
		size_t k = n < sizeof line - 1 ? n : sizeof line - 1;
		memcpy(line, text, k); line[k] = 0; text += n; if (*text) text++;
		lineno++;
		char *c = strchr(line, ';'); if (c) *c = 0;   /* comments, also after a value */
		char *l = trim(line);
		if (!*l) continue;
		if (*l == '[') {
			char *e = strchr(l, ']');
			if (!e) { WARN("%s:%d: bad section line", origin, lineno); section[0] = 0; continue; }
			*e = 0; snprintf(section, sizeof section, "%s", trim(l + 1)); sec_n = -1;
			int num;
			if (sscanf(section, "Level %d", &num) == 1) {
				if (num < 1 || num > SETTINGS_LEVELS) { WARN("%s:%d: [%s]: levels are 1..%d", origin, lineno, section, SETTINGS_LEVELS); section[0] = 0; continue; }
				sec_n = num; snprintf(section, sizeof section, "Level");
			} else if (sscanf(section, "Skill %d", &num) == 1) {
				if (num < 0 || num >= SETTINGS_SKILLS) { WARN("%s:%d: [%s]: skills are 0..%d", origin, lineno, section, SETTINGS_SKILLS - 1); section[0] = 0; continue; }
				sec_n = num; snprintf(section, sizeof section, "Skill");
			} else if (strcmp(section, "General") && strcmp(section, "AdditionalFeatures") && strcmp(section, "CustomGameplay")
			           && strcmp(section, "Controller")) {
				WARN("%s:%d: unknown section [%s]", origin, lineno, section); section[0] = 0;
			}
			continue;
		}
		char *eq = strchr(l, '=');
		if (!eq) { WARN("%s:%d: expected key = value", origin, lineno); continue; }
		*eq = 0; char *key = trim(l), *val = trim(eq + 1);
		if (!section[0]) { WARN("%s:%d: '%s' outside a known section", origin, lineno, key); continue; }
		int ok = -1;   /* -1 unknown key, 0 bad value, 1 set */
		if (!strcmp(section, "Level")) {
			if (!strcmp(key, "sword_type")) {
				long v;
				if (ieq(val, "default")) { s->sword_type[sec_n] = d.sword_type[sec_n]; ok = 1; }
				else if (ieq(val, "none")) { s->sword_type[sec_n] = SWORD_NONE; ok = 1; }
				else if (parse_int(val, &v) && (v == 1 || v == 2)) { s->sword_type[sec_n] = (uint8_t)v; ok = 1; }
				else ok = 0;
			}
		} else if (!strcmp(section, "Skill")) {
			for (int t = 0; t < 6; t++) if (!strcmp(key, skill_keys[t])) {
				long v;
				if (ieq(val, "default")) { skill_table(s, t)[sec_n] = skill_table(&d, t)[sec_n]; ok = 1; }
				else if (parse_int(val, &v) && v >= 0 && v <= (t == 5 ? 65535 : 255)) { skill_table(s, t)[sec_n] = (uint16_t)v; ok = 1; }
				else ok = 0;
			}
		} else {
			if (!strcmp(section, "General"))
				for (int k = 0; k < KEY_COUNT; k++) if (!strcmp(key, settings_key_ini_names[k])) {
					if (ieq(val, "default")) snprintf(s->keys[k], sizeof s->keys[k], "%s", d.keys[k]);
					else snprintf(s->keys[k], sizeof s->keys[k], "%s", val);
					ok = 1;
				}
			if (!strcmp(section, "Controller"))
				for (int b = 0; b < BUTTON_COUNT; b++) if (!strcmp(key, settings_button_ini_names[b])) {
					snprintf(s->buttons[b], sizeof s->buttons[b], "%s", ieq(val, "default") ? d.buttons[b] : val);
					ok = 1;
				}
			for (int i = 0; i < NFIELDS && ok < 0; i++)
				if (!strcmp(section, fields[i].section) && !strcmp(key, fields[i].key)) ok = set_field(s, &d, &fields[i], val);
		}
		if (ok < 0) WARN("%s:%d: unknown option '%s' in [%s%s]", origin, lineno, key, section, sec_n >= 0 ? " N" : "");
		else if (ok == 0) WARN("%s:%d: bad value '%s' for '%s' (the default is kept)", origin, lineno, val, key);
	}
	#undef WARN
	return nwarn;
}

int settings_load(pop2_settings *s, const char *path, settings_warn_fn warn)
{
	FILE *f = fopen(path, "rb"); if (!f) return 0;
	size_t cap = 1 << 16, n = 0; char *buf = malloc(cap);
	for (size_t r; buf && (r = fread(buf + n, 1, cap - n - 1, f)) > 0; ) { n += r; if (n + 1 >= cap) { char *b = realloc(buf, cap *= 2); if (!b) { free(buf); buf = NULL; } else buf = b; } }
	fclose(f);
	if (!buf) return 0;
	buf[n] = 0;
	settings_parse_text(s, buf, path, warn);
	free(buf);
	return 1;
}

void settings_write_gameplay(const pop2_settings *s, FILE *f)
{
	const char *sec = "";
	for (int i = 0; i < NFIELDS; i++) {
		const field *fl = &fields[i]; if (!fl->gameplay) continue;
		if (strcmp(sec, fl->section)) { sec = fl->section; fprintf(f, "[%s]\n", sec); }
		fprintf(f, "%s = ", fl->key);
		const char *p = (const char *)s + fl->off;
		if (fl->type == T_BOOL) fprintf(f, "%s\n", *(const int *)p ? "true" : "false"); else fprintf(f, "%d\n", *(const int *)p);
	}
	for (int l = 1; l <= SETTINGS_LEVELS; l++) {
		fprintf(f, "[Level %d]\n", l);
		if (s->sword_type[l] == SWORD_NONE) fprintf(f, "sword_type = none\n"); else fprintf(f, "sword_type = %d\n", s->sword_type[l]);
	}
	for (int k = 0; k < SETTINGS_SKILLS; k++) {
		fprintf(f, "[Skill %d]\n", k);
		for (int t = 0; t < 6; t++) fprintf(f, "%s = %d\n", skill_keys[t], skill_table((pop2_settings *)s, t)[k]);
	}
}
int settings_gameplay_equal(const pop2_settings *a, const pop2_settings *b)
{
	for (int i = 0; i < NFIELDS; i++)
		if (fields[i].gameplay && memcmp((const char *)a + fields[i].off, (const char *)b + fields[i].off, fields[i].size)) return 0;
	if (memcmp(a->sword_type + 1, b->sword_type + 1, SETTINGS_LEVELS)) return 0;
	for (int t = 0; t < 6; t++) if (memcmp(skill_table((pop2_settings *)a, t), skill_table((pop2_settings *)b, t), SETTINGS_SKILLS * 2)) return 0;
	return 1;
}
void settings_copy_gameplay(pop2_settings *dst, const pop2_settings *src)
{
	for (int i = 0; i < NFIELDS; i++) if (fields[i].gameplay) memcpy((char *)dst + fields[i].off, (const char *)src + fields[i].off, fields[i].size);
	memcpy(dst->sword_type, src->sword_type, sizeof dst->sword_type);
	for (int t = 0; t < 6; t++) memcpy(skill_table(dst, t), skill_table((pop2_settings *)src, t), SETTINGS_SKILLS * 2);
}
