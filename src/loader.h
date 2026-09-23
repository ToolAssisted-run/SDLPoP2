#pragma once
/* The program's files: resource files (the 194C library's resource manager as the program uses it), the TXT4 texts,
 * CONFIG.DAT and the three files the game writes (PRINCE.SAV, PRINCE.HOF, PRINCE.OPT). See docs/SHELL.md. */
#include <stdint.h>
#include <stddef.h>

/* resource files (2797:01D4 open / 2797:01B6 close / 2797:02D4; 194C:717E / 70F4). get_resource (194C:6F4C) looks
 * through the open files, the most recently opened first. Tags are given in reading order ("TXT4", "SHAP", "FONT",
 * "SHPL", "PALS", "CUST", ...; "" = the untyped table: levels 2000.., demos 25..27, copy-protection table 8000). */
int  res_open(const char *name);                 /* 0 when the file is missing ("Couldn't find resource file") */
void res_close(const char *name);
void res_close_all(void);
const uint8_t *res_get(const char *tag, uint16_t id, uint16_t *size);   /* NULL when absent; the data after the checksum byte */
const uint8_t *res_get_in(const char *file, const char *tag, uint16_t id, uint16_t *size);

/* TXT4 texts (2751:0050 -> 194C:85BA / 8652): 4-bit codes into the character table DS:2500, escape to 5..8 bits.
 * Returns the decoded bytes (cached; NUL-terminated past *len). A text list is [count] then count C strings. */
const char *txt4_get(uint16_t id, uint16_t *len);
const char *txt4_string(uint16_t id, int n);     /* 194C:8580: the n-th (1-based) string of list `id`, or NULL */
int txt4_decode(const uint8_t *res, size_t size, char *out, size_t cap);   /* -> decoded length, -1 on error */

/* CONFIG.DAT (written by SETUP; read by 194C:2D44 at 2D3E:01FC; the struct DS:1FB8 points to). 16 words. */
typedef struct pop2_config {
	int16_t w[16];      /* +2 input device (0 keyboard, 2 joystick), +6 digital sound card (-1 none), +8 MIDI card type
	                       (-1 none; 0x20 / 0x29 select other music files), others: ports, IRQ, DMA (-2 = default) */
} pop2_config;
int config_load(pop2_config *c);                 /* 0 when CONFIG.DAT is missing ("Could not find the system configuration file") */

/* the files the game writes, in the game directory (DS:0378 PRINCE.SAV, DS:0384 / 03D1 PRINCE.HOF, DS:0390 PRINCE.OPT).
 * file_read returns the bytes read (-1 when missing); file_write replaces the file; file_write_at writes at an offset,
 * extending the file (C runtime open/lseek/write, 2812:1C26 / 10B6 / 1C45). The platform may override them (weak). */
extern char file_dir[400];         /* where they are read and written ("": the game directory) */
long file_size(const char *name);
long file_read(const char *name, long off, void *buf, long n);
int  file_write_at(const char *name, long off, const void *buf, long n, int create);   /* 0 on failure */
int  file_create(const char *name, const void *buf, long n);   /* a new file with these bytes */
