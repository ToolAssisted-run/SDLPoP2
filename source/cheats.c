/* SDLPoP2's own cheats, beside the DOS game's (shell.c cheat_keys, 0823:0528; like those they run inside the prince's
 * control, Char = the prince, and leave Kid = Char): god mode, leaving the body as the
 * shadow or the flame, the sword type, looking into the neighbouring rooms, teleporting there and flying. They come
 * through the shell's input like the game's cheat keys, so replays reproduce them, and their state is game state
 * (quicksaves keep it). Off, they change nothing: every hook tests its flag first. */
#include <stddef.h>
#include "types.h"
#include "globals.h"

uint8_t cheat_god;       /* nothing hurts or kills the prince (the hooks: GOD_KID) */
uint8_t cheat_fly_key;   /* the fly key is held (the shell sets it every frame; not state: input) */
uint8_t cheat_flying;    /* the prince flew in the last tick */
uint8_t cheat_view;      /* a room to show at the end of the tick (the look keys), 0 none */
uint8_t cheat_looking;   /* the drawn room is not the prince's: one the look keys showed */
uint8_t cheat_spirit;    /* the spirit left the body through the cheat: the falling body does not drain it */
uint8_t cheat_form;      /* the prince's spirit drawn as by the cheat: 0 the game's (the flame on level 14), 1 shadow, 2 flame */
uint8_t cheat_goto_entry;   /* the entry point the next level load starts at (cheat_goto): 0 none, else entry + 1 */

void cheats_reset(void) { cheat_flying = 0; cheat_view = 0; cheat_looking = 0; cheat_spirit = 0; cheat_form = 0; }   /* a level starts */

/* god mode on / off; out of the level with it on, the prince falls for ever: off, the game's own fall kills him */
const char *cheat_god_toggle(void)
{
	cheat_god = !cheat_god;
	return cheat_god ? "GOD MODE ON" : "GOD MODE OFF";
}

/* the prince leaves his body (as the eighth turn on the temple levels does, without its cost), as the shadow or the
 * flame on any level: the game has one spirit, drawn as a flame on level 14 (2F86:052E) and as a shadow elsewhere; the
 * form is only how it is drawn */
const char *cheat_leave_body(int flame)
{
	if (Char.charid != 0) return "ALREADY A SPIRIT";
	if (Char.alive >= 0 || Char.room == 0 || Char.room != drawn_room || cheat_flying) return NULL;
	if (Char.f10 == 1 || !(frame_table_kid[Char.frame * 7 + 6] & 0x40)) return "STAND STILL FIRST";
	if (room_nchars(Char.room) >= 5) return "NO SPACE FOR THE BODY";
	if (!spirit_leave_body()) return NULL;
	cheat_spirit = 1; cheat_form = flame ? 2 : 1;
	return flame ? "THE FLAME" : "THE SHADOW";
}

/* the sword: none, 2 (levels 7 and 8's, 5 pixels shorter), 1 (the full sword), again none */
const char *cheat_sword(void)
{
	if (Char.f10 == 1) return "PUT THE SWORD AWAY";
	if (Char.charid != 0) return "NOT AS A SPIRIT";
	byte_5cba = byte_5cba == 0xFF ? 2 : byte_5cba == 2 ? 1 : 0xFF;
	Char.f10 = byte_5cba == 0xFF ? 0xFF : 0; Kid = Char;
	return byte_5cba == 0xFF ? "NO SWORD" : byte_5cba == 2 ? "SHORT SWORD" : "FULL SWORD";
}

/* the levels' entry points for the level cheat: every level's start, its checkpoints (the level's table, level+0x26E7:
 * two (room, tile) pairs, a room of the level or none) and level 7's second start (room 4, after leaving level 6 on
 * the right). entry: 0 the start, 1 / 2 a checkpoint, 3 level 7's room-4 start; room: where it is */
int cheat_level_entries(int *levels, int *entries, int *rooms, int max)
{
	int n = 0;
	for (int lv = 1; lv <= 14 && n < max; lv++) {
		uint16_t size; const uint8_t *p = level_resource((uint16_t)(0x7CF + lv), &size);
		if (!p || size < 0x26EB) continue;
		const level_type *l = (const level_type *)p;
		levels[n] = lv; entries[n] = 0; rooms[n] = lv == 7 ? 0x20 : l->start_room; n++;
		if (lv == 7 && n < max) { levels[n] = lv; entries[n] = 3; rooms[n] = 4; n++; }
		for (int k = 0; k < 2 && n < max; k++) {
			uint8_t room = p[0x26E7 + 2 * k];
			if (room == 0 || room > l->nrooms || room > 28) continue;
			levels[n] = lv; entries[n] = k + 1; rooms[n] = room; n++;
		}
	}
	return n;
}
/* to a level and entry point: another level through the game's own level change (as Alt+N: its story scene, the copy
 * protection from level 3 on), the current one through a restart; the entry is set up when the level loads
 * (level.c cheat_goto_apply) */
void cheat_goto(int lv, int entry)
{
	if (lv < 1 || lv > 14) return;
	cheat_goto_entry = (uint8_t)(entry + 1);
	if (lv == (int8_t)word_32d8) word_5cd8 = 1;   /* (a restart: 0823:051C) */
	else counter_5cec = (uint16_t)lv;
	sound_stop_all();
}

/* look into the room left (0), right (1), above (2) or below (3) of the one shown: it is drawn from the end of the
 * tick; the prince goes on where he is, unseen (walking out of his room brings the view back to him) */
int cheat_look(int dir)
{
	uint8_t from = cheat_view ? cheat_view : drawn_room;
	if (from == 0) return 0;
	uint8_t r = level_links(from)[dir];
	if (r) cheat_view = r;
	return r;
}
