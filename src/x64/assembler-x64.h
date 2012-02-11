#ifndef _SRC_X64_ASSEMBLER_H_
#define _SRC_X64_ASSEMBLER_H_

#include <stdint.h> // uint32_t
#include <stdlib.h> // NULL
#include <string.h> // memset

namespace dotlang {

struct Register {
  const int high() {
    return code_ >> 3;
  };

  const int low() {
    return code_ & 7;
  };

  const int code() {
    return code_;
  }

  int code_;
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
 public:
  Immediate(uint32_t value) : value_(value) {
  }

 private:
  uint32_t value_;

  friend class Assembler;
};

class Operand {
 public:
  enum Scale {
    one = 0,
    two = 1,
    four = 2,
    eight = 3
  };

  Operand(Register base, Scale scale, uint32_t disp);
  Operand(Register base, uint32_t disp);

 private:

  friend class Assembler;
};

class Assembler {
 public:
  Assembler() : offset_(0), length_(256) {
    buffer_ = new char[length_];
    memset(buffer_, 0x90, length_);
  }

  ~Assembler() {
    delete buffer_;
  }

  // Instructions
  void push(Register src);
  void pop(Register dst);
  void ret(uint16_t imm);

  void movq(Register dst, Register src);

  // Routines
  void emit_rex_if_high(Register src);
  void emit_rexw(Register dst, Register src);
  void emit_modrm(Register dst, Register src);

  void emitb(uint8_t v);
  void emitw(uint16_t v);
  void emitl(uint32_t v);
  void emitq(uint64_t v);

  // Increase buffer size automatically
  void Grow();

  inline char* pos() {
    return buffer_ + offset_;
  }

  inline char* buffer() {
    return buffer_;
  }

  inline uint32_t length() {
    return length_;
  }

  char* buffer_;
  uint32_t offset_;
  uint32_t length_;
};

} // namespace dotlang

#endif // _SRC_X64_ASSEMBLER_H_
