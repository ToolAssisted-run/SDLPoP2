sdlpop2Frontend = executable('sdlpop2',
  'main.c', 'controller.c',
  c_args : [ commonCompileArgs,
             '-DSDLPOP2_DATADIR="' + (get_option('prefix') / get_option('datadir') / 'sdlpop2') + '"' ],
  dependencies : [ sdlpop2Dependency, dependency('sdl2') ],
  install : true,
)

# SDLPoP2.ini: next to the binary in the build directory (the frontend looks there), and installed to share/sdlpop2
configure_file(input : '../SDLPoP2.ini', output : 'SDLPoP2.ini', copy : true)
install_data('../SDLPoP2.ini', install_dir : get_option('datadir') / 'sdlpop2')
