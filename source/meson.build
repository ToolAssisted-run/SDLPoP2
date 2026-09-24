# The reconstructed game as one library. It is linked whole: several routines are weak defaults that other objects
# (the shell, the renderer, a test) replace, exactly as when every source is compiled into the program.
sdlpop2Sources = files(
  'anim.c', 'audio.c', 'audio_opl3.c', 'beast.c', 'blades.c', 'caverns.c', 'char.c', 'collision.c', 'control.c',
  'core.c', 'dat.c', 'fight.c', 'final.c', 'frame.c', 'game.c', 'glue.c', 'guard.c', 'heads.c', 'hooks.c', 'image.c',
  'input.c', 'items.c', 'kid.c', 'kidctl.c', 'kind1.c', 'kind5.c', 'level.c', 'lever5.c', 'loader.c', 'menu.c',
  'mobs.c', 'nis.c', 'render.c', 'render_desc.c', 'render_frame.c', 'render_hooks.c', 'render_kind2.c',
  'render_kind3.c', 'render_kind4.c', 'render_kind_common.c', 'render_kind_desc.c', 'render_ovl37f0.c',
  'render_palette.c', 'render_screen.c', 'render_sprites.c', 'render_tiles.c', 'room.c', 'roomhooks.c', 'ruins.c',
  'seq.c', 'shadow13.c', 'shell.c', 'skeleton.c', 'sound.c', 'spirit.c', 'state.c', 'temple.c', 'text.c', 'tick.c',
  'tiles.c', 'trap.c', 'walls.c', 'bridge5.c',
)

mathDependency = meson.get_compiler('c').find_library('m', required : false)

sdlpop2Library = static_library('sdlpop2',
  sdlpop2Sources,
  c_args : commonCompileArgs,
  dependencies : [ mathDependency ],
)

sdlpop2Dependency = declare_dependency(
  link_whole : sdlpop2Library,
  include_directories : include_directories('.'),
  dependencies : [ mathDependency ],
)
