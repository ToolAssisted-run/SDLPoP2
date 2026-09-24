# SDL2: the system's on Linux; built from the WrapDB wrap (extern/sdl2.wrap) where there is none, e.g. for Windows
sdl2Dependencies = [ dependency('sdl2', default_options : [ 'default_library=static', 'use_video_offscreen=enabled', 'test=false' ]) ]   # (offscreen: headless runs and tests)
if host_machine.system() == 'windows'
  sdl2Dependencies += dependency('sdl2main')   # (SDL's WinMain -> main)
endif

sdlpop2Frontend = executable('sdlpop2',
  'main.c', 'controller.c',
  c_args : [ commonCompileArgs,
             '-DSDLPOP2_DATADIR="' + (get_option('prefix') / get_option('datadir') / 'sdlpop2') + '"' ],
  dependencies : [ sdlpop2Dependency, sdl2Dependencies ],
  win_subsystem : 'windows',
  install : true,
)

# SDLPoP2.ini: next to the binary in the build directory (the frontend looks there), and installed to share/sdlpop2
configure_file(input : '../SDLPoP2.ini', output : 'SDLPoP2.ini', copy : true)
install_data('../SDLPoP2.ini', install_dir : get_option('datadir') / 'sdlpop2')
