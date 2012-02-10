#ifndef _SRC_X64_ASSEMBLER_H_
#define _SRC_X64_ASSEMBLER_H_

struct Register {
  const int high() {
    return code >> 3;
  };

  const int low() {
    return code & 7;
  };

  int code;
};

const Register rax = { 0 };
const Register rcx = { 1 };
const Register rdx = { 2 };
const Register rbx = { 3 };
const Register rsp = { 4 };
const Register rbp = { 5 };
const Register rsi = { 6 };
const Register rdi = { 7 };

const Register r8  = { 8 };
const Register r9  = { 9 };
const Register r10 = { 10 };
const Register r11 = { 11 };
const Register r12 = { 12 };
const Register r13 = { 13 };
const Register r14 = { 14 };
const Register r15 = { 15 };

class Immediate {
};

class Operand {
};

class Assembler {
};

#endif // _SRC_X64_ASSEMBLER_H_
