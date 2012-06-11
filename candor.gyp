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
      'src/hir.cc',
      'src/hir.h',
      'src/hir-instructions.cc',
      'src/hir-instructions.h',
      'src/heap-inl.h',
      'src/lexer.cc',
      'src/lexer.h',
      'src/lir.cc',
      'src/lir.h',
      'src/lir-inl.h',
      'src/lir-allocator.h',
      'src/lir-allocator-inl.h',
      'src/lir-allocator.cc',
      'src/lir-instructions.cc',
      'src/lir-instructions.h',
      'src/lir-instructions-inl.h',
      'src/macroassembler.h',
      'src/macroassembler-inl.h',
      'src/parser.cc',
      'src/parser.h',
      'src/root.cc',
      'src/root.h',
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
          'src/x64/lir-instructions-x64.cc',
          'src/x64/lir-instructions-x64.h',
          'src/x64/macroassembler-x64.cc',
          'src/x64/stubs-x64.cc',
          'src/x64/stubs-x64.h'
        ]
      }],
      ['target_arch == "ia32"', {
        'sources': [
          'src/ia32/assembler-ia32.cc',
          'src/ia32/assembler-ia32.h',
          'src/ia32/assembler-ia32-inl.h',
          'src/ia32/lir-instructions-ia32.cc',
          'src/ia32/lir-instructions-ia32.h',
          'src/ia32/macroassembler-ia32.cc',
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
               '-pedantic'],
    'sources': [
      'src/can.cc'
    ]
  }]
}
