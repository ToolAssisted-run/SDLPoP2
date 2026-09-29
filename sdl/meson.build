# SDL2: the system's on Linux; built from the WrapDB wrap (extern/sdl2.wrap) where there is none, e.g. for Windows
sdl2Dependencies = [ dependency('sdl2', default_options : [ 'default_library=static', 'use_video_offscreen=enabled', 'test=false' ]) ]   # (offscreen: headless runs and tests)
if host_machine.system() == 'windows'
  sdl2Dependencies += dependency('sdl2main')   # (SDL's WinMain -> main)
endif

# The Roland MT-32: Munt's libmt32emu (LGPL-2.1-or-later), built from extern/munt (a git submodule) with CMake and linked
# in statically, with the C++ runtime after it (the program needs no C++ library of the system's); else an installed
# one (pkg-config mt32emu); else none (the MT-32 settings then fall back to the FM chip)
fs = import('fs')
mt32emuDependency = dependency('', required : false)
frontendArgs = [ commonCompileArgs, '-DSDLPOP2_DATADIR="' + (get_option('prefix') / get_option('datadir') / 'sdlpop2') + '"' ]
frontendLinkDepends = []
frontendRpath = ''
if get_option('mt32').allowed() and fs.exists(meson.project_source_root() / 'extern' / 'munt' / 'mt32emu' / 'CMakeLists.txt')
  add_languages('cpp', native : false)
  cmake = import('cmake')
  muntOptions = cmake.subproject_options()
  muntOptions.add_cmake_defines({
    'munt_WITH_MT32EMU_SMF2WAV' : false, 'munt_WITH_MT32EMU_QT' : false, 'munt_WITH_MT32EMU_ALSADRV' : false,
    'libmt32emu_SHARED' : false, 'mt32emu_BUILD_TESTING' : false, 'CMAKE_POSITION_INDEPENDENT_CODE' : true,
    'CMAKE_BUILD_TYPE' : 'Release',
  })
  muntOptions.set_install(false)
  munt = cmake.subproject('munt', options : muntOptions)
  mt32emuLibrary = munt.target('mt32emu')
  stdcxxStatic = meson.get_compiler('cpp').find_library('stdc++', static : true)
  mt32emuDependency = declare_dependency(
    link_args : [ mt32emuLibrary.full_path(), '-static-libgcc', '-lm' ],
    dependencies : [ stdcxxStatic ],
    include_directories : munt.include_directories('mt32emu'))
  frontendLinkDepends += [ mt32emuLibrary ]
elif get_option('mt32').allowed()
  mt32emuDependency = dependency('mt32emu', required : get_option('mt32'))
  if mt32emuDependency.found()
    frontendRpath = mt32emuDependency.get_variable(pkgconfig : 'libdir', default_value : '')
  endif
endif
if mt32emuDependency.found()
  frontendArgs += [ '-DHAVE_MT32EMU' ]
endif

sdlpop2Frontend = executable('sdlpop2',
  'main.c', 'controller.c', 'overlay_menu.c',
  c_args : frontendArgs,
  dependencies : [ sdlpop2Dependency, sdl2Dependencies, mt32emuDependency ],
  link_language : 'c',
  link_depends : frontendLinkDepends,
  build_rpath : frontendRpath,
  install_rpath : frontendRpath,
  win_subsystem : 'windows',
  install : true,
)

# SDLPoP2.ini: next to the binary in the build directory (the frontend looks there), and installed to share/sdlpop2
configure_file(input : '../SDLPoP2.ini', output : 'SDLPoP2.ini', copy : true)
install_data('../SDLPoP2.ini', install_dir : get_option('datadir') / 'sdlpop2')
