project(
  'cbot', 'c',
  version : '0.3.0',
)

sources = [
  'src/cbot.c',
  'src/cbot_cli.c',
  'src/cbot_hashchain.c',
  'src/cbot_irc.c',
  'src/db.c',
  'src/tok.c',
  'src/fmt.c',
  #'src/main.c',
]

inc = include_directories('inc')

cc = meson.get_compiler('c')
libircclient_dep = dependency(
  'libircclient',
  fallback: ['libircclient', 'libircclient_dep'],
)
libsc_regex_dep = dependency(
  'libsc-regex',
  fallback: ['sc-regex', 'libsc_regex_dep'],
  version: '>=0.3.2',
)
libsc_collections_dep = dependency(
  'libsc-collections',
  fallback: ['sc-collections', 'libsc_collections_dep'],
  version: '>=0.5.1',
)
libsc_argparse_dep = dependency(
  'libsc-argparse',
  fallback: ['sc-argparse', 'libsc_argparse_dep'],
  version: '>=0.2.0',
)
libsc_lwt_dep = dependency(
  'libsc-lwt',
  fallback: ['sc-lwt', 'libsc_lwt_dep'],
  version: '>=0.1.0',
)
crypto_dep = dependency('libcrypto')
dl_dep = cc.find_library('dl', required : true)
sqlite_dep = dependency(
  'sqlite3',
  fallback: ['sqlite', 'sqlite_dep'],
)
config_dep = dependency('libconfig')

cbot_deps = [
    libsc_collections_dep,
    libsc_regex_dep,
    libsc_argparse_dep,
    libsc_lwt_dep,
    libircclient_dep,
    crypto_dep,
    dl_dep,
    sqlite_dep,
    config_dep,
]

libcbot = shared_library(
  'cbot', sources,
  include_directories : inc,
  install : true,
  dependencies : cbot_deps,
)
libcbot_dep = declare_dependency(
  include_directories : inc,
  link_with : libcbot,
)
cbot = executable(
  'cbot', ['src/main.c'],
  include_directories : inc,
  install : true,
  dependencies : [libcbot_dep] + cbot_deps,
)

plugins = [
  'emote',
  'greet',
  'help',
  'ircctl',
  'karma',
  'log',
  'name',
  'become',
  'who',
  'annoy',
  'tok',
  'reply',
  'sqlkarma',
]
plugin_libs = []
foreach f: plugins
  plugin_libs += [
    shared_library(
      f, 'plugin/' + f + '.c',
      include_directories : inc, install : true,
      dependencies : [libcbot_dep] + cbot_deps,
      name_prefix : '',
    )
  ]
endforeach

install_headers('inc/cbot/cbot.h', subdir : 'cbot')
install_headers('inc/cbot/db.h', subdir : 'cbot')
