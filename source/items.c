/* Potions and swords: picking them up (OVL01 2FDF:104C / 10BE, 0AFF:1548) and drinking (0AFF:1954, a sequence
 * opcode). Transcribed from the disassembly. */
#include <string.h>
#include "types.h"
#include "globals.h"
#include "settings.h"

/* DS:612E..6138: the tile lookup's results (curr_tile .. char_bottom_row) */
typedef struct { uint8_t tile; uint16_t mod; uint8_t tilepos, room; int8_t col, row, cl, cr, tr, br; } tile_ctx;
static tile_ctx ctx_save(void) { return (tile_ctx){curr_tile, curr_modifier, curr_tilepos, curr_room, tile_col, tile_row, char_col_left, char_col_right, char_top_row, char_bottom_row}; }
static void ctx_load(tile_ctx c) { curr_tile = c.tile; curr_modifier = c.mod; curr_tilepos = c.tilepos; curr_room = c.room; tile_col = c.col; tile_row = c.row; char_col_left = c.cl; char_col_right = c.cr; char_top_row = c.tr; char_bottom_row = c.br; }

/* 0AFF:1548: the item is taken (DS:27C0 = what: -1 the sword, else the potion's kind + 1); its tile becomes floor */
static void take_item(int16_t what)
{
	word_27c0 = (uint16_t)what; ctrl1_shift = 1;
	uint16_t m = level_kind == 3 ? curr_modifier & 0x3F00 : level_kind == 4 ? curr_modifier & 0x0F00 : 0;
	if (room_bg != 0) m |= 0xC000;   /* DS:01AC: the drawn room has a description */
	ROOM_TILES(curr_room)[curr_tilepos] = 1; *(uint16_t *)&ROOM_ATTRS(curr_room)[curr_tilepos] = m;
	/* 1375:0EB8 redraws it */
}
/* 2FDF:10BE: step up to the item and crouch, or (crouched) take it */
static int pick_up(void)
{
	uint8_t item = curr_tile; tile_ctx ctx = ctx_save(); uint8_t kind = curr_modifier & 3; int r = 1;
	int16_t dist = distance_to_edge_weight();
	uint8_t here = get_tile_at_char();
	if (Char.frame != 0x6D) {   /* not crouched yet */
		int16_t dx = here == item ? (Char.direction == 0 ? dist - 0x19 : dist - 0x11) : (Char.direction == 0 ? dist + 1 : dist + 0xF);
		Char.x = char_dx_forward(dx); kid_crouch_pub();
		return r;
	}
	if (Char.f19 != 0x32 && (Char.f19 == 0x14 || ctrl1_shift != -1)) return 0;
	if (Char.direction == 0) { if (dist < 0x18 && here != item) Char.x = char_dx_forward(dist - 4); }
	else Char.x = char_dx_forward(dist - 0x16);
	ctx_load(ctx);
	if (item == 0x16) {   /* the sword */
		if (Char.room == 9 && level_number == 8 && word_2bb2 == 0) {
			Char.x = char_dx_forward(-0xE); if (Char.direction == -1) Char.x = char_dx_forward(2);
			seqtbl_offset_char(0xED); sound_1611_01a8(0xFE);
		} else {
			seqtbl_offset_char(0x5B); take_item(-1);
			/* 1286:0454 loads the sword's images */
			byte_5cba = kind;
			if (!(Char.room == 9 && level_number == 8)) sound_1611_01a8(0x97);
		}
	} else if (item == 0xA) {   /* a potion */
		take_item((curr_modifier & 0xE0) >> 5);
		seqtbl_offset_char(0x4E);
	}
	return r;
}
/* 2FDF:104C (shift, standing or crouched): a potion or sword under or in front of the prince */
int try_pick_up(void)
{
	uint8_t t = get_tile_at_char();
	if (t == 0xA || t == 0x16) return pick_up();
	if (get_tile_infrontof(1) == 0xA) return pick_up();
	if (get_tile_infrontof(1) != 0x16) return 0;
	return pick_up();
}
/* 0AFF:1954 (sequence opcode 1, or 2 with a kind-6 potion): the item's effect */
void drink(void)
{
	if (word_27c0 == 0) return;
	if (Char.charid != 0 && !(Char.charid == 1 && word_27c0 == 1)) return;
	if (word_27c0 == 0xFFFF) Char.f10 = 0;   /* the sword */
	else switch (--word_27c0) {
	case 0: if (Char.f12 != Char.f13) Char.hp_delta = 1; sound_1611_01a8(0x65); break;   /* heal */
	case 1: {   /* 0823:0F16: life */
		int m = (int8_t)Char.f13 + 1, cap = GAME_SETTING(max_hitp_allowed, 12); if (m > cap) m = cap; Char.f13 = (uint8_t)m; Char.hp_delta = (int8_t)(Char.f13 - Char.f12);   /* (the cap: SDLPoP2.ini max_hitp_allowed) */
		sound_1611_01a8(0x65); break; }
	case 2: word_5d36 = 0xE4; sound_1611_01a8(0x69); word_087e = -1; break;   /* 0823:13C4: feather fall */
	case 3: toggle_upside_down_pub(); break;   /* 0823:139C */
	case 4:   /* poison, or back upright */
		if (word_5d38) { toggle_upside_down_pub(); break; }
		if (!GOD_KID && take_hp(1)) seqtbl_offset_char(0x47);   /* (god mode: harmless) */
		play_sound(0xE); break;
	case 5: play_sound(0xE); seqtbl_offset_char(0x6F); break;
	}
	word_27c0 = 0;
}
