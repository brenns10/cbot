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
