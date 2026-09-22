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
