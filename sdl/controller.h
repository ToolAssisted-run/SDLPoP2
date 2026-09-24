#pragma once
/* SDL game controllers for the SDL frontend ([Controller] in SDLPoP2.ini). A controller drives the game only through
 * the keys the keyboard has (shell_input_key: the PC scan codes held and the keystrokes typed), so the game logic, the
 * replays and the quicksaves see nothing new; the DOS game's own joystick path (Alt+J) is left as it is ("Joystick Not
 * Found").
 *
 * Every connected controller (SDL's game controller database; hot-plugged ones too) drives the game; their states are
 * combined. What a button gives depends on what the program is doing (shell_mode()):
 *   playing, paused   the D-pad and the left stick hold the arrows (a diagonal holds two: Up + Left = Home), the
 *                     buttons hold Up / Down / Shift / Ctrl, or type Esc (in the pause: Enter, so that it ends the pause),
 *                     Alt+A, space; quicksave / quickload / the info screen are the frontend's actions
 *   menus             arrows (one direction, repeating while held), A Enter, B / menu / back Esc, X Tab, Y types the
 *                     name "Prince" (the name fields), the other buttons space
 *   title, scenes     any button types space (demo: Enter): the game's "any key"
 * A button still held when the program changes between these is ignored until it is released. */
#include <SDL.h>
#include "../source/shell.h"
#include "../source/settings.h"

enum { CONTROLLER_QUICKSAVE = 1, CONTROLLER_QUICKLOAD = 2, CONTROLLER_INFO = 4, CONTROLLER_CLOSE = 8 };

/* the ini's button names resolved (unknown names reported on stderr), SDL's game controller subsystem started and the
 * mappings of gamecontrollerdb_file added. only_virtual: (tests) only SDL's virtual joysticks are used.
 * 0: no controllers (enable_controller = false, or SDL's subsystem could not start). */
int  controller_init(const pop2_settings *s, int only_virtual);
void controller_quit(void);
/* each SDL event: controllers connected / disconnected, and the presses (a press shorter than a frame still counts);
 * 1 when it was a controller's event */
int  controller_event(const SDL_Event *e);
/* once per video frame, after the events and before shell_step: the controllers into the frame's input. mode: the
 * shell's (enum shell_mode). feed 0 (a replay plays, the info screen shows): nothing goes to the game, the keys the
 * controllers held are released. -> CONTROLLER_* actions for the frontend (CONTROLLER_CLOSE: a button that closes the
 * info screen, only with feed 0). */
int  controller_frame(shell_input *in, int mode, int feed);
/* after shell_step: the prince's hit points (Kid +0x12); a controller rumbles when they went down while playing the
 * same level (not across a quickload: quickloaded) */
void controller_after_step(int mode, int level, int hp, int quickloaded);
int  controller_count(void);     /* controllers open */
int  controller_rumbles(void);   /* rumbles asked for so far (tests) */
