#pragma once
/* Replays: the whole program is deterministic given the random seed (DOS time() at start), the command-line words, the
 * settings the shell and the core read, the game's own files as they were at the start (PRINCE.OPT / HOF / SAV) and
 * the shell_input of every video frame (with the frontend's quick save / load requests). A replay file holds exactly
 * that: a text header (the seed, the words, the files, the gameplay settings as SDLPoP2.ini text), then the frames
 * whose input changed, and at the end the frame count, pop2_hash() and a checksum of the screen, which playing it
 * back must reproduce. No SDL here: tests/settingstest.c records and replays headlessly. */
#include <stdint.h>
#include <stdio.h>
#include "shell.h"
#include "settings.h"

enum { REPLAY_NONE = 0, REPLAY_QUICKSAVE = 1, REPLAY_QUICKLOAD = 2 };   /* the frontend's action of a frame */

typedef struct replay_rec {
	FILE *f; uint32_t frame;
	shell_input last; int have_last;
} replay_rec;
/* before shell_init's first step: the game's own files are read where loader.h's file_dir says */
int  replay_record_start(replay_rec *r, const char *path, uint32_t seed, int argc, const char **argv, const pop2_settings *s);
void replay_record_frame(replay_rec *r, const shell_input *in, int action);   /* before each shell_step */
void replay_record_end(replay_rec *r);   /* after the last step: the frame count and the hashes */

typedef struct replay_play {
	FILE *f; uint32_t frame;
	uint32_t seed; int argc; char argv[16][64]; const char *argp[16];
	pop2_settings settings;    /* the recorded gameplay settings over the defaults */
	uint32_t end_frame; uint64_t end_hash; uint32_t end_screen; int ended;
	shell_input cur;
	/* the recorded game files, to be written where the replay's files are read */
	int nfiles; struct { char name[16]; uint8_t *data; long size; } files[3];
	uint32_t next_frame; int next_type; int eof;
} replay_play;
int  replay_open(replay_play *p, const char *path, char *err, size_t errlen);   /* 0 on failure (err says why) */
int  replay_write_files(const replay_play *p, const char *dir);   /* the recorded PRINCE.OPT / HOF / SAV into dir */
/* the input of the next frame (the held keys carry over) and its action; 0 once the recording has ended (frame ==
 * end_frame): then compare replay_verify() */
int  replay_frame(replay_play *p, shell_input *in, int *action);
int  replay_verify(const replay_play *p);   /* 1: pop2_hash() and the screen are the recorded ones */
void replay_close(replay_play *p);
uint32_t replay_screen_checksum(void);
