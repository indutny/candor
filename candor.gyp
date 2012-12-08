{
  'target_defaults': {
    'defines': [ 'CANDOR_ARCH_<(target_arch)' ],
    'conditions': [
      ['OS == "mac"', {
        'defines': [ 'CANDOR_PLATFORM_DARWIN' ],
      }, {
        'defines': [ 'CANDOR_PLATFORM_LINUX' ]
      }]
    ]
  },
  'targets': [{
    'target_name': 'candor',
    'type': 'static_library',
    'include_dirs': [
      'include',
      'src'
    ],
    'sources': [
      'src/zone.cc',
      'src/isolate.cc',
      'src/api.cc',
      'src/code-space.cc',
      'src/cpu.cc',
      'src/gc.cc',
      'src/heap.cc',
      'src/lexer.cc',
      'src/parser.cc',
      'src/scope.cc',
      'src/root.cc',
      'src/visitor.cc',
      'src/source-map.cc',
      'src/fullgen.cc',
      'src/fullgen-instructions.cc',
      'src/hir.cc',
      'src/hir-instructions.cc',
      'src/lir.cc',
      'src/lir-instructions.cc',
      'src/pic.cc',
      'src/macroassembler.cc',
      'src/runtime.cc',
    ],
    'conditions': [
      ['target_arch == "x64"', {
        'sources': [
          'src/x64/assembler-x64.cc',
          'src/x64/macroassembler-x64.cc',
          'src/x64/stubs-x64.cc',
          'src/x64/fullgen-x64.cc',
          'src/x64/lir-builder-x64.cc',
          'src/x64/lir-x64.cc',
          'src/x64/pic-x64.cc',
        ]
      }],
      ['target_arch == "ia32"', {
        'sources': [
          'src/ia32/assembler-ia32.cc',
          'src/ia32/macroassembler-ia32.cc',
          'src/ia32/stubs-ia32.cc',
          'src/ia32/fullgen-ia32.cc',
          'src/ia32/lir-builder-ia32.cc',
          'src/ia32/lir-ia32.cc',
          'src/ia32/pic-ia32.cc',
        ]
      }],
    ]
  }, {
    'target_name': 'can',
    'type': 'executable',

    'dependencies': ['candor'],

    'include_dirs': [
      'include',
      'src'
    ],
    'sources': [
      'src/can.cc'
    ]
  }]
}
