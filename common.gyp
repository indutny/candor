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
    'defines': [ 'CANDOR_ARCH_<(target_arch)' ],
    'conditions': [
      ['OS == "mac"', {
        'defines': [ 'CANDOR_PLATFORM_DARWIN' ]
      }, {
        'defines': [ 'CANDOR_PLATFORM_LINUX' ]
      }],
      ['OS == "mac" and target_arch == "x64"', {
        'xcode_settings': {
          'ARCHS': [ 'x86_64' ]
        },
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
