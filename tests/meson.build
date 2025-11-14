fs = import('fs')

tests = [
  'mentions.c',
  'fmt2.c',
]
unity_dep = dependency(
    'Unity',
    fallback: ['Unity', 'unity_dep'],
)
math_dep = meson.get_compiler('c').find_library('m', required : false)

foreach t: tests
  testname = fs.name(t)
  extra_deps = []
  if testname == 'fmt2.c'
    extra_deps = [math_dep]
  endif
  exe = executable(
    'test_' + testname,
    t,
    dependencies : [libcbot_dep, unity_dep] + cbot_deps + extra_deps,
    include_directories : inc,
  )
  test('TEST_' + testname, exe)
endforeach

# Plugin testing infrastructure
plugintest_lib = static_library(
  'plugintest',
  ['plugintest.c'],
  dependencies : [libcbot_dep] + cbot_deps,
  include_directories : inc,
)

plugintest_dep = declare_dependency(
  link_with : plugintest_lib,
  include_directories : inc,
)

# Plugin tests - these link against both the plugin and the test backend
plugin_tests = [
  { 'name': 'test_name_plugin.c', 'plugin': '../plugin/name.c' },
]

foreach pt: plugin_tests
  testname = fs.name(pt['name'])
  exe = executable(
    'test_' + testname,
    [pt['name'], pt['plugin']],
    dependencies : [libcbot_dep, unity_dep, plugintest_dep] + cbot_deps,
    include_directories : inc,
  )
  test('TEST_' + testname, exe)
endforeach
