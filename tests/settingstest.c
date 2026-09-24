/* SDLPoP2.ini and replays, headless (the 'settings' suite).
 *  1. the repository's SDLPoP2.ini parses without a warning to exactly the built-in defaults, and those defaults are
 *     the game's own tables (PRINCE.EXE's DS:1BB6 guard probabilities and DS:13D0 refractory timers);
 *  2. the parser: unknown keys and bad values warn, `default` restores;
 *  3. the whole program (the shell) on a scripted session (the intro cut short, play, quick saves and loads): with no
 *     settings, with the default settings installed, and while recording a replay: the same states every frame;
 *  4. the replay played back reproduces the recording (pop2_hash and the screen at the end), also with non-default
 *     gameplay settings, which travel in the replay file and change the game, and with the cheats turned on and off
 *     during play (the frontend's toggle, a replay action) and a cheat key used meanwhile;
 *  5. no settings = the default settings on other levels (guards, sword types);
 *  6. the shell's options: story scenes off (the same state after them), the copy protection off or later, the intro
 *     off, skip_title with first_level, start_minutes_left and a level's sword_type.
 * Each run is a child process (the shell is a process-wide coroutine). The game's own files go to scratch directories.
 * usage: settingstest GAME_DIR SDLPoP2.ini */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "../source/core.h"
#include "../source/shell.h"
#include "../source/settings.h"
#include "../source/replay.h"
#include "../source/loader.h"
#include "../source/render.h"

extern uint16_t minutes_left;   /* game.c: DS:5CD2 */
static const char *game_dir; static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL: " __VA_ARGS__); printf("\n"); } else if (getenv("VERBOSE")) { printf("ok: " __VA_ARGS__); printf("\n"); } } while (0)

static int nwarn; static void count_warn(const char *m) { nwarn++; if (getenv("VERBOSE")) printf("  (warning) %s\n", m); }

/* ---- the scripted session ---- */
enum { FRAMES = 9000, SAVE1 = 2600, LOAD1 = 4200, SAVE2 = 5000, LOAD2 = 7600 };
typedef struct result { uint64_t digest, hash; uint32_t screen; int minutes, verified, frames, mode_play, saves, loads, cheats; } result;
enum { CHEATS_ON1 = 3000, CHEATS_OFF1 = 3400 };
static int cheat_script;   /* the session toggles the cheats (section 4) */
static uint32_t lcg;
static uint32_t rnd(void) { lcg = lcg * 1103515245u + 12345u; return lcg >> 16; }
static void script_input(int f, shell_input *in, int *action)
{
	static const int keys[5] = { 0x4B, 0x4D, 0x48, 0x50, 0x2A };
	*action = REPLAY_NONE;
	if (f == 0) lcg = 777;
	if (f == 60) shell_input_key(in, 0x01, 1, 0x1B);   /* Esc: the intro cut short, the game starts */
	if (f == 61) shell_input_key(in, 0x01, 0, 0);
	if (f > 200 && f % 11 == 0) {
		for (int k = 0; k < 5; k++) if (in->down[keys[k]]) shell_input_key(in, keys[k], 0, 0);
		uint32_t r = rnd();
		int x = r % 5; if (x < 4) shell_input_key(in, keys[x], 1, 0);
		if ((r >> 4) % 4 == 0) shell_input_key(in, 0x2A, 1, 0);
		if ((r >> 7) % 23 == 0) shell_input_key(in, 0x39, 1, 0x20), shell_input_key(in, 0x39, 0, 0);   /* space: the time left */
	}
	if (f == SAVE1 || f == SAVE2) *action = REPLAY_QUICKSAVE;
	if (f == LOAD1 || f == LOAD2) *action = REPLAY_QUICKLOAD;
	if (cheat_script) {   /* (no cheat word) the cheats on, 'T' (one more hit point), off, 'T' again (nothing), '+' */
		if (f == CHEATS_ON1) *action |= REPLAY_CHEATS_ON;
		if (f == CHEATS_OFF1) *action |= REPLAY_CHEATS_OFF;
		if (f == CHEATS_ON1 + 100 || f == CHEATS_OFF1 + 100) shell_input_type(in, 'T');
		if (f == CHEATS_OFF1 + 200) shell_input_type(in, '+');
	}
}
static void scratch_dir(char *out, size_t n) { snprintf(out, n, "/tmp/sdlpop2-settingstest-XXXXXX"); if (!mkdtemp(out)) { perror("mkdtemp"); exit(2); } }
static void remove_dir(const char *d)
{
	const char *names[3] = { "PRINCE.OPT", "PRINCE.HOF", "PRINCE.SAV" }; char p[512];
	for (int i = 0; i < 3; i++) { snprintf(p, sizeof p, "%s/%s", d, names[i]); unlink(p); }
	rmdir(d);
}
/* one run in this (child) process: settings (NULL: none), recording to rec_path, or playing back play_path */
static const char *level_word;   /* NULL: the program from its start (title, intro); else "yippeeyahoo LEVELn" */
/* The copy protection has no switch. Sessions on levels 3 and later that test something else mark it as already
 * answered in the game's memory (DS:0366, as the oracle captures poke state); section 6 checks it is asked. */
static int cp_answered; extern uint16_t word_0366;   /* (level.c) */
static result session(const pop2_settings *s, const char *rec_path, const char *play_path)
{
	result res = {0}; char dir[256]; scratch_dir(dir, sizeof dir);
	snprintf(file_dir, sizeof file_dir, "%s", dir);
	static replay_play p; static replay_rec rec; static pop2_settings ps;
	uint32_t seed = 0x5EED; const char *words[2] = { "yippeeyahoo", level_word }; int nwords = level_word ? 2 : 0;
	if (play_path) {
		char err[256];
		if (!replay_open(&p, play_path, err, sizeof err)) { printf("replay: %s\n", err); exit(3); }
		replay_write_files(&p, dir);
		ps = p.settings; s = &ps; seed = p.seed; nwords = p.argc;
	}
	pop2_settings_game = s;
	shell_set_seed(seed);
	if (!shell_init(game_dir, nwords, play_path ? p.argp : words)) { printf("shell_init failed\n"); exit(3); }
	if (cp_answered) word_0366 = 1;
	if (rec_path && !replay_record_start(&rec, rec_path, seed, nwords, words, s)) { printf("cannot write %s\n", rec_path); exit(3); }
	static shell_input in; uint64_t dg = 1469598103934665603ull;
	for (int f = 0; f < FRAMES; f++) {
		int action;
		if (play_path) { if (!replay_frame(&p, &in, &action)) break; }
		else script_input(f, &in, &action);
		if (rec_path) replay_record_frame(&rec, &in, action);
		if (action & REPLAY_CHEATS_OFF) shell_set_cheats(0);
		if (action & REPLAY_CHEATS_ON) shell_set_cheats(1);
		if (action & REPLAY_QUICKSAVE) shell_quicksave();
		if (action & REPLAY_QUICKLOAD) shell_quickload();
		int r = shell_step(&in);
		in.ntyped = 0;
		int q = shell_quick_result(); if (q == 1) res.saves++; if (q == 2) res.loads++;
		if (f == CHEATS_ON1 + 50 || f == CHEATS_OFF1 + 50) res.cheats = res.cheats * 2 + shell_cheats();
		if (shell_mode() == SH_PLAY) res.mode_play++;
		dg = (dg ^ pop2_hash()) * 1099511628211ull;
		res.frames = f + 1;
		if (r == SHELL_EXIT) break;
	}
	if (rec_path) replay_record_end(&rec);
	if (play_path) { int a; shell_input dummy; replay_frame(&p, &dummy, &a); res.verified = replay_verify(&p); replay_close(&p); }
	res.digest = dg; res.hash = pop2_hash(); res.screen = replay_screen_checksum(); res.minutes = minutes_left;
	remove_dir(dir);
	return res;
}
static result run(const pop2_settings *s, const char *rec_path, const char *play_path)
{
	int fd[2]; if (pipe(fd)) { perror("pipe"); exit(2); }
	fflush(stdout);
	pid_t pid = fork();
	if (pid == 0) { close(fd[0]); result r = session(s, rec_path, play_path); if (write(fd[1], &r, sizeof r) != sizeof r) _exit(4); _exit(0); }
	close(fd[1]); result r; memset(&r, 0, sizeof r);
	if (read(fd[0], &r, sizeof r) != sizeof r) { failures++; printf("FAIL: a session crashed\n"); }
	close(fd[0]); int st; waitpid(pid, &st, 0);
	return r;
}
/* the shell options: from "yippeeyahoo LEVEL4" (or the title with words NULL), Alt+N at a frame; the state 3 ticks
 * into level 5, the frames spent in scenes, the demo and playing */
typedef struct opt_result { uint64_t hash5; int scene, demo, play, menu, play_after, level, minutes, sword; } opt_result;
extern uint8_t byte_5cba; extern uint32_t tick;
static opt_result opt_session(const pop2_settings *s, const char *word, int alt_n_at, int frames)
{
	opt_result o = {0}; char dir[256]; scratch_dir(dir, sizeof dir); snprintf(file_dir, sizeof file_dir, "%s", dir);
	const char *w[2] = { "yippeeyahoo", word }; pop2_settings_game = s; shell_set_seed(0x5EED);
	if (!shell_init(game_dir, word ? 2 : 0, w)) exit(3);
	if (cp_answered) word_0366 = 1;
	static shell_input in; uint32_t t0 = 0;
	for (int f = 0; f < frames; f++) {
		memset(&in, 0, sizeof in);
		if (f == alt_n_at) { in.typed[0] = 0x3100; in.ntyped = 1; }
		if (shell_step(&in) == SHELL_EXIT) break;
		int m = shell_mode(); o.scene += m == SH_SCENE; o.demo += m == SH_DEMO; o.play += m == SH_PLAY; o.menu += m == SH_MENU;
		if (alt_n_at >= 0 && f > alt_n_at) o.play_after += m == SH_PLAY;
		if (!t0 && pop2_level() == 5 && m == SH_PLAY) t0 = tick;
		if (t0 && !o.hash5 && tick == t0 + 3) o.hash5 = pop2_hash();
	}
	o.level = pop2_level(); o.minutes = minutes_left; o.sword = byte_5cba;
	remove_dir(dir);
	return o;
}
static opt_result opt_run(const pop2_settings *s, const char *word, int alt_n_at, int frames)
{
	int fd[2]; if (pipe(fd)) { perror("pipe"); exit(2); }
	fflush(stdout);
	pid_t pid = fork();
	if (pid == 0) { close(fd[0]); opt_result r = opt_session(s, word, alt_n_at, frames); if (write(fd[1], &r, sizeof r) != sizeof r) _exit(4); _exit(0); }
	close(fd[1]); opt_result r; memset(&r, 0, sizeof r);
	if (read(fd[0], &r, sizeof r) != sizeof r) { failures++; printf("FAIL: a session crashed\n"); }
	close(fd[0]); int st; waitpid(pid, &st, 0);
	return r;
}

int main(int argc, char **argv)
{
	if (argc < 3) { fprintf(stderr, "usage: settingstest GAME_DIR SDLPoP2.ini\n"); return 2; }
	game_dir = argv[1];
	/* 1. the ini and the defaults */
	pop2_settings d, s; settings_defaults(&d); settings_defaults(&s);
	nwarn = 0;
	CHECK(settings_load(&s, argv[2], count_warn), "%s read", argv[2]);
	CHECK(nwarn == 0, "%s: %d warnings", argv[2], nwarn);
	CHECK(!memcmp(&s, &d, sizeof s), "%s gives the built-in defaults", argv[2]);
	char p[600]; snprintf(p, sizeof p, "%s/PRINCE.EXE", game_dir);
	FILE *f = fopen(p, "rb"); uint8_t ds[0x27BF];
	CHECK(f && !fseek(f, 0x3CE40, SEEK_SET) && fread(ds, 1, sizeof ds, f) == sizeof ds, "PRINCE.EXE's data segment");
	if (f) fclose(f);
	const uint16_t *tabs[5] = { d.strikeprob, d.restrikeprob, d.blockprob, d.impblockprob, d.advprob };
	int same = 1;
	for (int t = 0; t < 5; t++) for (int k = 0; k < SETTINGS_SKILLS; k++) { const uint8_t *w = ds + 0x1BB6 + (t * 12 + k) * 2; same &= tabs[t][k] == (w[0] | w[1] << 8); }
	for (int k = 0; k < SETTINGS_SKILLS; k++) { const uint8_t *w = ds + 0x13D0 + k * 2; same &= d.refractimer[k] == (w[0] | w[1] << 8); }
	CHECK(same, "the [Skill N] defaults are PRINCE.EXE's DS:1BB6 / DS:13D0 tables");

	/* 2. the parser */
	pop2_settings t = d; nwarn = 0;
	settings_parse_text(&t, "[General]\nbogus = 1\nvolume = 99\nvolume = 7 ; comment\n[Nope]\nx = 1\n[Level 6]\nsword_type = 2\n[Skill 3]\nstrikeprob = 9\n"
	                        "[CustomGameplay]\nstart_minutes_left = 10\nstart_minutes_left = default\nbase_speed = 3\n[AdditionalFeatures]\nrandom_seed = 42\n", "text", count_warn);
	CHECK(nwarn == 4, "an unknown key, a bad value, an unknown section and its key warn (%d warnings)", nwarn);
	CHECK(t.volume == 7 && t.sword_type[6] == 2 && t.strikeprob[3] == 9 && t.start_minutes_left == 75 && t.base_speed == 3 && !t.random_seed_clock && t.random_seed == 42, "values and default");
	CHECK(!settings_gameplay_equal(&t, &d), "gameplay settings compare");

	/* 3. no settings / defaults / recording: the same program */
	char rec1[64], rec2[64]; snprintf(rec1, sizeof rec1, "/tmp/sdlpop2-settingstest-%d-1.p2r", (int)getpid()); snprintf(rec2, sizeof rec2, "/tmp/sdlpop2-settingstest-%d-2.p2r", (int)getpid());
	result r0 = run(NULL, NULL, NULL), r1 = run(&d, NULL, NULL), r2 = run(&d, rec1, NULL);
	printf("session: %d frames (%d playing), final hash %016llx, minutes left %d\n", r0.frames, r0.mode_play, (unsigned long long)r0.hash, r0.minutes);
	CHECK(r0.frames == FRAMES && r0.mode_play > FRAMES / 2, "the session reaches the game");
	CHECK(r0.saves == 2 && r0.loads == 2, "two quick saves and two quick loads done (%d, %d)", r0.saves, r0.loads);
	CHECK(r0.digest == r1.digest && r0.hash == r1.hash && r0.screen == r1.screen, "default settings = no settings, every frame");
	CHECK(r2.digest == r1.digest && r2.screen == r1.screen, "recording changes nothing");
	/* 4. playback */
	result r3 = run(NULL, NULL, rec1);
	CHECK(r3.verified && r3.digest == r2.digest && r3.hash == r2.hash && r3.screen == r2.screen, "the replay reproduces the recording (%d frames)", r3.frames);
	pop2_settings c = d; c.start_minutes_left = 10; c.ticks_per_minute = 100; c.base_speed = 4; c.strikeprob[1] = 255;
	result r4 = run(&c, rec2, NULL), r5 = run(NULL, NULL, rec2);
	CHECK(r4.digest != r1.digest && r4.minutes == 10, "custom gameplay settings change the game (minutes left %d)", r4.minutes);
	CHECK(r5.verified && r5.digest == r4.digest && r5.screen == r4.screen, "a replay carries its gameplay settings");
	/* the cheats turned on and off during play (no cheat word): recorded, replayed */
	cheat_script = 1;
	result r6 = run(&d, rec2, NULL), r7 = run(NULL, NULL, rec2);
	cheat_script = 0;
	CHECK(r6.cheats == 2 && r6.digest != r1.digest, "the cheats toggled during play: on, then off (%d), the game differs", r6.cheats);
	CHECK(r7.verified && r7.digest == r6.digest && r7.screen == r6.screen && r7.cheats == 2, "a replay reproduces the cheat toggles");
	unlink(rec1); unlink(rec2);
	/* 5. no settings / defaults on other levels (the guards' tables, the sword types, the copy protection answered) */
	cp_answered = 1;
	static const char *const lv[] = { "LEVEL3", "LEVEL4", "LEVEL6", "LEVEL7", "LEVEL8", "LEVEL9", "LEVEL12", "LEVEL14" };
	for (int i = 0; i < (int)(sizeof lv / sizeof lv[0]); i++) {
		level_word = lv[i];
		result a = run(NULL, NULL, NULL), b = run(&d, NULL, NULL);
		CHECK(a.digest == b.digest && a.screen == b.screen && a.mode_play > FRAMES / 2, "%s: default settings = no settings, every frame (%d playing)", lv[i], a.mode_play);
	}
	level_word = "LEVEL7";
	result a = run(&d, rec1, NULL), b = run(NULL, NULL, rec1);
	CHECK(b.verified && a.digest == b.digest, "LEVEL7: the replay reproduces the recording");
	unlink(rec1);
	/* 6. the shell's options */
	pop2_settings o = d;
	opt_result s0 = opt_run(&d, "LEVEL4", 800, 3000);
	o.enable_story_scenes = 0; opt_result s1 = opt_run(&o, "LEVEL4", 800, 3000);
	CHECK(s0.scene > 0 && s0.hash5 && s1.scene == 0 && s1.hash5 == s0.hash5 && s1.level == 5, "enable_story_scenes = false: no scene, the same state after it (%d scene frames)", s0.scene);
	/* the copy protection cannot be skipped: a game reaching level 3 or later waits at the question */
	cp_answered = 0;
	opt_result c1 = opt_run(&d, "LEVEL3", -1, 1500);
	CHECK(c1.play == 0 && c1.menu > 1000, "LEVEL3 (cheat): the copy protection is asked before the level (%d menu frames)", c1.menu);
	opt_result c2 = opt_run(&d, "LEVEL9", -1, 1500);
	CHECK(c2.play == 0 && c2.menu > 1000, "LEVEL9 (cheat): the copy protection is asked before the level");
	o = d; o.skip_title = 1; o.first_level = 5; opt_result c3 = opt_run(&o, NULL, -1, 1500);
	CHECK(c3.play == 0 && c3.menu > 1000, "first_level = 5: the copy protection is asked before the level");
	opt_result c4 = opt_run(&d, "LEVEL2", 800, 2000);
	CHECK(c4.play > 300 && c4.play_after < 30 && c4.menu > 900, "LEVEL2, Alt+N to level 3: the copy protection is asked (%d play frames after the skip)", c4.play_after);
	printf("copy protection: LEVEL3 %d/%d, LEVEL9 %d/%d, first_level 5 %d/%d, LEVEL2 + Alt+N %d/%d (menu / play frames)\n", c1.menu, c1.play, c2.menu, c2.play, c3.menu, c3.play, c4.menu, c4.play);
	cp_answered = 1;
	o = d; o.enable_intro = 0; opt_result i1 = opt_run(&o, NULL, -1, 1500);
	CHECK(i1.scene == 0 && i1.demo > 1000, "enable_intro = false: the title goes to the demo");
	o = d; o.skip_title = 1; o.first_level = 2; o.start_minutes_left = 33; o.sword_type[2] = 2;
	opt_result t1 = opt_run(&o, NULL, -1, 600);
	CHECK(t1.play > 400 && t1.level == 2 && t1.minutes == 33 && t1.sword == 2, "skip_title, first_level = 2, start_minutes_left = 33, [Level 2] sword_type = 2");
	printf("%s: %d failure(s)\n", failures ? "FAILED" : "passed", failures);
	return failures ? 1 : 0;
}
