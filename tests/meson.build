fs = import('fs')

tests = [
  'mentions.c',
]
unity_dep = dependency(
    'Unity',
    fallback: ['Unity', 'unity_dep'],
)
foreach t: tests
  testname = fs.name(t)
  exe = executable(
    'test_' + testname,
    t,
    dependencies : [libcbot_dep, unity_dep] + cbot_deps,
    include_directories : inc,
  )
  test('TEST_' + testname, exe)
endforeach
