{
  'includes': [ 'common.gyp' ],

  'targets': [{
    'target_name': 'candor',
    'type': 'static_library',
    'include_dirs': [
      'include',
      'src'
    ],
    'cflags': ['-Wall', '-Wextra', '-Wno-unused-parameter',
               '-fPIC', '-fno-strict-aliasing', '-fno-exceptions',
               '-pedantic'],
    'sources': [
      'src/api.cc',
      'src/code-space.cc',
      'src/cpu.cc',
      'src/gc.cc',
      'src/heap.cc',
      'src/hir.cc',
      'src/hir-instructions.cc',
      'src/lir.cc',
      'src/lexer.cc',
      'src/parser.cc',
      'src/root.cc',
      'src/runtime.cc',
      'src/scope.cc',
      'src/source-map.cc',
      'src/visitor.cc',
      'src/zone.cc',
    ],
    'conditions': [
      ['target_arch == "x64"', {
        'sources': [
          'src/x64/assembler-x64.cc',
          'src/x64/macroassembler-x64.cc',
          'src/x64/stubs-x64.cc',
          'src/x64/lir-x64.cc',
        ]
      }],
      ['target_arch == "ia32"', {
        'sources': [
          'src/ia32/assembler-ia32.cc',
          'src/ia32/macroassembler-ia32.cc',
          'src/ia32/stubs-ia32.cc',
          'src/ia32/lir-instructions-ia32.cc',
        ]
      }]
    ]
  }, {
    'target_name': 'can',
    'type': 'executable',

    'dependencies': ['candor'],

    'include_dirs': [
      'include',
      'src'
    ],
    'cflags': ['-Wall', '-Wextra', '-Wno-unused-parameter',
               '-fPIC', '-fno-strict-aliasing', '-fno-exceptions',
               '-pedantic'],
    'sources': [
      'src/can.cc'
    ]
  }]
}
