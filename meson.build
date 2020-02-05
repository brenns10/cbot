project(
  'cbot', 'c',
  version : '0.2.0',
)

sources = [
  'src/cbot.c',
  'src/cbot_cli.c',
  'src/cbot_hashchain.c',
  'src/cbot_irc.c',
  'src/main.c',
]

inc = include_directories('inc')

cc = meson.get_compiler('c')
libstephen_dep = dependency(
  'libstephen',
  fallback: ['libstephen', 'libstephen_dep'],
)
libircclient_dep = dependency(
  'libircclient',
  fallback: ['libircclient', 'libircclient_dep'],
)
crypto_dep = dependency('libcrypto')
dl_dep = cc.find_library('dl', required : true)

cbot = executable(
  'cbot', sources,
  include_directories : inc,
  install : true,
  dependencies : [libstephen_dep, libircclient_dep, crypto_dep, dl_dep],
)

plugins = [
  'emote',
  'greet',
  'help',
  'ircctl',
  'karma',
  'lod',
  'log',
  'magic8',
  'name',
  'sadness',
]
plugin_libs = []
foreach f: plugins
  plugin_libs += [
    shared_library(
      f, 'plugin/' + f + '.c',
      include_directories : inc, install : true,
      dependencies : [libstephen_dep],
      name_prefix : '',
    )
  ]
endforeach

install_headers('inc/cbot/cbot.h', subdir : 'cbot')
