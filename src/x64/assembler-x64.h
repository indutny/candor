#ifndef _SRC_X64_ASSEMBLER_H_
#define _SRC_X64_ASSEMBLER_H_

#include <stdint.h> // uint32_t
#include <stdlib.h> // NULL
#include <string.h> // memset

#include "zone.h" // ZoneObject
#include "utils.h" // List

namespace dotlang {

struct Register {
  const int high() {
    return (code_ >> 3) & 1;
  };

  const int low() {
    return code_ & 7;
  };

  const int code() {
    return code_;
  }

  inline bool is(Register reg) {
    return code_ == reg.code();
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

const Register scratch = r11;

class Immediate : public ZoneObject {
 public:
  Immediate(uint64_t value) : value_(value) {
  }

  inline uint64_t value() { return value_; }
  inline bool is64() { return value_ > 0xffffffff; }

 private:
  uint64_t value_;

  friend class Assembler;
};

class Operand : public ZoneObject {
 public:
  enum Scale {
    one = 0,
    two = 1,
    four = 2,
    eight = 3
  };

  Operand(Register base, Scale scale, uint32_t disp) : base_(base),
                                                       scale_(scale),
                                                       disp_(disp) {
  }
  Operand(Register base, uint32_t disp) : base_(base),
                                          scale_(one),
                                          disp_(disp) {
  }

  inline Register base() { return base_; }
  inline Scale scale() { return scale_; }
  inline uint32_t disp() { return disp_; }

 private:
  Register base_;
  Scale scale_;
  uint32_t disp_;


  friend class Assembler;
};

class Label {
 public:
  Label(): pos_(NULL) {
  }

 private:
  inline void relocate(char* pos);
  inline void use(char* addr);
  inline void emit(char* addr);

  char* pos_;
  List<char*, EmptyClass> addrs_;
  friend class Assembler;
};

enum Condition {
  kEq,
  kLt,
  kGt,
  kCarry
};

class Assembler {
 public:
  Assembler() : offset_(0), length_(256) {
    buffer_ = new char[length_];
    memset(buffer_, 0xCC, length_);
  }

  ~Assembler() {
    delete[] buffer_;
  }

  // Instructions
  void push(Register src);
  void pop(Register dst);
  void ret(uint16_t imm);

  void bind(Label* label);
  void cmp(Register dst, Register src);
  void cmp(Register dst, Operand& src);
  void jmp(Label* label);
  void jmp(Condition cond, Label* label);

  void movq(Register dst, Register src);
  void movq(Register dst, Operand& src);
  void movq(Operand& dst, Register src);
  void movq(Register dst, Immediate src);
  void movq(Operand& dst, Immediate src);

  void addq(Register dst, Immediate src);
  void subq(Register dst, Immediate imm);

  // Routines
  inline void emit_rex_if_high(Register src);
  inline void emit_rexw(Register dst);
  inline void emit_rexw(Operand& dst);
  inline void emit_rexw(Register dst, Register src);
  inline void emit_rexw(Register dst, Operand& src);
  inline void emit_rexw(Operand& dst, Register src);

  inline void emit_modrm(Register dst);
  inline void emit_modrm(Operand &dst);
  inline void emit_modrm(Register dst, Register src);
  inline void emit_modrm(Register dst, Operand& src);
  inline void emit_modrm(Register dst, uint32_t op);

  inline void emitb(uint8_t v);
  inline void emitw(uint16_t v);
  inline void emitl(uint32_t v);
  inline void emitq(uint64_t v);

  // Increase buffer size automatically
  inline void Grow();

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
