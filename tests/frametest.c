/* frametest FRAMES RAM [draw|full] [-v] [-o DIR]: the drawing of every frame of a tools/framecap.py capture.
 *   draw: the tables the game drew (probe at 0FB3:1436) drawn with render.c over the buffer it started from; compares
 *         the offscreen buffer (192 rows) pixel by pixel.
 *   full: the tables built from the captured state (render_frame.c) and drawn; compares the tables and the buffer.
 * RAM: a dump of the same run (the static data segment). One line per drawing pass; a summary at the end. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/types.h"
#include "../src/globals.h"
#include "../src/glue.h"
#include "../src/dat.h"
#include "../src/render.h"
#include "../src/render_tiles.h"
#include "../src/render_frame.h"
#include "snap.h"

typedef struct rec { char label[9]; uint32_t frame, len; uint8_t *data; } rec;
static rec *recs; static int nrecs;
static void load_frames(const char *p)
{
	FILE *f = fopen(p, "rb"); if (!f) { perror(p); exit(2); }
	int cap = 0;
	for (;;) {
		uint8_t h[16]; if (fread(h, 1, 16, f) != 16) break;
		if (nrecs == cap) { cap = cap ? cap * 2 : 1024; recs = realloc(recs, cap * sizeof *recs); }
		rec *r = &recs[nrecs++]; memcpy(r->label, h, 8); r->label[8] = 0;
		r->frame = h[8] | h[9] << 8 | h[10] << 16 | (uint32_t)h[11] << 24; r->len = h[12] | h[13] << 8 | h[14] << 16 | (uint32_t)h[15] << 24;
		r->data = malloc(r->len); if (fread(r->data, 1, r->len, f) != r->len) { nrecs--; break; }
	}
	fclose(f);
}
static uint8_t *load(const char *p) { FILE *f = fopen(p, "rb"); if (!f) { perror(p); exit(2); } uint8_t *b = malloc(655360); if (fread(b, 1, 655360, f) != 655360) exit(2); fclose(f); return b; }
static void write_pgm(const char *path, const uint8_t *a, int h)
{
	FILE *o = fopen(path, "wb"); if (!o) return;
	fprintf(o, "P5 320 %d 255\n", h); fwrite(a, 1, 320 * h, o); fclose(o);
}
static const uint8_t *ds_raw;   /* the DS window of the current pass (DS:2900..6C00) */
static uint16_t dsw(uint16_t a) { return (uint16_t)(ds_raw[a - 0x2900] | ds_raw[a - 0x2900 + 1] << 8); }
static void load_state(const uint8_t *ds, const uint8_t *heap)
{
	ds_raw = ds;
	SNAP_BASE = 0x2900; SNAP_SIZE = 0x4300; snap_load(ds);
	if (heap) for (int i = 0; i < 4; i++) if (floor_ptrs[i] >= 0x9800 && floor_ptrs[i] + 0x65 <= 0xB800) memcpy(floor_objs[i], heap + floor_ptrs[i] - 0x9800, 0x65);   /* (the collapsing floors' objects in the near heap) */
	level_roomlinks = (uint8_t *)&level + 0x17BC; level_number = ((uint8_t *)&level)[0x1847]; level_kind = ds[0x43FD - 0x2900];
	glue_select_guard_dat(level.type);
	for (int i = 0; i < 4; i++) screen_rect[i] = (int16_t)ds_word(0x097E + 2 * i);
}

static int have_lo;
static const char *kind_dat_name(int k) { static const char *n[7] = {NULL, "DESERT.DAT", "TEMPLE.DAT", "CAVERNS.DAT", "RUINS.DAT", "ROOFTOPS.DAT", "FINAL.DAT"}; return k >= 1 && k < 7 ? n[k] : NULL; }
/* the room's drawing context (as tests/tiletest.c): neighbours, description, the kind's drawers, the piece table */
static void room_context(const uint8_t *ds, const uint8_t *heap)
{
	static uint8_t pcopy[0x1000], base[0x1000]; static int pkind = -1;
	if (pkind != level_kind) {
		static dat_file d; char p[512]; snprintf(p, sizeof p, "%s/%s", getenv("PRINCE2_DIR"), kind_dat_name(level_kind));
		if (dat_open(&d, p)) { uint16_t n; const uint8_t *pc = dat_find(&d, "CEIP", 3500, &n); if (pc) memcpy(pcopy, pc, n < sizeof pcopy ? n : sizeof pcopy); }
		pkind = level_kind; memset(base, 0, sizeof base);
	}
	piece_table = pcopy;
	uint16_t pt = ds_word(0x1090);   /* DS:[0x1090]: the game's piece table (some drawers rewrite it) */
	if (!base[0] && !base[1] && !base[2]) memcpy(base, pcopy, sizeof base);
	memcpy(pcopy, base, sizeof pcopy);
	if (heap && pt >= 0x9800 && pt + 0x13 * 0x3D <= 0xB800) {   /* (the pointer is from the end of the run: only when the heap still holds the table there) */
		int diff = 0; for (int i = 0; i < 0x13 * 0x3D; i++) diff += heap[pt - 0x9800 + i] != base[i];
		if (diff < 0x40) memcpy(pcopy, heap + (pt - 0x9800), 0x13 * 0x3D);
	}
	tile_drawers = kind_drawers_for(level_kind);
	set_neighbour_rooms();
	word_2ba6 = dsw(0x2BA6);
	byte_5ce7 = (uint8_t)room_has_description(drawn_room); room_bg = 0; if (byte_5ce7) { int16_t bg = room_description_bg(drawn_room); if (bg >= 0) room_bg = bg + 1; }
	render_desc_load(drawn_room);
	uint16_t dp = ds_word(0x01AC);   /* DS:[0x1AC]: the game's description (its runtime state: objects moved by the drawing) */
	if (have_lo && heap && render_desc_loaded() && dp >= 0x9800 && dp + 0x1C <= 0xB800) {
		int n = 0x1C + 0x19 * (int8_t)heap[dp - 0x9800];
		if (dp + n <= 0xB800 && heap[dp - 0x9800 + 1] == (uint8_t)render_desc_bg()) render_desc_set(heap + dp - 0x9800, n);
	}
	(void)ds;
}
/* the frame's drawing state from a DS window: the requests, the objects, the clip, the counts */
static void frame_state(const uint8_t *ds)
{
	memcpy(&redraw, ds + 0x61E4 - 0x2900, sizeof redraw);
	memcpy(objs, ds + 0x5D3A - 0x2900, sizeof objs); obj_count = dsw(0x60F8);
	for (int k = 0; k < 4; k++) draw_clip[k] = (int16_t)dsw(0x60DE + 2 * k);
	memcpy(sv.rect, ds + 0x6103 - 0x2900, 8);
}
static int compare_tables(const uint8_t *tables, const uint8_t *counts, int verbose)
{
	int bad = 0;
	for (int t = 0; t < 4; t++) {
		if (t == 2) continue;
		int want = counts[2 * t] | counts[2 * t + 1] << 8, got = table_counts[t];
		const uint8_t *exp = tables + (t == 0 ? 0 : t == 1 ? 0xA28 : 0x11F8);
		const uint8_t *mine = t == 0 ? (const uint8_t *)back_table : t == 1 ? (const uint8_t *)fore_table : (const uint8_t *)sprite_table;
		int n = got > want ? got : want, b = 0;
		for (int i = 0; i < n; i++)
			if (i >= got || i >= want || memcmp(exp + 20 * i, mine + 20 * i, 20)) {
				if (verbose && b < 6) {
					printf("  table %d #%d:", t, i);
					if (i < want) { printf(" game"); for (int k = 0; k < 20; k++) printf(" %02x", exp[20 * i + k]); }
					if (i < got) { printf("\n            mine"); for (int k = 0; k < 20; k++) printf(" %02x", mine[20 * i + k]); }
					printf("\n");
				}
				b++;
			}
		if (b && verbose) printf("  table %d: %d entries, game %d, %d differ\n", t, got, want, b);
		bad += b;
	}
	return bad;
}
/* the screen dumps (records 'vram' of tools/framecap.py with VRAM_STEP, or the files DIR/v<frame>.bin of oracle
 * 'mem F 4' dumps (chain-4: pixel a at (a & ~3) * 4 + (a & 3))): compared with the screen after the last pass before */
static const char *screen_out;
static uint8_t screen_before[320 * 200]; static uint32_t pass_end_frame = 0xFFFFFFFF, end_slack; static int vram_timing;
static const char *vram_dir; static int vram_synced, vram_checked, vram_bad, vram_px; static int next_dump;
static int *dump_idx, n_dumps, next_rec;   /* the 'vram' records in frame order */
static int cmp_dump(const void *a, const void *b) { return (int)recs[*(const int *)a].frame - (int)recs[*(const int *)b].frame; }
static void compare_screen(const uint8_t *v, int linear, int frame, int verbose)
{
	if (!vram_synced) return;
	int d = 0, ds = 0;
	for (int a = 0; a < 320 * 200; a++) {
		if (a >= 320 * 193 && a % 320 >= 98 && a % 320 < 235) continue;   /* (the status line's messages, DS:098E: text.c's, not drawn here) */
		if ((linear ? v[a] : v[(a & ~3) * 4 + (a & 3)]) != screen_buf[a]) { if (a < 320 * 192) d++; else ds++; }
	}
	if ((d || ds) && (uint32_t)frame >= pass_end_frame && (uint32_t)frame <= pass_end_frame + end_slack) {   /* (a dump of the pass's last frame: taken before or during its copies to the screen) */
		int d2 = 0, ds2 = 0, dm = 0;
		for (int a = 0; a < 320 * 200; a++) {
			if (a >= 320 * 193 && a % 320 >= 98 && a % 320 < 235) continue;
			uint8_t g = linear ? v[a] : v[(a & ~3) * 4 + (a & 3)];
			if (g != screen_before[a]) { if (a < 320 * 192) d2++; else ds2++; }
			if (g != screen_before[a] && g != screen_buf[a]) dm++;
		}
		if (d2 + ds2 == 0 || dm == 0) { vram_timing++; return; }
	}
	vram_checked++; if (d || ds) { vram_bad++; vram_px += d + ds; }
	if (d || ds || verbose) printf("  screen %d: %d px differ, status line %d\n", frame, d, ds);
	if ((d || ds) && screen_out) {
		char p[512]; uint8_t g[320 * 200]; for (int a = 0; a < 320 * 200; a++) g[a] = linear ? v[a] : v[(a & ~3) * 4 + (a & 3)];
		snprintf(p, sizeof p, "%s/s%05d_mine.pgm", screen_out, frame); write_pgm(p, screen_buf, 200);
		snprintf(p, sizeof p, "%s/s%05d_game.pgm", screen_out, frame); write_pgm(p, g, 200);
	}
}
static uint32_t ds_frame; static int has_lfr, in_gap, has_blkgo; static int16_t last_box[4];
static void skip_vram(uint32_t upto) { while (next_rec < n_dumps && recs[dump_idx[next_rec]].frame < upto) next_rec++; }
static void check_vram(uint32_t upto, int verbose)
{
	if (in_gap) { skip_vram(upto); return; }
	for (; next_rec < n_dumps && recs[dump_idx[next_rec]].frame < upto; next_rec++) compare_screen(recs[dump_idx[next_rec]].data, 1, (int)recs[dump_idx[next_rec]].frame, verbose);
	if (!vram_dir) return;
	if (upto > (uint32_t)next_dump + 3000) upto = (uint32_t)next_dump + 3000;
	for (; next_dump < (int)upto; next_dump++) {
		char p[512]; snprintf(p, sizeof p, "%s/v%d.bin", vram_dir, next_dump);
		FILE *f = fopen(p, "rb"); if (!f) continue;
		static uint8_t v[0x40000]; size_t n = fread(v, 1, sizeof v, f); fclose(f); (void)n;
		compare_screen(v, 0, next_dump, verbose);
	}
}
int main(int argc, char **argv)
{
	if (argc < 3) { fprintf(stderr, "usage: frametest FRAMES RAM [draw|full] [-v] [-o DIR]\n"); return 2; }
	const char *mode = "draw", *outdir = NULL; int verbose = 0;
	for (int i = 3; i < argc; i++) { if (!strcmp(argv[i], "-V") && i + 1 < argc) vram_dir = argv[++i]; else if (!strcmp(argv[i], "-v")) verbose = 1; else if (!strcmp(argv[i], "-o") && i + 1 < argc) outdir = screen_out = argv[++i]; else mode = argv[i]; }
	load_frames(argv[1]);
	uint32_t last_frame = 0; for (int i = 0; i < nrecs; i++) if (strcmp(recs[i].label, "vram") && strcmp(recs[i].label, "shot") && recs[i].frame > last_frame) last_frame = recs[i].frame;
	for (int i = 0; i < nrecs; i++) { if (!strcmp(recs[i].label, "lfr_hp")) has_lfr = 1; if (!strcmp(recs[i].label, "blkgo")) has_blkgo = 1; }
	for (int i = 0; i < nrecs; i++) if (!strcmp(recs[i].label, "vram")) { dump_idx = realloc(dump_idx, (n_dumps + 1) * sizeof *dump_idx); dump_idx[n_dumps++] = i; }
	if (n_dumps) qsort(dump_idx, n_dumps, sizeof *dump_idx, cmp_dump);
	uint8_t *ram = load(argv[2]);
	{ char p1[512], p2[512]; snprintf(p1, sizeof p1, "%s/SEQUENCE.DAT", getenv("PRINCE2_DIR")); snprintf(p2, sizeof p2, "%s/PRINCE.EXE", getenv("PRINCE2_DIR"));
	  glue_init(p1, "/dev/null"); glue_load_exe_tables(p2); }
	glue_load_ds_tables(ram);
	const uint8_t *ds = NULL, *heap = NULL, *start = NULL;
	int passes = 0, bad_passes = 0, rd_pending = 0, pass_kind = 0, synced = 0, after_room = 0; long bad_px = 0;
	for (int i = 0; i < nrecs; i++) {
		rec *r = &recs[i];
		if (!strcmp(mode, "full") && vram_synced) {   /* the tick's own screen writes (tools/framecap.py probes) */
			const int16_t *w = (const int16_t *)r->data;
			if (!strcmp(r->label, "blk") || !strcmp(r->label, "blkgo") || !strcmp(r->label, "lfr_er") || !strcmp(r->label, "lfr_hp") || !strcmp(r->label, "msgerase") || !strcmp(r->label, "msgclear") || !strcmp(r->label, "msgclral") || !strcmp(r->label, "hpbars")) { check_vram(r->frame, verbose); memcpy(screen_before, screen_buf, sizeof screen_before); pass_end_frame = r->frame; end_slack = 2; }   /* (a dump of the same frame or the next two: before or after; the probes and the dumps are not taken at the same point of a frame) */
			if (!strcmp(r->label, "blk")) memcpy(last_box, w, sizeof last_box);
			if (!strcmp(r->label, "blk") && !has_blkgo) { check_vram(r->frame, verbose); render_room_switch(w); }
			else if (!strcmp(r->label, "blkgo")) {   /* (0FB3:29C7: the colors were not blacked out already, e.g. by a flash) */
				check_vram(r->frame, verbose); render_pal_restore();
				if ((uint16_t)w[0] < 0x100) render_room_switch(w);   /* (older captures: the box DS:5B62) */
				else if ((uint16_t)w[0] == 0x5D0A) render_room_switch(last_box);   /* (the box from the blk record; with the offscreen port current the erase misses the screen) */
				else render_pal_blackout();
			}
			else if (!strcmp(r->label, "msgkey")) { check_vram(r->frame + 1, verbose); in_gap = 1; }   /* (a restart / level end: the screens up to the next level-first-room erase or pass are the shell's and nis.c's) */
			else if (!strcmp(r->label, "lfr_er")) { if (in_gap) skip_vram(r->frame); in_gap = 0; check_vram(r->frame, verbose); render_erase_screen((const int16_t[4]){0, 0, 200, 320}); }
			else if (!strcmp(r->label, "lfr_hp")) { check_vram(r->frame, verbose); render_kid_hp((int8_t)r->data[0], (int8_t)r->data[1]); }
			else if (!strcmp(r->label, "msgerase")) { check_vram(r->frame, verbose); render_msg_erase(); }
			else if (!strcmp(r->label, "msgclral")) { check_vram(r->frame, verbose); render_msg_erase(); }   /* (0FB3:2144: DS:5CDC 0x258) */
			else if (!strcmp(r->label, "msgclear")) { check_vram(r->frame, verbose); word_5cdc = 0; render_msg_clear(); }   /* (DS:5CDC from the last pass) */
			else if (!strcmp(r->label, "hpbars")) { check_vram(r->frame, verbose); char_type k = Kid; memcpy(&Kid, r->data, 0x40); render_hp_bars(); Kid = k; }
		}
		if (!strcmp(mode, "objs") && !strcmp(r->label, "pre_ds")) {   /* 0FB3:12F4 up to 1308 from the state before it, against the state at 1308 */
			const uint8_t *hp = NULL, *mid = NULL, *mt = NULL, *mc = NULL;
			for (int k = i + 1; k < nrecs && k <= i + 10; k++) {
				if (!strcmp(recs[k].label, "pre_heap")) hp = recs[k].data;
				if (!strcmp(recs[k].label, "mid_ds")) mid = recs[k].data;
				if (!strcmp(recs[k].label, "mid_tab")) mt = recs[k].data;
				if (!strcmp(recs[k].label, "mid_cnt")) { mc = recs[k].data; break; }
			}
			if (!hp || !mid || !mt || !mc) continue;
			load_state(r->data, hp); ds_raw = r->data;
			redraw_all_flag = 0;
			room_context(r->data, hp);
			frame_state(r->data);
			saved_count = dsw(0x5FEC); if (saved_count > SAVED_MAX) saved_count = 0;   /* the saved screens: ids, kinds, flags, rects (from the heap) */
			for (int k = 0; k < saved_count; k++) {
				uint16_t h = dsw((uint16_t)(0x5FEE + 6 * k)); saved_bg *b = &saved_bgs[k];
				b->id = r->data[0x5FF0 + 6 * k - 0x2900]; b->kind = r->data[0x5FF1 + 6 * k - 0x2900]; b->flag = dsw((uint16_t)(0x5FF2 + 6 * k));
				for (int q = 0; q < 4; q++) b->rect[q] = h >= 0x9800 && h + 0x18 < 0xB800 ? (int16_t)(hp[h - 0x9800 + 0x10 + 2 * q] | hp[h - 0x9800 + 0x11 + 2 * q] << 8) : 0;
			}
			missing_reset();
			render_frame_objects();
			/* compare: the objects, the requests, the tables so far */
			int b = 0;
			uint16_t want_n = (uint16_t)(mid[0x60F8 - 0x2900] | mid[0x60F9 - 0x2900] << 8);
			if (want_n != obj_count) { b++; if (verbose) printf("  objects: %u, game %u\n", obj_count, want_n); }
			for (int k = 0; k < want_n && k < obj_count; k++) if (memcmp(&objs[k], mid + 0x5D3A - 0x2900 + 0x17 * k, 0x17)) {
				b++;
				if (verbose) { printf("  obj %d game", k); for (int q = 0; q < 0x17; q++) printf(" %02x", mid[0x5D3A - 0x2900 + 0x17 * k + q]); printf("\n        mine"); for (int q = 0; q < 0x17; q++) printf(" %02x", ((uint8_t *)&objs[k])[q]); printf("\n"); }
			}
			const uint8_t *rq = mid + 0x61E4 - 0x2900, *mq = (const uint8_t *)&redraw;
			for (int q = 0; q < (int)sizeof redraw; q++) if (rq[q] != mq[q]) { b++; if (verbose) printf("  requests +%X (DS:%04X): %02x game %02x\n", q, 0x61E4 + q, mq[q], rq[q]); }
			b += compare_tables(mt, mc, verbose);
			passes++; if (b) bad_passes++;
			if (b || verbose) printf("frame %u room %u: %d differ %s\n", r->frame, drawn_room, b, missing_log());
			continue;
		}
		if (!strcmp(mode, "tiles") && !strcmp(r->label, "mid_ds")) {   /* 0FB3:1308 from the state before it, against the tables drawn */
			const uint8_t *mt = NULL, *mc = NULL, *tables = NULL, *counts = NULL;
			for (int k = i + 1; k < nrecs && k <= i + 8; k++) {
				if (!strcmp(recs[k].label, "mid_tab")) mt = recs[k].data;
				if (!strcmp(recs[k].label, "mid_cnt")) mc = recs[k].data;
				if (!strcmp(recs[k].label, "tables")) tables = recs[k].data;
				if (!strcmp(recs[k].label, "counts")) { counts = recs[k].data; break; }
			}
			if (!mt || !mc || !tables || !counts) continue;
			load_state(r->data, heap); ds_raw = r->data;
			redraw_all_flag = 0;
			room_context(r->data, heap);
			frame_state(r->data);
			memcpy(back_table, mt, sizeof back_table); memcpy(fore_table, mt + 0xA28, sizeof fore_table); memcpy(sprite_table, mt + 0x11F8, sizeof sprite_table);
			for (int t = 0; t < 5; t++) table_counts[t] = mc[2 * t] | mc[2 * t + 1] << 8;
			missing_reset();
			if (getenv("FT_FRAME") && (uint32_t)atoi(getenv("FT_FRAME")) == r->frame) {
				for (int t = 0; t < 30; t++) {
					if (redraw.full[t]) printf("  full %d\n", t);
					if (redraw.fore_full[t]) printf("  fore_full %d\n", t);
					if (redraw.objs_at[t]) printf("  objs_at %d\n", t);
					const redraw_rect *rr[3] = {&redraw.back[t], &redraw.fore_part[t], &redraw.fore2[t]}; const char *nm[3] = {"back", "fore_part", "fore2"};
					for (int k = 0; k < 3; k++) if (rr[k]->set) printf("  %s %d: %d %d %d %d\n", nm[k], t, rr[k]->rect[0], rr[k]->rect[1], rr[k]->rect[2], rr[k]->rect[3]);
				}
				for (int c = 0; c < 10; c++) if (redraw.above[c].set) printf("  above %d: %d %d %d %d\n", c, redraw.above[c].rect[0], redraw.above[c].rect[1], redraw.above[c].rect[2], redraw.above[c].rect[3]);
				for (int k = 0; k < obj_count; k++) printf("  obj %d: x %d y %d chtab %u id %u frame %u dir %u type %u rect %d %d %d %d key %u mask %x idx %u\n", k, objs[k].x, objs[k].y, objs[k].chtab, objs[k].id, objs[k].frame, objs[k].dir, objs[k].type, objs[k].rect[0], objs[k].rect[1], objs[k].rect[2], objs[k].rect[3], objs[k].key, objs[k].mask, objs[k].charidx);
				printf("  clip %d %d %d %d\n", draw_clip[0], draw_clip[1], draw_clip[2], draw_clip[3]);
			}
			redraw_requested();
			render_sort_tables();
			for (int t = 0; t < 2; t++) { draw_entry *e = t ? fore_table : back_table; for (int k = 0; k < table_counts[t]; k++) if (e[k].id >= 0x6370 && e[k].id <= 0x6372) e[k].id = 0; }   /* (0FB3:0D20: cleared once drawn) */
			int b = compare_tables(tables, counts, verbose);
			passes++; if (b) bad_passes++;
			if (b || verbose) printf("frame %u room %u: %d entries differ %s\n", r->frame, drawn_room, b, missing_log());
			continue;
		}
		if (!strcmp(r->label, "pre_lo") || !strcmp(r->label, "rd_lo") || !strcmp(r->label, "rd2_lo")) {   /* the static data as it is then (DS:0000..2900: the drawing rewrites some) */
			memcpy(ram + 0x3B250, r->data, r->len < 0x2900 ? r->len : 0x2900); glue_load_ds_tables(ram); render_mob_box_reset(); have_lo = 1;
		}
		if (!strcmp(r->label, "pre_ds") || !strcmp(r->label, "rd_ds") || !strcmp(r->label, "rd2_ds")) { ds = r->data; if (!strcmp(r->label, "rd_ds") || (!strcmp(r->label, "pre_ds"))) ds_frame = r->frame; if (!strcmp(r->label, "rd_ds")) rd_pending = 1; pass_kind = !strcmp(r->label, "rd_ds") ? 1 : 2; }
		else if (!strcmp(r->label, "pre_heap") || !strcmp(r->label, "rd_heap") || !strcmp(r->label, "rd2_heap")) heap = r->data;
		else if (!strcmp(r->label, "pre_buf") || !strcmp(r->label, "rd_buf") || !strcmp(r->label, "buf")) {
			if (!strcmp(r->label, "buf") && start && !strcmp(mode, "full")) {   /* a pass from the game's state at its start */
				const uint8_t *tables = NULL, *counts = NULL, *dirty = NULL;
				for (int k = i - 1; k >= 0 && k >= i - 5; k--) { if (!strcmp(recs[k].label, "tables")) tables = recs[k].data; if (!strcmp(recs[k].label, "counts")) counts = recs[k].data; if (!strcmp(recs[k].label, "dirty")) dirty = recs[k].data; }
				if (!ds || !tables || !counts || !dirty) { start = r->data; continue; }
				load_state(ds, heap);
				room_context(ds, heap);
				frame_state(ds);
				memcpy(offscreen, start, 320 * 192);
				render_owner = NULL;
				missing_reset();
				if (in_gap) { skip_vram(ds_frame); in_gap = 0; }
				check_vram(ds_frame, verbose);   /* the screen dumps before this pass: the screen after the previous one */
				skip_vram(r->frame);             /* (those during the pass: the status line is drawn at its start, the rest copied at its end) */
				memcpy(screen_before, screen_buf, sizeof screen_before); pass_end_frame = r->frame; end_slack = 0;
				if (pass_kind == 1) { if (!synced) { render_reset_images(); memset(screen_buf + 320 * 192, 0, 320 * 8); } render_redraw_room(); synced = 1; }   /* (the level's first room: 169B:03DE erases the screen first) */
				else {
					redraw_all_flag = 0;
					if (!synced) {   /* the saved screens from the game's list (their pixels unknown until a whole redraw) */
						saved_count = dsw(0x5FEC); if (saved_count > SAVED_MAX) saved_count = 0;
						for (int k = 0; k < saved_count; k++) {
							uint16_t h = dsw((uint16_t)(0x5FEE + 6 * k)); saved_bg *sb = &saved_bgs[k];
							sb->id = ds[0x5FF0 + 6 * k - 0x2900]; sb->kind = ds[0x5FF1 + 6 * k - 0x2900]; sb->flag = dsw((uint16_t)(0x5FF2 + 6 * k));
							for (int q = 0; q < 4; q++) sb->rect[q] = heap && h >= 0x9800 && h + 0x18 < 0xB800 ? (int16_t)(heap[h - 0x9800 + 0x10 + 2 * q] | heap[h - 0x9800 + 0x11 + 2 * q] << 8) : 0;
							int nb = (sb->rect[2] - sb->rect[0]) * (sb->rect[3] - sb->rect[1]); if (nb < 0) nb = 0;
							sb->bits = realloc(sb->bits, nb + 1); memset(sb->bits, 0, nb + 1);
						}
					}
					else if (dsw(0x5FEC) == saved_count) {   /* the tick's requests on the saved screens (0CD6:0684 from the animations): their flags */
						for (int k = 0; k < saved_count; k++) if (saved_bgs[k].id == ds[0x5FF0 + 6 * k - 0x2900]) saved_bgs[k].flag = dsw((uint16_t)(0x5FF2 + 6 * k));
					} else if (verbose) printf("  saved screens: %d, game %d\n", saved_count, dsw(0x5FEC));
					render_frame_tables(); render_draw_tables();
				}
				for (int t = 0; t < 2; t++) { draw_entry *e = t ? fore_table : back_table; for (int k = 0; k < table_counts[t]; k++) if (e[k].id >= 0x6370 && e[k].id <= 0x6372) e[k].id = 0; }
				int tb = compare_tables(tables, counts, verbose);
				int nd = dirty[0] | dirty[1] << 8, db = 0;
				if (pass_kind != 1) {
					if (nd != dirty_count) db++;
					for (int k = 0; k < nd && k < dirty_count; k++) for (int q = 0; q < 4; q++) if ((int16_t)(dirty[0x24 + 8 * k + 2 * q] | dirty[0x25 + 8 * k + 2 * q] << 8) != dirty_rects[k][q]) db++;
					if (db && verbose) { printf("  dirty %d, game %d:", dirty_count, nd); for (int k = 0; k < nd; k++) printf(" (%d %d %d %d)", (int16_t)(dirty[0x24 + 8 * k] | dirty[0x25 + 8 * k] << 8), (int16_t)(dirty[0x26 + 8 * k] | dirty[0x27 + 8 * k] << 8), (int16_t)(dirty[0x28 + 8 * k] | dirty[0x29 + 8 * k] << 8), (int16_t)(dirty[0x2A + 8 * k] | dirty[0x2B + 8 * k] << 8)); printf("\n   mine:"); for (int k = 0; k < dirty_count; k++) printf(" (%d %d %d %d)", dirty_rects[k][0], dirty_rects[k][1], dirty_rects[k][2], dirty_rects[k][3]); printf("\n"); }
				}
				int d = 0; for (int p = 0; p < 320 * 192; p++) if (offscreen[p] != r->data[p]) d++;
				passes++; if (tb || db || (d && synced)) { bad_passes++; bad_px += d; }
				if (tb || db || (d && synced) || verbose) printf("frame %u room %u pass %d%s: tables %d, dirty %d, %d px differ %s\n", r->frame, drawn_room, pass_kind, synced ? "" : " (unsynced)", tb, db, d, missing_log());
				if (d && outdir) {
					char p[512]; snprintf(p, sizeof p, "%s/f%05u_mine.pgm", outdir, r->frame); write_pgm(p, offscreen, 192);
					snprintf(p, sizeof p, "%s/f%05u_game.pgm", outdir, r->frame); write_pgm(p, r->data, 192);
				}
				if (pass_kind == 2) {
					if (after_room) {
						render_present_all();
						render_pal_restore();   /* (0FB3:294C: the next room switch blacks out again) */
						if (!vram_synced && !has_lfr) render_kid_hp((int8_t)Kid.f12, (int8_t)Kid.f13);   /* (older captures: 169B:03AE draws the hit points after the first room) */
						vram_synced = 1;
					} else render_present();
				}
				after_room = pass_kind == 1;
				pass_kind = 2;   /* (a pass after a whole redraw's first starts from the state at 169B:04A2) */
				start = r->data;
				continue;
			}
			if (!strcmp(r->label, "buf") && start && !strcmp(mode, "draw")) {   /* the end of a pass: compare */
				const uint8_t *tables = NULL, *counts = NULL;
				for (int k = i - 1; k >= 0 && k >= i - 4; k--) { if (!strcmp(recs[k].label, "tables")) tables = recs[k].data; if (!strcmp(recs[k].label, "counts")) counts = recs[k].data; }
				if (!ds || !tables || !counts) { start = r->data; continue; }
				load_state(ds, heap);
				room_context(ds, heap);
				redraw_all_flag = dsw(0x610E) || rd_pending; rd_pending = 0;
				memcpy(offscreen, start, 320 * 192);
				memcpy(back_table, tables, sizeof back_table); memcpy(fore_table, tables + 0xA28, sizeof fore_table); memcpy(sprite_table, tables + 0x11F8, sizeof sprite_table);
				for (int t = 0; t < 5; t++) table_counts[t] = counts[2 * t] | counts[2 * t + 1] << 8;
				static int16_t owner[320 * 200]; for (int p = 0; p < 320 * 200; p++) owner[p] = -1; render_owner = owner;
				if (!strcmp(mode, "draw")) {
					dirty_count = 0;
					if (redraw_all_flag) render_free_saved();
					render_restore_saved();
					int saved = !(redraw_all_flag && render_desc_loaded());
					for (int k = 0; k < table_counts[0]; k++) { if (!saved && back_table[k].piece) { render_desc_save_under(); saved = 1; } render_owner_id = k; render_draw_entry(&back_table[k]); }
					for (int k = 0; k < table_counts[3]; k++) { render_owner_id = 3000 + k; render_draw_sprite(&sprite_table[k]); }
					saved = !(redraw_all_flag && render_desc_loaded());
					for (int k = 0; k < table_counts[1]; k++) { if (!saved && back_table[k].piece) { render_desc_save_under(); saved = 1; } render_owner_id = 1000 + k; render_draw_entry(&fore_table[k]); }
				}
				int d = 0, x0 = 320, x1 = -1, y0 = 200, y1 = -1;
				for (int p = 0; p < 320 * 192; p++) if (offscreen[p] != r->data[p]) { d++; int x = p % 320, y = p / 320; if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y; }
				passes++; if (d) { bad_passes++; bad_px += d; }
				if (d || verbose) printf("frame %u room %u counts %u/%u/%u redraw %u: %d px differ (x %d..%d, y %d..%d)\n", r->frame, drawn_room, table_counts[0], table_counts[3], table_counts[1], redraw_all_flag, d, x0, x1, y0, y1);
				if (d && verbose) {   /* the entries that last wrote the differing pixels */
					static int cnt[4100]; memset(cnt, 0, sizeof cnt);
					for (int p = 0; p < 320 * 192; p++) if (offscreen[p] != r->data[p]) cnt[owner[p] < 0 ? 4099 : owner[p]]++;
					int shown = 0;
					for (int p = 0; p < 320 * 192 && shown < 12; p++) if (offscreen[p] != r->data[p]) { printf("   (%d,%d) mine %02x game %02x start %02x owner %d\n", p % 320, p / 320, offscreen[p], r->data[p], start[p], owner[p]); shown++; }
					for (int k = 0; k < 4100; k++) if (cnt[k]) {
						if (k == 4099) { printf("  (no entry): %d\n", cnt[k]); continue; }
						int t = k / 1000, j = k % 1000;
						if (t == 3) { const sprite_entry *e = &sprite_table[j]; printf("  S%d chtab %u id %u (%d,%d) mode %u mask %x mir %u: %d\n", j, e->chtab, e->id, e->x, e->y, e->mode, e->mask, e->mirror, cnt[k]); }
						else { const draw_entry *e = t ? &fore_table[j] : &back_table[j]; printf("  T%d#%d chtab %u id %u (%d,%d) mode %u mir %u: %d\n", t, j, e->chtab, e->id, e->x, e->y, e->mode, e->mirror, cnt[k]); }
					}
				}
				if (d && outdir) {
					char p[512]; snprintf(p, sizeof p, "%s/f%05u_mine.pgm", outdir, r->frame); write_pgm(p, offscreen, 192);
					snprintf(p, sizeof p, "%s/f%05u_game.pgm", outdir, r->frame); write_pgm(p, r->data, 192);
					snprintf(p, sizeof p, "%s/f%05u_start.pgm", outdir, r->frame); write_pgm(p, start, 192);
				}
			}
			start = r->data;
		}
	}
	if (!strcmp(mode, "tiles") || !strcmp(mode, "objs")) { printf("%s: %d frames, %d differ\n", argv[1], passes, bad_passes); return bad_passes != 0; }
	if (vram_dir || n_dumps) { check_vram(last_frame + 1, verbose);   /* (not past the capture's level: the story scene after it is nis.c's) */
		 printf("screens: %d compared, %d differ (%d px), %d mid-copy\n", vram_checked, vram_bad, vram_px, vram_timing); }
	printf("%s: %d passes, %d differ, %ld px\n", argv[1], passes, bad_passes, bad_px);
	return bad_passes != 0;
}
