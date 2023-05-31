project(
  'cbot', 'c',
  version : '0.11.5',
  # be sure to update APKBUILD version too
)

sources = [
  'src/cbot.c',
  'src/cbot_cli.c',
  'src/cbot_irc.c',
  'src/db.c',
  'src/tok.c',
  'src/fmt.c',
  'src/curl.c',
  'src/log.c',
  'src/signal/backend.c',
  'src/signal/api.c',
  'src/signal/jmsg.c',
  'src/signal/mention.c',
  'src/http.c',
]

inc = include_directories('inc')

cc = meson.get_compiler('c')
libircclient_dep = dependency(
  'libircclient',
  fallback: ['libircclient', 'libircclient_dep'],
  # There are some compiler warnings, silence them
  default_options: [
    'libircclient:warning_level=0',
  ],
)
libsc_regex_dep = dependency(
  'libsc-regex',
  fallback: ['sc-regex', 'libsc_regex_dep'],
  version: '>=0.4.0',
)
libsc_collections_dep = dependency(
  'libsc-collections',
  fallback: ['sc-collections', 'libsc_collections_dep'],
  version: '>=0.9.0',
)
libsc_argparse_dep = dependency(
  'libsc-argparse',
  fallback: ['sc-argparse', 'libsc_argparse_dep'],
  version: '>=0.2.0',
)
libsc_lwt_dep = dependency(
  'libsc-lwt',
  fallback: ['sc-lwt', 'libsc_lwt_dep'],
  version: '>=0.7.1',
)
libnosj_dep = dependency(
  'libnosj',
  fallback: ['nosj', 'libnosj_dep'],
  version: '>=2.2.1',
)
dl_dep = cc.find_library('dl', required : false)
sqlite_dep = dependency(
  'sqlite3',
  fallback: ['sqlite', 'sqlite_dep'],
)
config_dep = dependency('libconfig')
curl_dep = dependency('libcurl')
uhttp_dep = dependency('libmicrohttpd')

cbot_deps = [
    libsc_collections_dep,
    libsc_regex_dep,
    libsc_argparse_dep,
    libsc_lwt_dep,
    libnosj_dep,
    libircclient_dep,
    dl_dep,
    sqlite_dep,
    config_dep,
    curl_dep,
    uhttp_dep,
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
  'sqlknow',
  'weather',
  'aqi',
  'birthday',
  'buttcoin',
  'reactrack',
  'trivia',
]
plugin_libs = []
foreach f: plugins
  plugin_libs += [
    shared_library(
      f, 'plugin/' + f + '.c',
      include_directories : inc, install : true,
      dependencies : [libcbot_dep] + cbot_deps,
      name_prefix : '',
      install_dir : get_option('libexecdir') / 'cbot',
    )
  ]
endforeach

install_headers('inc/cbot/cbot.h', subdir : 'cbot')
install_headers('inc/cbot/db.h', subdir : 'cbot')

if get_option('doc')
	subdir('doc')
else
	message('Documentation will not be built. Use -Ddoc to build it.')
endif
