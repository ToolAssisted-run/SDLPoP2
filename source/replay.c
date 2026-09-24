/* Replay files (replay.h). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "replay.h"
#include "core.h"
#include "loader.h"
#include "render.h"

#define MAGIC "SDLPoP2 replay 1"
enum { REC_INPUT = 1, REC_QUICKSAVE = 2, REC_QUICKLOAD = 3, REC_CHEATS_OFF = 4, REC_CHEATS_ON = 5, REC_END = 0xFE };
static const char *const game_files[3] = { "PRINCE.OPT", "PRINCE.HOF", "PRINCE.SAV" };

uint32_t replay_screen_checksum(void)   /* FNV-1a over the screen and its palette */
{
	uint32_t h = 2166136261u;
	for (int i = 0; i < SCREEN_W * SCREEN_H; i++) h = (h ^ screen_buf[i]) * 16777619u;
	for (int i = 0; i < 768; i++) h = (h ^ render_palette[i]) * 16777619u;
	return h;
}
static void w32(FILE *f, uint32_t v) { for (int i = 0; i < 4; i++) fputc((int)(v >> 8 * i) & 0xFF, f); }
static void w64(FILE *f, uint64_t v) { for (int i = 0; i < 8; i++) fputc((int)(v >> 8 * i) & 0xFF, f); }
static int r32(FILE *f, uint32_t *v) { uint8_t b[4]; if (fread(b, 1, 4, f) != 4) return 0; *v = b[0] | b[1] << 8 | b[2] << 16 | (uint32_t)b[3] << 24; return 1; }

int replay_record_start(replay_rec *r, const char *path, uint32_t seed, int argc, const char **argv, const pop2_settings *s)
{
	memset(r, 0, sizeof *r);
	r->f = fopen(path, "wb"); if (!r->f) return 0;
	fprintf(r->f, MAGIC "\nseed %u\n", (unsigned)seed);
	for (int i = 0; i < argc && i < 16; i++) fprintf(r->f, "word %s\n", argv[i]);
	for (int i = 0; i < 3; i++) {   /* the game's own files as the program will find them */
		long n = file_size(game_files[i]); if (n <= 0 || n > (1 << 20)) continue;
		uint8_t *b = malloc((size_t)n); if (!b) continue;
		if (file_read(game_files[i], 0, b, n) == n) { fprintf(r->f, "file %s %ld\n", game_files[i], n); fwrite(b, 1, (size_t)n, r->f); fputc('\n', r->f); }
		free(b);
	}
	pop2_settings d; if (!s) { settings_defaults(&d); s = &d; }
	fprintf(r->f, "settings\n"); settings_write_gameplay(s, r->f); fprintf(r->f, "end\n");
	return 1;
}
void replay_record_frame(replay_rec *r, const shell_input *in, int action)
{
	if (!r->f) return;
	int changed = !r->have_last || memcmp(r->last.down, in->down, sizeof in->down) || r->last.shift_flags != in->shift_flags;
	if (changed || in->ntyped > 0) {
		w32(r->f, r->frame); fputc(REC_INPUT, r->f);
		uint8_t bits[12] = {0};
		for (int s = 0; s < 0x60; s++) if (in->down[s]) bits[s >> 3] |= (uint8_t)(1 << (s & 7));
		fwrite(bits, 1, 12, r->f); fputc(in->shift_flags, r->f);
		int n = in->ntyped < 0 ? 0 : in->ntyped > 8 ? 8 : in->ntyped; fputc(n, r->f);
		for (int i = 0; i < n; i++) { fputc(in->typed[i] & 0xFF, r->f); fputc(in->typed[i] >> 8, r->f); }
		r->last = *in; r->have_last = 1;
	}
	static const struct { int bit, rec; } acts[4] = { {REPLAY_CHEATS_OFF, REC_CHEATS_OFF}, {REPLAY_CHEATS_ON, REC_CHEATS_ON}, {REPLAY_QUICKSAVE, REC_QUICKSAVE}, {REPLAY_QUICKLOAD, REC_QUICKLOAD} };
	for (int i = 0; i < 4; i++) if (action & acts[i].bit) { w32(r->f, r->frame); fputc(acts[i].rec, r->f); }
	r->frame++;
}
void replay_record_end(replay_rec *r)
{
	if (!r->f) return;
	w32(r->f, r->frame); fputc(REC_END, r->f); w64(r->f, pop2_hash()); w32(r->f, replay_screen_checksum());
	fclose(r->f); r->f = NULL;
}

static int read_line(FILE *f, char *buf, size_t n)
{
	if (!fgets(buf, (int)n, f)) return 0;
	buf[strcspn(buf, "\r\n")] = 0; return 1;
}
static void next_header(replay_play *p)
{
	int t;
	if (!r32(p->f, &p->next_frame) || (t = fgetc(p->f)) == EOF) { p->eof = 1; p->next_type = 0; return; }
	p->next_type = t;
}
int replay_open(replay_play *p, const char *path, char *err, size_t errlen)
{
	memset(p, 0, sizeof *p);
	settings_defaults(&p->settings);
	p->f = fopen(path, "rb");
	if (!p->f) { snprintf(err, errlen, "cannot open %s", path); return 0; }
	char line[1024];
	if (!read_line(p->f, line, sizeof line) || strcmp(line, MAGIC)) { snprintf(err, errlen, "%s is not an SDLPoP2 replay", path); fclose(p->f); p->f = NULL; return 0; }
	size_t cap = 1 << 14, n = 0; char *ini = malloc(cap); int in_settings = 0, ok = 0; ini[0] = 0;
	while (read_line(p->f, line, sizeof line)) {
		if (in_settings) {
			if (!strcmp(line, "end")) { ok = 1; break; }
			size_t l = strlen(line);
			if (n + l + 2 > cap) { cap = (n + l + 2) * 2; ini = realloc(ini, cap); }
			memcpy(ini + n, line, l); n += l; ini[n++] = '\n'; ini[n] = 0;
		} else if (!strncmp(line, "seed ", 5)) p->seed = (uint32_t)strtoul(line + 5, NULL, 10);
		else if (!strncmp(line, "word ", 5)) { if (p->argc < 16) snprintf(p->argv[p->argc++], sizeof p->argv[0], "%.63s", line + 5); }
		else if (!strncmp(line, "file ", 5)) {
			char name[16]; long size;
			if (sscanf(line + 5, "%15s %ld", name, &size) != 2 || size < 0 || size > (1 << 20) || p->nfiles >= 3) break;
			uint8_t *b = malloc((size_t)size + 1);
			if (fread(b, 1, (size_t)size, p->f) != (size_t)size || fgetc(p->f) != '\n') { free(b); break; }
			snprintf(p->files[p->nfiles].name, sizeof p->files[0].name, "%s", name); p->files[p->nfiles].data = b; p->files[p->nfiles].size = size; p->nfiles++;
		} else if (!strcmp(line, "settings")) in_settings = 1;
		else break;
	}
	if (!ok) { snprintf(err, errlen, "%s: a damaged header", path); free(ini); replay_close(p); return 0; }
	int w = settings_parse_text(&p->settings, ini, path, NULL);
	free(ini);
	if (w) { snprintf(err, errlen, "%s: settings this program does not know", path); replay_close(p); return 0; }
	for (int i = 0; i < p->argc; i++) p->argp[i] = p->argv[i];
	next_header(p);
	return 1;
}
int replay_write_files(const replay_play *p, const char *dir)
{
	char path[1024];
	for (int i = 0; i < p->nfiles; i++) {
		snprintf(path, sizeof path, "%s/%s", dir, p->files[i].name);
		FILE *f = fopen(path, "wb"); if (!f) return 0;
		fwrite(p->files[i].data, 1, (size_t)p->files[i].size, f); fclose(f);
	}
	return 1;
}
int replay_frame(replay_play *p, shell_input *in, int *action)
{
	*action = REPLAY_NONE; p->cur.ntyped = 0;
	while (!p->eof && p->next_frame == p->frame) {
		int t = p->next_type;
		if (t == REC_END) {
			uint8_t b[12];
			if (fread(b, 1, 12, p->f) == 12) {
				p->end_hash = 0; for (int i = 7; i >= 0; i--) p->end_hash = p->end_hash << 8 | b[i];
				p->end_screen = b[8] | b[9] << 8 | b[10] << 16 | (uint32_t)b[11] << 24;
			}
			p->end_frame = p->frame; p->ended = 1; p->eof = 1;
			*in = p->cur; return 0;
		}
		if (t == REC_INPUT) {
			uint8_t bits[12]; int sf, nt;
			if (fread(bits, 1, 12, p->f) != 12 || (sf = fgetc(p->f)) == EOF || (nt = fgetc(p->f)) == EOF || nt > 8) { p->eof = 1; break; }
			for (int s = 0; s < 0x60; s++) p->cur.down[s] = (uint8_t)(bits[s >> 3] >> (s & 7) & 1);
			p->cur.shift_flags = (uint8_t)sf; p->cur.ntyped = nt;
			for (int i = 0; i < nt; i++) { int lo = fgetc(p->f), hi = fgetc(p->f); p->cur.typed[i] = (uint16_t)(lo | hi << 8); }
		} else if (t == REC_QUICKSAVE) *action |= REPLAY_QUICKSAVE;
		else if (t == REC_QUICKLOAD) *action |= REPLAY_QUICKLOAD;
		else if (t == REC_CHEATS_OFF) *action |= REPLAY_CHEATS_OFF;
		else if (t == REC_CHEATS_ON) *action |= REPLAY_CHEATS_ON;
		else { p->eof = 1; break; }
		next_header(p);
	}
	*in = p->cur; p->frame++;
	return !p->eof;   /* (a recording cut short: no end record; replay_verify fails) */
}
int replay_verify(const replay_play *p) { return p->ended && pop2_hash() == p->end_hash && replay_screen_checksum() == p->end_screen; }
void replay_close(replay_play *p)
{
	if (p->f) fclose(p->f);
	for (int i = 0; i < p->nfiles; i++) free(p->files[i].data);
	p->f = NULL; p->nfiles = 0;
}
