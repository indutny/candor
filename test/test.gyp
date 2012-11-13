{
  'variables': {
    'variables': {
      'host_arch%':
        '<!(uname -m | sed -e "s/i.86/ia32/;\
          s/x86_64/x64/;s/amd64/x64/;s/arm.*/arm/;s/mips.*/mips/")'
    },
    'conditions': [
      ['OS == "mac"', {
        'target_arch%': 'x64'
      }, {
        'target_arch%': '<(host_arch)'
      }]
    ]
  },

  'targets': [{
    'target_name': 'test',
    'type': 'executable',
    'include_dirs': [
      '../include',
      '../src',
      '../test'
    ],
    'cflags': ['-Wall', '-Wextra', '-Wno-unused-parameter',
               '-fPIC', '-fno-strict-aliasing', '-fno-exceptions',
               '-pedantic'],
    'dependencies': ['../candor.gyp:candor'],
    'sources': [
      'test.h',
      'test.cc',
      'test-api.cc',
      'test-binary.cc',
      'test-functional.cc',
      'test-gc.cc',
      'test-numbers.cc',
      'test-parser.cc',
      'test-scope.cc',
      'test-fullgen.cc',
      'test-hir.cc',
      'test-lir.cc',
    ]
  }]
}
