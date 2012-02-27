#ifndef _SRC_X64_ASSEMBLER_H_
#define _SRC_X64_ASSEMBLER_H_

#include <stdint.h> // uint32_t
#include <stdlib.h> // NULL
#include <string.h> // memset

#include "zone.h" // ZoneObject
#include "utils.h" // List

namespace dotlang {

// Forward declaration
class Assembler;

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

const Register reg_nil = { -1 };

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

  inline Register base(Register base) { return base_ = base; }
  inline Scale scale(Scale scale) { return scale_ = scale; }
  inline uint32_t disp(uint32_t disp) { return disp_ = disp; }

 private:
  Register base_;
  Scale scale_;
  uint32_t disp_;

  friend class Assembler;
};

class RelocationInfo : public ZoneObject {
 public:
  enum RelocationInfoSize {
    kByte,
    kWord,
    kLong,
    kQuad
  };

  enum RelocationInfoType {
    kAbsolute,
    kRelative
  };

  RelocationInfo(RelocationInfoType type,
                 RelocationInfoSize size,
                 uint32_t offset) : type_(type),
                                    size_(size),
                                    offset_(offset),
                                    target_(0) {

  }

  void Relocate(char* buffer);

  inline void target(uint32_t target) { target_ = target; }

  RelocationInfoType type_;
  RelocationInfoSize size_;

  // Offset of address use in code
  uint32_t offset_;

  // Address to put
  uint32_t target_;
};

class Label {
 public:
  Label(Assembler* a) : pos_(NULL), asm_(a) {
  }

 private:
  inline void relocate(uint32_t offset);
  inline void use(uint32_t offset);

  uint32_t pos_;
  Assembler* asm_;
  List<RelocationInfo*, EmptyClass> uses_;

  friend class Assembler;
};

enum Condition {
  kEq,
  kNe,
  kLt,
  kLe,
  kGt,
  kGe,
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

  // Relocate all absolute/relative addresses in new code space
  void Relocate(char* buffer);

  // Instructions
  void push(Register src);
  void push(Immediate imm);
  void pop(Register dst);
  void ret(uint16_t imm);

  void bind(Label* label);
  void jmp(Label* label);
  void jmp(Condition cond, Label* label);

  void cmp(Register dst, Register src);
  void cmp(Register dst, Operand& src);
  void cmp(Register dst, Immediate src);
  void cmp(Operand& dst, Immediate src);

  void movq(Register dst, Register src);
  void movq(Register dst, Operand& src);
  void movq(Operand& dst, Register src);
  void movq(Register dst, Immediate src);
  void movq(Operand& dst, Immediate src);
  void movl(Operand& dst, Immediate src);
  void movb(Operand& dst, Immediate src);

  void addq(Register dst, Register src);
  void addq(Register dst, Operand& src);
  void addq(Register dst, Immediate src);
  void subq(Register dst, Register src);
  void subq(Register dst, Immediate src);

  void inc(Register dst);
  void dec(Register dst);
  void shl(Register dst, Immediate src);
  void shr(Register dst, Immediate src);

  void callq(Register dst);
  void callq(Operand& dst);

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
  inline void emit_modrm(Operand& dst, uint32_t op);

  inline void emitb(uint8_t v);
  inline void emitw(uint16_t v);
  inline void emitl(uint32_t v);
  inline void emitq(uint64_t v);

  // Increase buffer size automatically
  void Grow();

  inline char* pos() { return buffer_ + offset_; }
  inline char* buffer() { return buffer_; }
  inline uint32_t offset() { return offset_; }
  inline uint32_t length() { return length_; }

  char* buffer_;
  uint32_t offset_;
  uint32_t length_;

  List<RelocationInfo*, ZoneObject> relocation_info_;
};

} // namespace dotlang

#endif // _SRC_X64_ASSEMBLER_H_
