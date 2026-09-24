#pragma once
/* Text and the little of the 194C graphics library the shell's screens use: ports (QuickDraw-like: a bitmap, a clip
 * rectangle, a pen, colours and a font), rectangles, fonts (FONT resources) and strings, 4-bit shape-set images, the
 * VGA palette, and the status-line messages (0FB3:204C / 20A4 / 2104 / 2136). Everything draws into 320x200 bitmaps
 * of palette indices; the screen port's bitmap is render.h's screen_buf (what the frontend shows), its palette
 * render_palette. See docs/SHELL.md. */
#include <stdint.h>

typedef struct qrect { int16_t top, left, bottom, right; } qrect;   /* QuickDraw order; bottom/right exclusive */

typedef struct gport {           /* 0x2C bytes in the original (template DS:247E, 194C:4FC2) */
	uint8_t *bits;               /* +00: row 0 of the bitmap, SCREEN_W bytes a row */
	int16_t origin_v, origin_h;  /* +04/+06: the bitmap's top-left in port coordinates (portRect top/left) */
	qrect clip;                  /* +18: top, left, bottom, right */
	int16_t bg;                  /* +20: the erase colour */
	int16_t pen_v, pen_h;        /* +22/+24: the pen (text baseline) */
	int16_t mode;                /* +26: transfer mode (0 copy) */
	int16_t fg;                  /* +28: the colour of text and frames */
	uint16_t font;               /* +2A: FONT resource id */
	uint8_t own;                 /* the bitmap was allocated by port_new */
} gport;

extern gport port_screen;        /* DS:5D0A: the screen (screen_buf) */
extern gport *the_port;          /* DS:2450: the current port */
extern gport *port_back;         /* DS:5CC2: the offscreen port (NULL when none) */
gport *port_new(const qrect *r); /* 2699:000E -> 194C:3890/4FC2: an offscreen port for r (colour 0, pen 0, fg 0xF) */
void   port_free(gport *p);      /* 2699:00D8 */

/* rectangles */
extern const qrect rect_screen;  /* DS:1F2A (0, 0, 200, 320) */
extern const qrect rect_game;    /* DS:097E (0, 0, 192, 320) */
extern const qrect rect_status;  /* DS:098E (193, 98, 202, 235) */
qrect rect_inset(qrect r, int dv, int dh);   /* 194C:500C: dh on the left and right, dv on top and bottom (arguments v, h) */
qrect rect_offset(qrect r, int dv, int dh);  /* 194C:50EC */

/* drawing in the_port */
void gfx_fill_rect(int color, const qrect *r);    /* 194C:6632 -> 6F00 (clipped to the port) */
void gfx_erase_rect(const qrect *r);              /* 194C:4D72: the port's bg colour */
void gfx_frame_rect(const qrect *r);              /* 194C:4D8E: 1-pixel lines in the fg colour */
void gfx_copy_bits(const gport *src, gport *dst, const qrect *sr, const qrect *dr);   /* 194C:4CB0 (mode 0, same size) */
void gfx_move_to(int v, int h);                   /* 194C:50D8 */
void gfx_move(int dv, int dh);                    /* 194C:50C0 */
void gfx_text_font(uint16_t id);                  /* 194C:52D0 (FONT resource 10..13, 100) */
void gfx_add_font(uint16_t id, const uint8_t *data);   /* (not in the game) a frontend's font under an id no FONT resource
                                                     has (the same format; the data stays the caller's) */
int  gfx_text_width(const char *s, int n);        /* 194C:53E2 */
int  gfx_draw_text(const char *s, int n);         /* 194C:4CD2: glyphs at the pen (baseline), the pen moves; -> the advance */
void gfx_draw_string(const char *s);              /* 194C:4CB4 */
void gfx_text_box(const char *s, int n, int vjust, int hjust, const qrect *r);   /* 194C:64FE: words wrapped into r;
                                                     vjust / hjust < 0 top / left, 0 centre, > 0 bottom / right */
void gfx_text_box_str(const char *s, int vjust, int hjust, const qrect *r);      /* 194C:5334 */

/* shape sets (SHPL: first SHAP id, count, 16 colours) and their images */
int  gfx_shape_set(uint16_t id, uint16_t bank_mask, int set_palette);   /* 26BC:0034: returns the bank mask in use; installs the colours */
void gfx_draw_shape(uint16_t set, int index, int v, int h, int mirror);  /* 26BC:0876 / 25A1:00FA + 2583:0006 (index 1-based) */
void gfx_shape_size(uint16_t set, int index, int *height, int *width);

/* the VGA palette (6-bit components), render_palette */
void pal_get(uint8_t *dst, int first, int n);     /* 194C:7969 */
void pal_set(const uint8_t *src, int first, int n);   /* 194C:79A3 (src NULL: black) */

/* the 0D5E string style: a shadow one pixel right, then the letters in two colours (FONT n and n+1); in r, or at pt (v, h) */
void text_shadowed(const int16_t *just, const qrect *r, const int16_t *pt, int alt, int font, const char *s);      /* 0D5E:00B4 */
void text_res_shadowed(const int16_t *just, const qrect *r, const int16_t *pt, int alt, int font, uint16_t txt);  /* 0D5E:01F0 */

/* the status line (rows 193..201 of the screen: the message area between the hit points) */
void status_message(const char *s);   /* 0FB3:204C: upper-cased, centred in DS:098E */
void status_press_key(void);          /* 0FB3:20A4: "PRESS KEY TO CONTINUE" (BUTTON with the joystick), unless restarting */
void status_erase_line(void);         /* 0FB3:2104: the whole line, 0..320 */
void status_clear(int reset);         /* 0FB3:2136: the message area (the whole line for the death message); reset:
                                         DS:5CDC = DS:5CDA = 0 */
void time_message(void);              /* the 0823:0D5A message: "N MINUTES LEFT" / "N SECONDS LEFT" / "1 SECOND LEFT" / "TIME HAS EXPIRED!" */
