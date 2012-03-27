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
               '-pedandtic'],
    'sources': [
      'src/api.cc',
      'src/api.h',
      'src/ast.h',
      'src/code-space.cc',
      'src/code-space.h',
      'src/cpu.cc',
      'src/cpu.h',
      'src/fullgen.h',
      'src/gc.cc',
      'src/gc.h',
      'src/heap.cc',
      'src/heap.h',
      'src/heap-inl.h',
      'src/lexer.cc',
      'src/lexer.h',
      'src/parser.cc',
      'src/parser.h',
      'src/runtime.cc',
      'src/runtime.h',
      'src/scope.cc',
      'src/scope.h',
      'src/source-map.cc',
      'src/source-map.h',
      'src/stubs.h',
      'src/utils.h',
      'src/visitor.cc',
      'src/visitor.h',
      'src/zone.cc',
      'src/zone.h'
    ],
    'conditions': [
      ['target_arch == "x64"', {
        'sources': [
          'src/x64/assembler-x64.cc',
          'src/x64/assembler-x64.h',
          'src/x64/assembler-x64-inl.h',
          'src/x64/fullgen-x64.cc',
          'src/x64/fullgen-x64.h',
          'src/x64/macroassembler-x64.cc',
          'src/x64/macroassembler-x64.h',
          'src/x64/macroassembler-x64-inl.h',
          'src/x64/stubs-x64.cc',
          'src/x64/stubs-x64.h'
        ]
      }],
      ['target_arch == "ia32"', {
        'sources': [
          'src/ia32/assembler-ia32.cc',
          'src/ia32/assembler-ia32.h',
          'src/ia32/assembler-ia32-inl.h',
          'src/ia32/fullgen-ia32.cc',
          'src/ia32/fullgen-ia32.h',
          'src/ia32/macroassembler-ia32.cc',
          'src/ia32/macroassembler-ia32.h',
          'src/ia32/macroassembler-ia32-inl.h',
          'src/ia32/stubs-ia32.cc',
          'src/ia32/stubs-ia32.h'
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
               '-pedandtic'],
    'sources': [
      'src/can.cc'
    ]
  }]
}
