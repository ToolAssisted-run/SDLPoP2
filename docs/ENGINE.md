# Engine structure (as identified so far; DOS addresses are runtime CS:IP, DS = 3B25)

- int8_handler (194C:7EE7): PIT splitter; four countdown timers at DS:24DC; 32-bit BIOS-style tick
  DS:4E24; chains to the previous vector.
- Main state machine 1286:03B6 -> game_tick_step (169B:0A30) once per 12 Hz tick:
  - tick DS:5D04++ ; tick_logic (0FB3:12F4) = objects_update (1375:1FBA), tick_029a (0993:029A),
    tick_objects_05ca, draw_room (0FB3:1308); then tick_13c2; pending callbacks (0FB3:219D).
  - play_all_chars (169B:07EC): for each character i in room_table[drawn_room].nchars:
    load_char(i); if dead (direction 0x56) / out of level / y > 254: char_fell_out;
    else load_char_and_opp(i) (Opp = saved copy), load_fram_det_col, char_control_step
    (0AFF:1258: control_by_charid 1611:0068 for charids 2..11, OVL01 kid_control 2FDF:048C for
    the others), play_seq, then if x within 0x22..0x221: fall_accel, fall_speed, load_frame_to_obj,
    load_fram_det_col, collision/level specials; save_char.
- play_seq (0AFF:03AA), fall_accel/fall_speed (0AFF:08E4/091C) match PoP1's logic.
- Character records: Char DS:5AB6, Opp DS:5AF6, Char_saved DS:5B36, chars[5] DS:5B76 (64 bytes).
- Level: DS:2BB8 (see LEVEL.md). Input: control_x/y/shift DS:5CD4/5/6 from key_states DS:1D0D.
- Resources: get_resource(id, 4CC tag) 194C:6F4C, lock_resource 194C:15B2; level files read directly.
- RTLink overlays: 15 code overlays (OVL01 = gameplay: kid control, room entry; OVL00 = intro/menus).

## Kid control (OVL01, 2FDF:048C = control)
play_kid_frame (169B:0692): Char = Kid; play_kid_control (0AFF:10E8) -> control_kid (0AFF:11F8):
load_ctrl1_saved, read_input (0823:10A0 -> read_keyb_control), update_ctrl1_edges (0AFF:13B8:
ctrl1_* at DS:6122..6126 become -1 on a new press, 0 on release, 1 once consumed), control_dispatch
(0AFF:12CA: flip x to forward/backward for a left-facing character, flip y when upside_down, call
control(), flip back), save_ctrl1. control() is PoP1's frame-range dispatch: 15 or 50..52 standing,
45..49 turning, 1..3 start_run, 67..69 jumpup, <15 running, 87..99 hanging, 109 crouched,
0xF6..0x107 with-sword, 0xD9..0xE2, dead frames, ... Sequences start through seqtbl_offset_char
(2FDF:000E, also sets Char+0x19 = seq id). After control: play_seq, fall_accel, fall_speed,
load_frame_to_obj, and the OVL01 3212 checks (collisions/press), then Kid = Char.
