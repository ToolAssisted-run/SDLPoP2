/* debugging helpers for the tests */
#include <stdio.h>
#include "../src/types.h"
#include "../src/globals.h"
#include "../src/glue.h"
#include <stdlib.h>
void debug_case_tiles(void)
{
	printf("   dbg: room %u row %d col %d x %d dir %d | ahead1 %u infront %u atchar %u above %u | dist %d dx %d flags %02X\n", Char.room, Char.curr_row, Char.curr_col, Char.x, Char.direction,
	       get_tile_infrontof(1), get_tile_behind_char(), get_tile_at_char(), get_tile_above_char(), distance_to_edge_weight(), cur_frame.dx, cur_frame.flags);
}
void load_fram_det_col_nocol(void) { load_frame(); }   /* the game ran load_fram_det_col before control(); curr_col is already in the captured record */
void debug_opp(void) { printf("   misc: word_8604 %u drawn_room %u | ", word_8604, drawn_room); printf("   opp: charid %u room %u row %d x %d dir %d f12 %u f23 %u | char opp_index %u f12 %u f14 %d dist %d\n", Opp.charid, Opp.room, Opp.curr_row, Opp.x, Opp.direction, Opp.f12, Opp.f23, Char.opp_index, Char.f12, Char.hp_delta, opp_distance()); }

