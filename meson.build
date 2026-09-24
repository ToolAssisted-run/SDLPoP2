project('sdlpop2','c',
  version: '0.9.0',
  license: ['GPL-3.0-or-later', 'LGPL-2.1-or-later'],
  license_files: ['LICENSE', 'NOTICE', 'LICENSES/LGPL-2.1.txt'],
  default_options : ['c_std=gnu11', 'buildtype=release', 'warning_level=1'],
  subproject_dir: 'extern'
)

# Common flags for everything built here
commonCompileArgs = [ '-Wfatal-errors', '-Wall' ]

# The reconstructed game: core logic, renderer, scenes, audio drivers and the program shell
subdir('source')

# Do not build any targets if this is a subproject
if meson.is_subproject() == false

  # The SDL2 frontend (the whole program: title, menus, scenes, levels)
  if get_option('buildFrontend') == true
    subdir('sdl')
  endif

  # Analysis tools (the Go-Explore level explorer)
  if get_option('buildTools') == true
    subdir('tools')
  endif

  # Test programs and the test suite
  if get_option('buildTests') == true
    subdir('tests')
  endif

endif # is_subproject()
