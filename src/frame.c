/* Frame tables (0AFF:02AC load_frame, 0AFF:029E determine_col). 7-byte entries: image(word), sword(word), dx, dy, flags
 * (low 5 bits = x weight). Kid-type table at the start of the data resource (3891:0000); guard-type tables are the FRAM
 * resources 1000/1200 indexed from frame 149 (offset -0x413), with per-charid frame shifts. */
#include "types.h"
#include "globals.h"

frame_type cur_frame;                 /* DS:5CC6 */
const uint8_t *frame_table_kid;       /* 3891:0000 */
const uint8_t *frame_table_guard;     /* lock_resource(DS:0CB8) */

void load_frame(void)                 /* 0AFF:02AC */
{
	int f = Char.frame; const uint8_t *e;
	switch (Char.charid) {
	case 0: case 1: case 6: e = frame_table_kid + f * 7; break;
	case 2: case 4: case 10: case 12: if (f >= 0x66 && f < 0x6B) f += 0x46; e = guard_frame_table(Char.charid) + f * 7 - 0x413; break;
	case 7: case 11: e = guard_frame_table(Char.charid) + f * 7 - 0x413; break;
	case 8: if (f < 0xB7) f += 0x2B; e = guard_frame_table(Char.charid) + f * 7 - 0x413; break;
	default: return;                  /* 3, 5, 9: unchanged */
	}
	/* a character that just left the level (opcode FFEF) can still load its opcode as a frame: the game reads past the
	 * table (16-bit offsets into other memory); here past the table reads as zeros */
	static const uint8_t none[7];
	if (Char.frame >= 0x400) e = none;
	cur_frame.image = e[0] | (e[1] << 8); cur_frame.sword = e[2] | (e[3] << 8);
	cur_frame.dx = (int8_t)e[4]; cur_frame.dy = (int8_t)e[5]; cur_frame.flags = e[6];
}
void determine_col(void)       { Char.curr_col = x_to_col(dx_weight()); }   /* 0AFF:029E */
void load_fram_det_col(void)   { load_frame(); determine_col(); }           /* 0AFF:0294 */
