/* The game state: every variable the logic keeps between frames, for savestates (state_save/state_load) and for
 * the oracle snapshots (snap_fields: the ones mirroring the DOS data segment, by DS address). */
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "globals.h"
#include "state.h"
extern int16_t image_height, image_width;
/* one table drives load, store and diff: DS offset, size, address of the C global */
extern uint16_t word_5cd8, word_6142, word_6146;
const state_field snap_fields[] = {
	{"level", 0x2BB8, sizeof(level_type), &level}, {"tiles0", 0x2B9A, 30, tiles0}, {"amb_state", 0x2B98, 2, amb_state}, {"coll", 0x2B24, 0x44, &coll},
	{"Char", 0x5AB6, 64, &Char}, {"Opp", 0x5AF6, 64, &Opp}, {"Kid", 0x5B36, 64, &Kid}, {"chars", 0x5B76, 320, chars},
	{"word_8604", 0x5CC4, 2, &word_8604}, {"cur_frame", 0x5CC6, 7, &cur_frame}, {"control_x", 0x5CD4, 1, &control_x}, {"control_y", 0x5CD5, 1, &control_y}, {"control_shift", 0x5CD6, 1, &control_shift},
	{"word_5cd8", 0x5CD8, 2, &word_5cd8}, {"drawn_room", 0x5CDE, 1, &drawn_room},
	{"room_L", 0x5CDF, 1, &room_L}, {"room_R", 0x5CE0, 1, &room_R}, {"room_A", 0x5CE1, 1, &room_A}, {"room_B", 0x5CE2, 1, &room_B},
	{"room_AL", 0x5CE3, 1, &room_AL}, {"room_AR", 0x5CE4, 1, &room_AR}, {"room_BL", 0x5CE5, 1, &room_BL}, {"room_BR", 0x5CE6, 1, &room_BR}, {"byte_5ce7", 0x5CE7, 1, &byte_5ce7},
	{"counter_5cec", 0x5CEC, 2, &counter_5cec}, {"tick", 0x5D04, 4, &tick}, {"pal_slots", 0x5D08, 2, pal_slots},
	{"obj_x", 0x60FC, 2, &obj_x}, {"obj_y", 0x60FE, 2, &obj_y}, {"obj_id", 0x6100, 2, &obj_id}, {"obj_chtab", 0x6102, 1, &obj_chtab},
	{"image_height", 0x6112, 2, &image_height}, {"image_width", 0x6114, 2, &image_width}, {"char_x_left", 0x6116, 2, &char_x_left}, {"char_x_right", 0x6118, 2, &char_x_right},
	{"char_x_left_coll", 0x611A, 2, &char_x_left_coll}, {"char_x_right_coll", 0x611C, 2, &char_x_right_coll}, {"char_top_y", 0x611E, 2, &char_top_y},
	{"ctrl1_forward", 0x6122, 1, &ctrl1_forward}, {"ctrl1_backward", 0x6123, 1, &ctrl1_backward}, {"ctrl1_up", 0x6124, 1, &ctrl1_up}, {"ctrl1_down", 0x6125, 1, &ctrl1_down}, {"ctrl1_shift", 0x6126, 1, &ctrl1_shift},
	{"curr_tile", 0x612E, 1, &curr_tile}, {"curr_room", 0x6132, 1, &curr_room}, {"tile_col", 0x6133, 1, &tile_col}, {"tile_row", 0x6134, 1, &tile_row},
	{"char_col_left", 0x6135, 1, &char_col_left}, {"char_col_right", 0x6136, 1, &char_col_right}, {"char_top_row", 0x6137, 1, &char_top_row}, {"char_bottom_row", 0x6138, 1, &char_bottom_row},
	{"knock", 0x613E, 2, &knock}, {"word_6140", 0x6140, 2, &word_6140}, {"word_6142", 0x6142, 2, &word_6142}, {"word_8a84", 0x6144, 2, &word_8a84}, {"word_6146", 0x6146, 2, &word_6146},
	{"word_922a", 0x68EA, 2, &word_922a}, {"word_922e", 0x68EE, 2, &word_922e}, {"word_68f0", 0x68F0, 2, &word_68f0}, {"exit_dir", 0x68F2, 2, &exit_dir},
	{"byte_9276", 0x6936, 1, &byte_9276}, {"bridge_693e", 0x693E, 10, bridge_693e}, {"byte_6937", 0x6937, 1, &byte_6937}, {"word_6938", 0x6938, 2, &word_6938}, {"word_693c", 0x693C, 2, &word_693c}, {"byte_2b78", 0x2B78, 1, &byte_2b78}, {"byte_693a", 0x693A, 1, &byte_693a}, {"byte_2b68", 0x2B68, 1, &byte_2b68}, {"prev_coll_flags", 0x6948, 10, prev_coll_flags}, {"curr_row_coll_flags", 0x6952, 10, curr_row_coll_flags},
	{"next_room", 0x6B6D, 1, &next_room}, {"random_seed", 0x2B7A, 4, &random_seed}, {"word_68ec", 0x68EC, 2, &word_68ec}, {"byte_5cba", 0x5CBA, 1, &byte_5cba}, {"mob_count", 0x6186, 2, &mob_count}, {"mobs", 0x293E, 390, mobs}, {"trob_count", 0x6670, 2, &trob_count}, {"trobs", 0x6676, 80, trobs}, {"word_5ce8", 0x5CE8, 2, &word_5ce8}, {"floor_ptrs", 0x2B6C, 8, floor_ptrs}, {"kid_ctrl1_saved", 0x6128, 5, kid_ctrl1_saved}, {"word_5d38", 0x5D38, 2, &word_5d38},
	{"word_2b96", 0x2B96, 2, &word_2b96}, {"puzzle_answer", 0x2B6A, 1, &puzzle_answer}, {"puzzle_last", 0x2B6B, 1, &puzzle_last}, {"minutes_left", 0x5CD2, 2, &minutes_left}, {"word_2b90", 0x2B90, 2, &word_2b90}, {"word_2b92", 0x2B92, 2, &word_2b92}, {"word_5cee", 0x5CEE, 2, &word_5cee}, {"word_5cce", 0x5CCE, 2, &word_5cce}, {"byte_6b6c", 0x6B6C, 1, &byte_6b6c}, {"clock_ticks", 0x5CEA, 2, &clock_ticks}, {"word_5cb6", 0x5CB6, 2, &word_5cb6}, {"word_5cc0", 0x5CC0, 2, &word_5cc0}, {"start_hp", 0x6B71, 1, &start_hp}, {"word_5d36", 0x5D36, 2, &word_5d36}, {"byte_5cbb", 0x5CBB, 1, &byte_5cbb}, {"word_5cbe", 0x5CBE, 2, &word_5cbe}, {"word_5cd0", 0x5CD0, 2, &word_5cd0}, {"word_5cda", 0x5CDA, 2, &word_5cda}, {"word_5cdc", 0x5CDC, 2, &word_5cdc},
};
const int snap_nfields = sizeof snap_fields / sizeof snap_fields[0];

/* kept too, but not compared with the captures: DS variables outside the probed window, per-call scratch that
 * later calls read, and the C side's own state (the checkpoint copy, the collapsing floors) */
extern mob_type cur_mob; extern int16_t cur_mob_index; extern uint8_t curr_tilepos, anim_tile;
extern uint8_t byte_2ab4, edge_type, start_room; extern int16_t word_3bf62;
extern uint16_t word_2baa, word_927e; typedef struct snd_channel { int16_t id; uint32_t end; } snd_channel; extern snd_channel snd_ch[2]; extern uint32_t snd_time;
static int16_t room_ptr_tiles, room_ptr_attrs;   /* curr_room_tiles / attrs as offsets (DS:613C / 613A) */
static const state_field extra_fields[] = {
	{"byte_016a", 0x016A, 1, &byte_016a}, {"word_0366", 0x0366, 2, &word_0366}, {"word_087e", 0x087E, 2, &word_087e}, {"word_0880", 0x0880, 2, &word_0880},
	{"word_0996", 0x0996, 2, &word_0996}, {"word_32d8", 0x0998, 2, &word_32d8}, {"cheat_mode", 0x10C2, 2, &cheat_mode}, {"byte_14a0", 0x14A0, 1, &byte_14a0}, {"byte_0670", 0x0670, 1, &byte_0670},
	{"byte_2b74", 0x2B74, 2, byte_2b74}, {"lever5_flag3c", 0, 2, &lever5_flag3c}, {"cheat_god", 0, 1, &cheat_god}, {"cheat_flying", 0, 1, &cheat_flying}, {"cheat_view", 0, 1, &cheat_view}, {"cheat_looking", 0, 1, &cheat_looking}, {"cheat_spirit", 0, 1, &cheat_spirit}, {"cheat_form", 0, 1, &cheat_form}, {"cheat_goto_entry", 0, 1, &cheat_goto_entry}, {"lever5_flag3e", 0, 2, &lever5_flag3e}, {"fireball_width", 0x0842, 2, &fireball_width},
	{"anim_mod", 0x5CF0, 4, &anim_mod}, {"curr_modifier", 0x612F, 2, &curr_modifier}, {"curr_tilepos", 0x6131, 1, &curr_tilepos},
	{"cur_mob", 0x6662, 13, &cur_mob}, {"cur_mob_index", 0x66C6, 2, &cur_mob_index}, {"cur_trob", 0x6672, 4, &cur_trob}, {"anim_tile", 0x6B72, 1, &anim_tile},
	{"room_ptr_tiles", 0x613C, 2, &room_ptr_tiles}, {"room_ptr_attrs", 0x613A, 2, &room_ptr_attrs},
	{"kid_sprite", 0, sizeof(kid_sprite_t), &kid_sprite}, {"room_bg", 0, 2, &room_bg}, {"level_kind", 0, 1, &level_kind}, {"level_number", 0, 1, &level_number}, {"frame_delay", 0, 2, &frame_delay}, {"last_scene", 0, sizeof(int), &last_scene},
	{"level_switch", 0, sizeof(int), &level_switch}, {"floor_objs", 0, sizeof floor_objs, floor_objs},
	{"word_27c0", 0, 2, &word_27c0}, {"counter_27d6", 0, 2, &counter_27d6}, {"flag_5cb9", 0, 1, &flag_5cb9}, {"byte_5cb8", 0, 1, &byte_5cb8},
	{"obj_xl", 0, 1, &obj_xl}, {"word_2baa", 0, 2, &word_2baa}, {"word_37e8", 0, 2, &word_37e8},
	{"word_927e", 0, 2, &word_927e}, {"word_0882", 0x0882, 2, &word_0882}, {"word_0884", 0x0884, 2, &word_0884}, {"snd_ch", 0, sizeof snd_ch, snd_ch}, {"snd_time", 0, 4, &snd_time}, {"sound_pass_late", 0, sizeof(int), &sound_pass_late}, {"byte_2ab4", 0, 1, &byte_2ab4},
	{"edge_type", 0, 1, &edge_type}, {"start_room", 0, 1, &start_room}, {"word_3bf62", 0, 2, &word_3bf62},
};
#define NX (int)(sizeof extra_fields / sizeof extra_fields[0])

size_t state_size(void)
{
	size_t n = checkpoint_state_size();
	for (int i = 0; i < snap_nfields; i++) n += snap_fields[i].size;
	for (int i = 0; i < NX; i++) n += extra_fields[i].size;
	return n;
}
void state_save(uint8_t *buf)
{
	/* (0..29 into tiles0, the dummy room 0, 30 + n into the level: two objects, not one block) */
	room_ptr_tiles = !curr_room_tiles ? -1 : curr_room_tiles >= tiles0 && curr_room_tiles < tiles0 + 30 ? (int16_t)(curr_room_tiles - tiles0) : (int16_t)(30 + (curr_room_tiles - (uint8_t *)&level));
	room_ptr_attrs = curr_room_attrs ? (int16_t)((uint8_t *)curr_room_attrs - (uint8_t *)level.attrs) : -1;
	for (int i = 0; i < snap_nfields; i++) { memcpy(buf, snap_fields[i].p, snap_fields[i].size); buf += snap_fields[i].size; }
	for (int i = 0; i < NX; i++) { memcpy(buf, extra_fields[i].p, extra_fields[i].size); buf += extra_fields[i].size; }
	checkpoint_state_save(buf);
}
void state_load(const uint8_t *buf)
{
	for (int i = 0; i < snap_nfields; i++) { memcpy(snap_fields[i].p, buf, snap_fields[i].size); buf += snap_fields[i].size; }
	for (int i = 0; i < NX; i++) { memcpy(extra_fields[i].p, buf, extra_fields[i].size); buf += extra_fields[i].size; }
	checkpoint_state_load(buf);
	curr_room_tiles = room_ptr_tiles < 0 ? NULL : room_ptr_tiles < 30 ? tiles0 + room_ptr_tiles : (uint8_t *)&level + (room_ptr_tiles - 30);
	curr_room_attrs = room_ptr_attrs < 0 ? NULL : (uint32_t *)((uint8_t *)level.attrs + room_ptr_attrs);
	/* (not glue_select_guard_dat: frame_table_guard, guard_frame_table's fallback for a type without a guard file, stays
	 * what play leaves it, the kid's table; set from the level loaded here, a state played on from a load could differ
	 * from the same state reached by playing) */
}
/* the variables that PRINCE.EXE initialises (DS below 0x27BF) take their values from a data-segment image */
void state_load_ds_statics(const uint8_t *ds)
{
	for (int i = 0; i < snap_nfields; i++) if (snap_fields[i].ds && snap_fields[i].ds + snap_fields[i].size <= 0x27BF) memcpy(snap_fields[i].p, ds + snap_fields[i].ds, snap_fields[i].size);
	for (int i = 0; i < NX; i++) if (extra_fields[i].ds && extra_fields[i].ds + extra_fields[i].size <= 0x27BF) memcpy(extra_fields[i].p, ds + extra_fields[i].ds, extra_fields[i].size);
}
/* FNV-1a over the saved state */
uint64_t state_hash(void)
{
	static uint8_t *buf; static size_t cap; size_t n = state_size();
	if (cap < n) { free(buf); buf = malloc(n); cap = n; }
	state_save(buf); uint64_t h = 0xcbf29ce484222325ull;
	for (size_t i = 0; i < n; i++) { h ^= buf[i]; h *= 0x100000001b3ull; }
	return h;
}

/* the DS bytes lo..lo+len-1 of every mapped field, to or from buf (load 1: buf -> fields); unmapped bytes untouched */
void state_ds_range(uint16_t lo, uint16_t len, uint8_t *buf, int load)
{
	for (int i = 0; i < snap_nfields; i++) {
		const state_field *f = &snap_fields[i];
		if (!f->ds) continue;
		int a = f->ds > lo ? f->ds : lo, b = f->ds + f->size < lo + len ? f->ds + f->size : lo + len;
		if (a >= b) continue;
		if (load) memcpy((uint8_t *)f->p + (a - f->ds), buf + (a - lo), (size_t)(b - a));
		else memcpy(buf + (a - lo), (uint8_t *)f->p + (a - f->ds), (size_t)(b - a));
	}
}
