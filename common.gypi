{
  'variables': {
    'variables': {
      'host_arch%':
        '<!(uname -m | sed -e "s/i.86/ia32/;\
          s/x86_64/x64/;s/amd64/x64/;s/arm.*/arm/;s/mips.*/mips/")',
      'osx_arch%': ''
    },
    'conditions': [
      ['OS == "mac" and osx_arch != "ia32"', {
        'target_arch%': 'x64'
      }, {
        'target_arch%': '<(host_arch)'
      }]
    ]
  },
  'target_defaults': {
    'default_configuration': 'Debug',
    'cflags': [ '-Wall', '-pthread', '-fno-strict-aliasing' ],
    'defines': [ 'CANDOR_ARCH_<(target_arch)' ],
    'conditions': [
      ['OS == "mac"', {
        'xcode_settings': {
          'GCC_VERSION': '4.1',
          'GCC_WARN_ABOUT_MISSING_NEWLINE': 'YES',  # -Wnewline-eof
          'PREBINDING': 'NO',                       # No -Wl,-prebind
          'MACOSX_DEPLOYMENT_TARGET': '10.5',       # -mmacosx-version-min=10.5
          'USE_HEADERMAP': 'NO',
          'WARNING_CFLAGS': [
            '-Wall',
            '-Wendif-labels',
            '-W',
            '-Wno-unused-parameter',
          ],
        }
      }],
      ['OS == "mac" and target_arch == "x64"', {
        'xcode_settings': {
          'ARCHS': [ 'x86_64' ]
        },
      }],
      ['OS == "mac"', {
        'defines': [ 'CANDOR_PLATFORM_DARWIN' ],
      }, {
        'defines': [ 'CANDOR_PLATFORM_LINUX' ]
      }]
    ],
    'configurations': {
      'Debug': {
        'cflags': [ '-g', '-O0' ],
        'xcode_settings': {
          'OTHER_CFLAGS': [ '-g', '-O0' ]
        }
      },
      'Release': {
        'cflags': [ '-g', '-O3' ],
        'defines': [ 'NDEBUG' ],
        'xcode_settings': {
          'OTHER_CFLAGS': [ '-g', '-O3' ]
        }
      }
    }
  }
}
