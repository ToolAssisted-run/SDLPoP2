# Test programs. Most compare the C against oracle captures (the real game in DOSBox-X): their data lives outside the
# repository ($POP2_WORKSPACE, default ~/pop2dec), so only the core round-trip test runs by default (with -DgameDir); -DoracleTests=true adds
# the capture suites (the scripts in this directory, run with the programs built here).
testHelpers = files('snap.c', 'testutil.c')

testPrograms = {
  'e2e'       : 'end to end: whole levels from a capture\'s start, every tick compared',
  'snaptest'  : 'single ticks and passes against DS snapshots',
  'coretest'  : 'the core API: savestate round trips, unreconstructed routines reached',
  'tiletest'  : 'whole-room draw tables against the game\'s',
  'frametest' : 'the per-frame drawing against the game\'s offscreen buffer',
  'shelltest' : 'the program shell: ticks and screens',
  'audiotest' : 'the sound drivers\' register streams and audio',
  'ctltest'   : 'control() cases',
  'inputtest' : 'keyboard to controls',
}
testExecutables = {}
foreach name, description : testPrograms
  testExecutables += { name : executable(name,
    name + '.c', testHelpers,
    c_args : commonCompileArgs,
    dependencies : [ sdlpop2Dependency ],
  ) }
endforeach

# The story scenes' test, with the whole program linked (transitions 2 and 3 draw a game room through the shell)
nistest = executable('nistest',
  'nistest.c',
  c_args : [ commonCompileArgs, '-DNIS_ENGINE' ],
  dependencies : [ sdlpop2Dependency ],
)

# play_seq pairs: a standalone harness over seq.c and dat.c only (it defines its own stand-ins)
seqtest = executable('seqtest',
  'seqtest.c', files('../source/seq.c', '../source/dat.c'),
  c_args : commonCompileArgs,
  include_directories : include_directories('../source'),
)

# The core round trips (needs the game files)
if get_option('gameDir') != ''
  test('coretest', testExecutables['coretest'],
       args : [ get_option('gameDir'), '2000' ],
       timeout : 600,
       suite : [ 'core' ])
endif

# The oracle suites: the scripts take the programs built here through BUILD
if get_option('oracleTests') == true
  bash = find_program('bash')
  oracleEnv = environment({ 'BUILD' : meson.current_build_dir() })
  check = files('oracle_check.sh')
  oracleSuites = {
    'ticks' : [ 'ticks', files('run_all.sh') ],
    'tiles' : [ 'tiles', files('../tools/tiletests.sh') ],
    'sound' : [ 'info', files('run_soundmodel.sh') ],
    'shell' : [ 'info', files('run_shell.sh') ],
  }
  foreach name, how : oracleSuites
    test(name, bash,
         args : [ check, how[0], how[1] ],
         env : oracleEnv,
         workdir : meson.project_source_root(),
         timeout : 0,
         is_parallel : false,
         suite : [ 'oracle' ])
  endforeach
endif
