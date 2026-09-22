/* SDLPoP2 - reconstructed types. Offsets are the DOS game's (DGROUP = DS 3B25). */
#pragma once
#include <stdint.h>

#pragma pack(push, 1)
/* 64-byte character record (Char at DS:5AB6, Opp at DS:5AF6, chars[5] at DS:5B76). */
typedef struct char_type {
	uint8_t  index;        /* +00 index into chars[] (0 = prince); save_char stores Char there */
	int8_t   direction;    /* +01 0 = right, -1 = left (FLIP does ~direction) */
	int16_t  x;            /* +02 */
	int16_t  y;            /* +04 */
	uint8_t  charid;       /* +06 10 = ?, 12 (0x0C) = prince in the level init record, 1 = shadow?, 7/8 = ? */
	uint16_t frame;        /* +07 word; PoP1 frame numbering, sword frames 0xF6..0x107 */
	int8_t   curr_col;     /* +09 */
	int8_t   curr_row;     /* +0A */
	uint8_t  action;       /* +0B */
	int8_t   fall_x;       /* +0C SET_FALL / ADD_FALL, clamped to 16 */
	int8_t   fall_y;       /* +0D clamped to 32 */
	uint8_t  room;         /* +0E CLEAR_CHAR sets 0 */
	uint8_t  f0f;          /* +0F */
	uint8_t  f10;          /* +10 set to 1 by the standing shift handler; 1 = special control (2FDF:1BFA) */
	int8_t   alive;        /* +11 < 0 while alive (PoP1 alive = -1), counts up when dying */
	uint8_t  f12;          /* +12 hp-like (take_hp compares) */
	uint8_t  f13;          /* +13 */
	int8_t   hp_delta;     /* +14 pending hp change (take_hp writes -n) */
	uint16_t seq_pos;      /* +15 word index into the current sequence */
	uint16_t seq_id;       /* +17 SQES resource id */
	uint16_t f19;          /* +19 cleared by opcode FFEE; compared with 0x3C in JMP special case */
	int16_t  bbox_top, bbox_left, bbox_bottom, bbox_right;   /* +1B..+22 sprite box from set_char_collision */
	uint8_t  f23;          /* +23 */
	uint16_t f24;          /* +24 set by opcode FFEB; 8 = ? in play_kid */
	uint16_t f26, f28, f2a;   /* +26..+2B cleared on room entry */
	uint8_t  f2c[8];        /* +2C..+33 */
	uint16_t pal_slot;      /* +34 palette/sprite slot: 2 prince, 4 << n for the guard palette slots DS:5D08/5D09 */
	uint8_t  f36[2];        /* +36..+37 */
	uint8_t  f38;          /* +38 from the room record +0x0D (guard skill / state) */
	uint8_t  opp_index;    /* +39 index of the tracked opponent in chars[], 0xFF none; room record +0x0E */
	uint8_t  f3a;          /* +3A cleared on room entry */
	uint8_t  f3b[5];       /* +3B..+3F */
} char_type;

/* Level resource (PRINCE.DAT untyped ids 2000..2033, 12024 bytes), loaded at DS:2BB8. */
typedef struct level_char_init {   /* room record: level+0x1867 + (room-1)*0x74 + 1 + i*23 (DS:43AC + room*0x74 + i*23) */
	int8_t   tilepos;      /* +00 row*10 + col at level load; 30 = outside the room grid; runtime saves write row*10 only */
	int16_t  x;            /* +01 */
	int8_t   direction;    /* +03 */
	uint8_t  f04;
	uint16_t seq_id;       /* +05 0 pos = start this sequence on entry (0x4D guard idle, 2 kid, 0x69/0x6A, 0x16) */
	uint16_t seq_pos;      /* +07 nonzero: resume here */
	uint8_t  pal;          /* +09 palette slot (DS:5D08/5D09) */
	uint8_t  index;        /* +0A slot in the room (kept in sync by add/remove) */
	uint8_t  f10;          /* +0B sword drawn */
	uint8_t  hp;           /* +0C */
	uint8_t  f38;          /* +0D -> Char+0x38 */
	uint8_t  opp_index;    /* +0E -> Char+0x39 */
	uint8_t  type;         /* +0F character type; charid = DS:0096[type], back via DS:00A2[charid] */
	uint8_t  max_hp;       /* +10 (0 = 3) */
	int16_t  y;            /* +11 y for type-2 guards in seq 100/0xBA; low byte doubles as a state for charids 7/8 */
	int16_t  w13;          /* +13 */
	int16_t  w15;          /* +15 y for charid 6 */
} level_char_init;         /* 23 bytes */

typedef struct level_room {
	uint8_t  nchars;
	level_char_init chars[5];
} level_room;              /* 0x74 bytes */

typedef struct level_type {
	uint8_t  tiles[28][30];        /* 0x0000  rooms 1..28 (room 0 is the 30 bytes before the struct) */
	uint32_t attrs[29][30];        /* 0x0348  room 0 included */
	uint8_t  region_10e0[0x713];   /* 0x10E0  zero in level 1 */
	uint8_t  hdr_pad[0x4D];        /* 0x17F3  level header occupies the room-0 slot */
	uint8_t  nrooms;               /* 0x1840 */
	uint8_t  hdr_pad2[6];
	uint8_t  number;               /* 0x1847 */
	uint8_t  hdr_pad3[0x18];
	uint8_t  start_room;           /* 0x1860 */
	uint8_t  start_tile;           /* 0x1861 col + 10*row */
	int8_t   start_dir;            /* 0x1862 */
	uint8_t  hdr_pad4[3];
	uint8_t  type;                 /* 0x1866 */
	level_room rooms[28];          /* 0x1867 rooms 1..28 */
	uint8_t  region_2517[0x9E1];   /* 0x2517 zero in level 1 (ends at 0x2EF8 = 12024) */
} level_type;
#pragma pack(pop)

typedef struct frame_type { uint16_t image, sword; int8_t dx, dy; uint8_t flags; } frame_type;   /* 7 bytes */

/* sequence opcodes (16-bit items; values >= 0xFFE8 are opcodes, anything else is a frame number) */
enum {
	SEQ_FLASH = 0xFFE8, SEQ_HOLD, SEQ_JMP_IF, SEQ_SET_F24, SEQ_OP_EC, SEQ_Y_TO_FLOOR, SEQ_CLR_F19, SEQ_CLEAR_CHAR,
	SEQ_LVL6_COUNTER, SEQ_SND, SEQ_CTL, SEQ_KNOCK_UP, SEQ_KNOCK_DOWN, SEQ_SET_85F8, SEQ_JMP_IF_FEATHER, SEQ_ADD_FALL,
	SEQ_SET_FALL, SEQ_ACT, SEQ_DY, SEQ_DX, SEQ_DOWN, SEQ_UP, SEQ_FLIP, SEQ_JMP
};
