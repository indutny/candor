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
  }
}
