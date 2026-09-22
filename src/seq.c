/* play_seq (DOS 0AFF:03AA): advance Char through its current sequence until a frame is produced.
 * Reconstructed from the disassembly; jump table at cs:03F4 indexed by opcode + 0x18. */
#include "types.h"
#include "globals.h"

/* 0AFF:0376 */
int16_t char_dx_forward(int16_t dx) { return (Char.direction ? -dx : dx) + Char.x; }
/* 0AFF:07B0 */
void char_y_to_floor(void) { Char.y = 63 * Char.curr_row + 56; if (Char.charid == 7 || Char.charid == 8) Char.y -= 19; }
/* 0AFF:0794 */
void seq_set_85f8(uint16_t v) { if (Char.charid == 0 || Char.charid == 1) byte_5cb8 = (uint8_t)v; }
/* 0AFF:0716: n == 0 -> only the charid-0 flag; n == 1 -> alternating footstep 0x17/0x18 unless level byte 43FD == 1,
 * plus a room-dependent x nudge for charid 4; 2..0x111 -> play sound n (0x10F through a different entry) */
void seq_sound(uint16_t n)
{
	if (n == 1) {
		counter_27d6++;
		if (level_kind != 1) {
			play_sound(counter_27d6 % 2 + 0x17);
			if (Char.charid == 4 && ovl_366c_11f8(Char.room)) Char.x = char_dx_forward(Char.index - 1);
		}
	} else if (n > 1) {
		if (n > 0x111) return;
		if (n == 0x10F) sound_1611_01a8(0x10F); else play_sound(n);
		return;
	}
	if (Char.charid == 0) word_6140 = 1;
}

/* DS:5AC2/5AC3 clamps */
#define FALL_X_MAX 16
#define FALL_Y_MAX 32

static uint16_t seq_fetch_word(void)              /* 0AFF:06D4 */
{
	const uint16_t *seq = get_seq_words(Char.seq_id);  /* lock_resource(get_seq_resource()) */
	return seq[Char.seq_pos++];
}

void play_seq(void)
{
	uint16_t start_pos = Char.seq_pos;               /* [bp-8]  */
	int need_frame = 1;                              /* [bp-0xA] */
	if (!get_seq_resource(Char.seq_id)) return;      /* 0AFF:0704 -> get_resource(id,'SQES') == 0 */
	while (need_frame) {
		uint16_t item = seq_fetch_word();
		if (item < 0xFFE8) {                         /* a frame number */
			Char.frame = (uint8_t)item;
			need_frame = 0;
			continue;
		}
		switch (item) {
		case SEQ_FLASH: {                            /* 0430 */
			uint16_t a = seq_fetch_word();
			if (a) flash_on(seq_fetch_word()); else flash_off();
			break; }
		case SEQ_HOLD:                               /* 0452: stay on the item we started at */
			Char.seq_pos = start_pos ? start_pos - 1 : 0;
			break;
		case SEQ_JMP_IF: {                           /* 0460 */
			uint16_t cond = seq_fetch_word(), yes = seq_fetch_word(), no = seq_fetch_word();
			seq_jump_to(seq_condition(cond) ? yes : no); /* 2751:008C, 2FDF:000E */
			break; }
		case SEQ_SET_F24:                            /* 0496 */
			Char.f24 = seq_fetch_word();
			break;
		case SEQ_OP_EC:                              /* 0424 (table entry FFEC): frame = opcode value, then stop */
			Char.frame = (uint8_t)item;
			need_frame = 0;
			break;
		case SEQ_Y_TO_FLOOR:                         /* 04A2 -> 07B0 */
			char_y_to_floor();
			break;
		case SEQ_CLR_F19:                            /* 04AA */
			Char.f19 = 0;
			break;
		case SEQ_CLEAR_CHAR:                         /* 04B4 -> 1BA2, then the OP_EC path */
			clear_char();
			Char.frame = (uint8_t)item;
			need_frame = 0;
			break;
		case SEQ_LVL6_COUNTER:                       /* 04BC */
			counter_5cec++;
			if (level_number == 6) flag_5cb9 = (Char.curr_col < 6) ? 1 : 2;
			break;
		case SEQ_SND:                                /* 04DE -> 0716 */
			seq_sound(seq_fetch_word());
			break;
		case SEQ_CTL: {                              /* 04EC */
			uint16_t n = seq_fetch_word();
			if (n == 1 || (n == 2 && word_27c0 == 6)) {
				seq_ctl_1954();
				if (Char.seq_pos == 0) { seq_reload_current(); }   /* unlock + reload the same sequence */
			} else if (n == 3) {
				ovl_366c_1704();
			}
			break; }
		case SEQ_KNOCK_UP:   knock = -1; break;      /* 053A */
		case SEQ_KNOCK_DOWN: knock = 1;  break;      /* 0544 */
		case SEQ_SET_85F8:                           /* 054E -> 0794 */
			seq_set_85f8(seq_fetch_word());
			break;
		case SEQ_JMP_IF_FEATHER:                     /* 055C */
			if (is_feather_fall) goto do_jmp;
			(void)seq_fetch_word();
			break;
		case SEQ_ADD_FALL: {                         /* 0570 */
			int16_t dx = seq_fetch_word(), dy = seq_fetch_word();
			if (dx && Char.fall_x < FALL_X_MAX && (Char.fall_y == 0 || Char.fall_y < FALL_Y_MAX)) {
				int v = Char.fall_x + dx; Char.fall_x = v > FALL_X_MAX ? FALL_X_MAX : v;
			}
			if (dy && Char.fall_y < FALL_Y_MAX) {
				int v = Char.fall_y + dy; Char.fall_y = v > FALL_Y_MAX ? FALL_Y_MAX : v;
			}
			break; }
		case SEQ_SET_FALL: {                         /* 05D4: 10000 = keep */
			uint16_t x = seq_fetch_word(); if (x != 10000) Char.fall_x = (int8_t)x;
			uint16_t y = seq_fetch_word(); if (y != 10000) Char.fall_y = (int8_t)y;
			break; }
		case SEQ_ACT:  Char.action = (uint8_t)seq_fetch_word(); break;        /* 05F8 */
		case SEQ_DY:   Char.y += (int16_t)seq_fetch_word(); break;            /* 0604 */
		case SEQ_DX:   Char.x = char_dx_forward((int16_t)seq_fetch_word()); break; /* 0612 -> 0376 */
		case SEQ_DOWN: Char.curr_row++; break;                                /* 0624 */
		case SEQ_UP:   Char.curr_row--; break;                                /* 062C */
		case SEQ_FLIP:                                                        /* 0634 */
			if (Char.charid == 10) Char.x = char_dx_forward(25);
			Char.direction = ~Char.direction;
			break;
		case SEQ_JMP: {                                                       /* 064C */
			uint16_t target;
		do_jmp:
			target = seq_fetch_word();
			/* level 5 special: in rooms 10/7/12 with the character above row 0, sequence 0x39 becomes 0x56 */
			if (Char.curr_row != 0 && level_number == 5 && (Char.room == 10 || Char.room == 7 || Char.room == 12)
			    && target == 0x39 && Char.f19 != 0x3C && Char.curr_col == 9) target = 0x56;
			Char.seq_id = target;
			Char.seq_pos = 0;
			if (!get_seq_resource(Char.seq_id)) return;
			break; }
		}
	}
}
