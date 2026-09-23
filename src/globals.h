/* Globals (DS 3B25) referenced so far. Names follow SDLPoP where the meaning matches. */
#pragma once
#include "types.h"
extern char_type Char;            /* DS:5AB6 */
extern char_type Opp;             /* DS:5AF6 */
extern char_type Kid;             /* DS:5B36 - the prince's persistent record (play_kid_frame loads/saves it) */
extern char_type chars[5];        /* DS:5B76 */
extern level_type level;          /* DS:2BB8 */
extern uint8_t   tiles0[30];      /* DS:2B9A dummy room 0 */
extern uint32_t  tick;            /* DS:5D04 */
extern int16_t   knock;           /* DS:613E */
extern int16_t   is_feather_fall; /* DS:5D36 */
extern int8_t    control_x, control_y, control_shift;  /* DS:5CD4..5CD6 */
extern uint8_t   drawn_room;      /* DS:5CDE */
extern uint8_t   level_kind;      /* DS:43FD (level header): 2,4,5,6 = environment kinds */
extern uint8_t   level_number;    /* DS:43FF */
extern uint8_t   room_A;          /* DS:5CE1 (room above the drawn room) */
extern uint16_t  counter_5cec, word_27c0, counter_27d6, word_6140; extern uint8_t flag_5cb9, byte_5cb8;
void play_sound(uint16_t n); void sound_1611_01a8(uint16_t n); int ovl_366c_11f8(uint8_t room);
/* routines referenced by seq.c, to be reconstructed */
const uint16_t *get_seq_words(uint16_t seq_id);
int  get_seq_resource(uint16_t seq_id);
void seq_reload_current(void);
int  seq_condition(uint16_t cond);
void seq_jump_to(uint16_t seq_id);
void clear_char(void);            /* 0AFF:1BA2 */
void seq_ctl_1954(void);          /* 0AFF:1954 */
void ovl_366c_1704(void);
void flash_on(uint16_t v); void flash_off(void);   /* 0FB3:2A34 / 294C */
int16_t char_dx_forward(int16_t dx); void char_y_to_floor(void); void seq_sound(uint16_t n); void seq_set_85f8(uint16_t v);
void play_seq(void);              /* 0AFF:03AA */
void fall_accel(void); void fall_speed(void); void save_char(void); void load_char(int n); void loadkid(void);
void load_char_and_opp(int n); int char_out_of_level(void); void load_fram_det_col(void); void rtlink_fatal(int code);
/* tiles.c */
extern uint8_t curr_tile; extern uint16_t curr_modifier; extern uint8_t curr_tilepos, curr_room; extern int8_t tile_col, tile_row;
static const int8_t dir_front[] = {-1, 1};   /* DS:0CF8 indexed by direction+1 (-1 left, 0 right) */
static const int8_t dir_behind[] = {1, -1};  /* DS:0CFA */
#define level_links(room) (level_roomlinks + (room) * 4)   /* DS:4374 */
extern uint8_t *level_roomlinks;
uint8_t level_edge_tile(int8_t row, int8_t col);   /* 0AFF:0174 */
uint8_t get_tile(int8_t row, int8_t col, uint8_t room); uint8_t get_tile_at_char(void); uint8_t get_tile_above_char(void);
uint8_t get_tile_behind_char(void); uint8_t get_tile_infrontof(int8_t n); void get_room_address(uint8_t room); uint8_t find_room_of_tile(void);
int tile_is_empty_kind(uint8_t t); int tile_is_wall_kind(uint8_t t); int tile_is_floor(uint8_t t); int tile_is_loose_kind(uint8_t t); int tile_is_solid_floor(uint8_t t);
void seqtbl_offset_char(uint16_t seq_id); void shadow_hook_2f9a2(void);
/* control.c and its not-yet-reconstructed callees */
extern int8_t ctrl1_forward, ctrl1_backward, ctrl1_up, ctrl1_down, ctrl1_shift;   /* DS:6122..6126 */
extern frame_type cur_frame;               /* DS:5CC6 */
#define frame_dx cur_frame.dx
#define frame_flags cur_frame.flags
extern const uint8_t *frame_table_kid, *frame_table_guard; void load_frame(void); void determine_col(void);
extern uint8_t kid_f34; extern int16_t word_3bf62;
extern int8_t obj_xl;
void control(void); void control_running(void); void control_turning(void); void control_start_run(void); void control_jumpup(void);
void control_standing(void); void control_hanging(void); void control_crouched(void); void control_with_sword(void); void control_0d9_0e2(void);
void control_dead_0307a2(void); void control_frame81_0313c6(void); void control_standing_down(void); void control_standing_shift(void);
int control_runjump(int kind); int control_rest(void); void control_jump_031062(void); void control_2fdf_1bfa(void); void control_by_charid_cc1e(void);
int is_dead_frame(uint16_t frame); int8_t x_to_col(int16_t x); int16_t dx_weight(void); int16_t distance_to_edge(int16_t x); int16_t distance_to_edge_weight(void);
int can_climb_down_146e(uint16_t mod_here, uint16_t mod_front, uint8_t here, uint8_t front); int tile_passable_2f800(uint16_t mod, uint8_t tile);
int shadow_seq_2f86a(void); int sword_seq_0317c4(void); void ovl_2f86_0a5c(void); void shadow_2fba4(void); void ovl_34024(void);
void ovl_383fa(void); void ovl_35f5a(void);
extern const int16_t *col_x_left, *col_x_right;   /* DS:5CC5; column x tables DS:0D06 / 0D08 */
void ovl_35a88(void); int ovl_34350(void); int ovl_35240(int a); int ovl_34ab2(void); void control_hanging_climb(void); int seq_peek_frame_decreases(void);
extern uint16_t word_6d46, word_8a84; int control_sword_check_030e3c(void); void ovl_384e8(void); int ovl_32a0e(void); int8_t tile_col_in_drawn_room(void);
extern uint8_t byte_2ab4, edge_type, start_room;
int gate_blocks_0329b6(void); void ovl_2f86_08d8(void); int get_edge_distance(void); int level_door_open_0cfa(void); void ovl_30b52(void);
void control_jumpup_grab_031074(void); int opp_distance(void); void control_standing_turn(void); int control_standing_step(int dist);
void control_standing_forward(void); void control_standing_up(void);
extern uint16_t word_922e, word_922c, word_8604, word_927e;
void sword_retreat(void); int char_scan_31bc4(void); int ovl_377c6(void); void ovl_3741a(void); uint8_t room_nchars(uint8_t room);
void load_opp_080a(int n); int rtlink_0dd5(void);
/* collision.c (0993:09B6, OVL01 segment 3212) */
typedef struct coll_state {                 /* DS:2B24.. as laid out in the DOS data segment */
	uint8_t above_flags[10];                /* 2B24 */
	int8_t  prev_collision_row, collision_row;   /* 2B2E, 2B2F */
	uint8_t below_flags[10];                /* 2B30 */
	int8_t  left_checked_col, right_checked_col; /* 2B3A, 2B3B */
	int8_t  bump_col_left_of_wall, bump_col_right_of_wall;   /* 2B3C, 2B3D */
	uint8_t above_room[10], below_room[10], prev_room[10], curr_room[10];   /* 2B3E, 2B48, 2B52, 2B5C */
	int16_t tile_left_xpos;                 /* 2B66 */
} coll_state;
extern coll_state coll; extern uint8_t prev_coll_flags[10], curr_row_coll_flags[10];   /* DS:6948 / 6952 */
extern int16_t obj_x, obj_y, obj_id; extern uint8_t obj_chtab;   /* DS:60FC.. sprite placed by load_frame_to_obj */
extern int16_t image_height, image_width, char_x_left, char_x_right, char_x_left_coll, char_x_right_coll, char_top_y;   /* DS:6112.. */
extern int8_t char_col_left, char_col_right, char_top_row, char_bottom_row;   /* DS:6135.. */
extern uint8_t room_L, room_R, room_B, room_AL, room_AR, room_BL, room_BR;   /* DS:5CDF.. (861F..8626) */
extern int16_t word_440a;                  /* DS:440A (6d4a): level-7 moving objects */
extern const uint8_t *sword_table;         /* FRAM 1000/1200 resource: 4-byte sword frame entries */
int res_image_size(uint8_t chtab, int16_t image, int16_t *height, int16_t *width_m1);   /* 0993:0FE2 + 26BC:06B6: SHAP header words */
int8_t col_from_x18(int16_t x18); int8_t y_to_row(int16_t y);
void load_frame_to_obj(void); void set_char_collision(void); int wall_type(uint8_t t); int can_bump_into_gate(void);
void check_collisions(void); void check_bumped(void); void check_gate_push(void); int16_t sword_extra_width(void);
void ovl_366c2(void); void ovl_37bca(void); int ovl_34ce6(void); void ovl_34bd2(uint8_t *flags, uint8_t *rooms, int8_t row); int ovl_343c2(void);
int16_t ovl_34b28(int8_t row, uint8_t room, int8_t dir); int16_t ovl_352ca(void); void ovl_3211a(void);
/* kid.c (169B:0692 and the fall/land family) */
extern uint16_t word_5cd8, word_6142, word_6146; extern int16_t word_087e, word_37e8;
int take_hp(int n); void die_at_bottom(void); void char_fell_out(void); int8_t find_opponent(int8_t mode); int frame_is_strike_02f712(uint16_t frame, uint8_t charid);
uint8_t get_tile_above_front(void); uint8_t get_tile_above_behind(void);   /* 0AFF:1514 / 14F6 */
int play_kid_frame(void); int play_kid_control(void); void kid_post_move(void);
void ovl_348e6(void); void ovl_3564e(void); void ovl_34724(void); void ovl_37826(void);
void ovl_349be(void); void fall_scream_1611_0030(void); void sound_194c_83d2(uint16_t n); int sound_playing_8426(void); void level_kind_hooks(void);
/* room.c (OVL01 2D3E room records, 0823:0E72 room switch, 0FB3:0026) */
extern uint8_t next_room, pal_slots[2], byte_9276; extern int16_t exit_dir; extern uint16_t word_922a, word_32d8, word_68f0;
extern const uint8_t *type_to_charid, *charid_to_type;
level_char_init *room_char_record(int i, uint8_t room); void change_room(int dir); void check_kid_left_room(void); void apply_hp_deltas(void);
void set_neighbour_rooms(void); void enter_room_chars(void); void switch_room(void);
void ovl_37d2a(void); void ovl_352b4(void); int ovl_342b4(void); void ovl_34210(void); void ovl_34958(void); void ovl_34370(void); void ovl_2f9f2(void);
void load_guard_sprites(uint8_t type); void ovl_guard6_sprites(void); int random_2751(int n); level_char_init *ovl_379e8(level_char_init *r); level_char_init *ovl_36ada(level_char_init *r);
void ovl_36712(void); void ovl_3791e(int a, int idx); void room_music_087e(void); void redraw_room(void); void hp_bar_clear(void); void hp_bar_draw(uint8_t index, int a, uint8_t hp);
/* guard.c (2D3E:1864 autocontrol, OVL10 366C guard decisions) and play_all_chars */
extern uint32_t random_seed; extern uint16_t word_68ec; extern uint8_t byte_5cba;
void guard_set_prob_tables(const uint8_t *ds); int tile_passable_2f800(uint16_t mod, uint8_t tile); void autocontrol(void); void guard_after_seq(void);
int tile_blocks(uint8_t t); void check_guard_bumped(void); void check_gate_guard(void); void check_fall_or_ceiling(void); void check_tile_effects(void);
void char_control_step(void); void play_all_chars(void); void remove_record_pub(int i, uint8_t room);
int ovl_383d2(void); void ovl_shadow_37f0_78(void); void ovl_366c_10cc(void); void ovl_33fd_694(void); void ovl_366c_e0a(void); void ovl_366c_11(void);
int ovl_36ed6(int16_t d); void dead_char_sound_1611(void); void ovl_15db_64(void); void ovl_37d28(void); void level_kind_hooks_char(void);
#define word_2ba8 (*(uint16_t *)(tiles0 + 0xE))   /* DS:2BA8 */
/* fight.c, spawns, tick.c */
extern uint16_t word_5ce8; extern const uint16_t *refract_timer;
void check_sword_hits(void); void process_hurt(void); void guards_see_kid(void); void spawn_guards(uint8_t room); int tick_body(void);
void sword_range_pub(int16_t *far_ax, int16_t *near_bx); void land_adjust_pub(void);
int ovl_366c_6ac(void); int ovl_366c_1580(void); void ovl_366c_1166(void); void ovl_33fd_6ae(void); int ovl_366c_fc(void); void music_1286_07ce(uint8_t k); void ovl_366c_f24(void);
void falling_floors(void); void animate_tiles(void); void ovl_366c_f60(void); void checkpoints_0db4(void); void level_kind_tick(void);
/* anim.c */
extern trob_type trobs[20], cur_trob; extern uint16_t trob_count; extern uint32_t anim_mod; void add_trob(uint8_t tile, uint8_t state, int8_t tilepos, uint8_t room); void anim_tile_other(uint8_t t);
void start_room_anims(void); void anim_start_other(uint8_t t, int8_t tilepos, uint8_t room, int si);
/* mobs.c */
extern mob_type mobs[30], cur_mob; extern uint16_t mob_count; extern int16_t cur_mob_index;
void mobs_set_tables(const uint8_t *ds); trob_type *get_trob(int8_t tp, uint8_t room); void trigger_links(int link, uint8_t button);
void press_button(int link, uint8_t tile); void press_button_hold(void); void anim_button(void); void anim_gate(void); void anim_loose(void);
void loose_floor_touch(int8_t arg); void loose_floor_shake(void); void shake_loose_row(int8_t row, uint8_t room); void falling_floors(void);
int exit_door_speed(int state);
int ovl_button22(uint8_t room, int8_t tp); void ovl_347c_b3e(uint8_t room, int8_t tp, int k); int ovl_2a31_dad(uint8_t room, int8_t tp); void ovl_33fd_4d0(uint8_t room, int8_t tp);
void ovl_347c_e8e(void); void ovl_347c_126(void); void ovl_366c_1294(int8_t row, uint8_t room); void ovl_mob_other(uint8_t type); int ovl_torch_347c(int cur);
extern uint8_t *curr_room_tiles; extern uint32_t *curr_room_attrs;   /* DS:613C / 613A, set by get_room_address (17C1:0008; room 0 keeps them) */
/* trap.c */
void trap_room_entry(int8_t tp, uint8_t room); void trap_catch_check(void); void trap_touch_check(void); void trap_update(void); int ovl_347c_a0e(void); void ovl_366c_13e8(void);
mob_type *find_mob_pub(int n, uint8_t type); void add_mob_pub(void); int8_t mob_col_pub(void);
/* hooks.c */
void level_kind_hooks_char(void); void note_missing(const char *what); void ovl_347c_e48(void); void ovl_33fd_b2(void); void ovl_33fd_118(void); void ovl_kind6_char(void);
/* skeleton.c */
void skel_ai(void); void skel_collapse(void); void set_revive_timer(uint16_t v, uint8_t index); void skeleton_wake(void); level_char_init *skel_room_entry(level_char_init *r);
void skel_blade_hit(void); void skel_row_shake(int8_t row, uint8_t room); void guard_ai_pub(void); void char_dies_pub(void); void init_hp_pub(level_char_init *r); int ovl_2a31_ddf(void);
void add_mob(void); extern mob_type cur_mob; mob_type *find_mob_pub(int n, uint8_t type); int anim_visible_pub(void);
/* caverns.c */ void rock_drop(uint8_t room, int8_t tp); void anim_rock(void); void rocks_hit_char(void); void rock_fly(void);
extern uint16_t floor_ptrs[4]; extern uint8_t floor_objs[4][0x65]; void caverns_set_tables(const uint8_t *ds); void floor_room_entry(uint8_t room, int8_t tp);
void floor_free_all(void); void anim_floor(void); void floor_touch_check(void); int gate_squeeze(int si); int door_speed_0776(int st); trob_type *get_trob(int8_t tp, uint8_t room);
/* input.c */ extern uint8_t key_table[0x70], bios_shift_flags; extern int16_t joy_x, joy_y, joy_cx, joy_cy; extern uint8_t joy_button; extern uint16_t input_device;
int read_input(void); int hotkeys_02be(void);
/* kidctl.c */ extern int8_t kid_ctrl1_saved[5]; extern uint16_t word_5d38, word_5cd0, word_5cda, word_5cdc; int play_kid_control(void);
void seq_music_1611(uint8_t m); void restart_prompt(void); int death_sound_playing(int both);   /* platform: 194C:8426 on DS:0882 (and DS:0884) */
/* level.c */ extern uint8_t start_hp, byte_5cbb; extern uint16_t word_0996, word_0880, word_5cbe, word_5d36;
void checkpoint_save(int n); void checkpoint_restore(void); void checkpoint_free(void); void checkpoints_0db4(void); void level_postprocess(void); void level_begin(void);
/* game.c */ extern uint16_t minutes_left, clock_ticks, word_5cb6, word_5cc0, frame_delay; extern int8_t byte_016a; void game_clock(void); void frame_begin(void);
void chars_fell_below(void);
int tick_main(void); int tick_tail(void);
extern uint16_t word_2b90, word_2b92, word_5cee, word_5cce; extern uint8_t byte_6b6c; int frame_end(void);
int level_first_room(void); int play_frame(void); int level_end_sound_playing(void);   /* platform: 1611:02CE / 194C:8426 on DS:0882 */
void ambient_sound(void);   /* platform (sound timing): may call random_2751 */
int load_level(int n); int play_level(int n); const uint8_t *level_resource(uint16_t id, uint16_t *size); void platform_wait_frame(void);   /* platform: DAT resource, frame pacing */
int bios_key(void);   /* platform: pending keystroke (BIOS code), 0 = none */
int frame_after_tick(int r);
extern uint16_t cheat_mode; void frame_wait(void);
extern uint16_t word_0366; int story_scene(int prev, int n);
void keyboard_controls(const uint8_t *keys, uint8_t flags, int8_t *x, int8_t *y, int8_t *shift);
/* blades.c */ void blades_set_tables(const uint8_t *ds); void anim_blade(void); void blade_touch(void); void blade_hits(void); int blade_running_here(void); int boxes_overlap_pub(const int16_t *a, const int16_t *b);
/* temple.c */ int temple_torch(int cur); void slab_step(void); void slab_shake(void); void slab_mob(void); void temple_tick(void);
/* ruins.c */ void level6_entrance(void); void ruins_crumble(void); void close_entrance_pub(void); void remove_loose_pub(int8_t tp, uint8_t room);
/* beast.c */ void beast_ai(void); level_char_init *beast_room_entry(level_char_init *r); int8_t scan_to_wall_pub(int8_t dir, int8_t row, int8_t col, uint8_t room); void save_char_restore_kid_pub(void);
/* heads.c */ void heads_ai(void); int head_knock_back(void); int head_hit(void); int head_biting(int i, uint8_t room); int head_wall(int near); int nearest_foe_2d3e_a26(void);
void heads_set_tables(const uint8_t *ds); uint8_t ds_byte(uint16_t a); uint16_t ds_word(uint16_t a); int16_t wall_distance_pub(int8_t col, uint8_t room, uint8_t t);
const uint8_t *guard_frame_table(uint8_t charid);   /* platform: FRAM 750 of the chtab-3 file (the character's type: charid 10/12 by DS:00A2, others the level's) */

/* kind1.c (level 2, OVL03) */ extern int8_t puzzle_answer, puzzle_last; extern uint8_t byte_14a0; int sound_playing(uint16_t id);
void anim_tile1e(void); int tile1e_start(uint8_t room, int8_t tp, uint8_t mode); void anim_gate_kind1(void); void kind1_tick(void);
extern int level_switch; extern uint16_t word_2b96; void game_start(void);   /* level.c 169B:0006 */
extern int last_scene; extern uint8_t byte_2b68, byte_6937; void kind1_level_init(void); int room_background_id(void);
