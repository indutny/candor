#ifndef _SRC_IA32_ASSEMBLER_H_
#define _SRC_IA32_ASSEMBLER_H_

#include <stdint.h> // uint32_t
#include <stdlib.h> // NULL
#include <string.h> // memset

#include "zone.h" // ZoneObject
#include "utils.h" // List

namespace candor {
namespace internal {

// Forward declaration
class Assembler;

struct Register {
  int high() {
    return (code_ >> 3) & 1;
  };

  int low() {
    return code_ & 7;
  };

  int code() {
    return code_;
  }

  inline bool is(Register reg) {
    return code_ == reg.code();
  }

  int code_;
};

const Register reg_nil = { -1 };

const Register eax = { 0 };
const Register ecx = { 1 };
const Register edx = { 2 };
const Register ebx = { 3 };
const Register esp = { 4 };
const Register ebp = { 5 };
const Register esi = { 6 };
const Register edi = { 7 };

const Register scratch = ebx;

struct DoubleRegister {
  int high() {
    return (code_ >> 3) & 1;
  };

  int low() {
    return code_ & 7;
  };

  int code() {
    return code_;
  }

  inline bool is(DoubleRegister reg) {
    return code_ == reg.code();
  }

  int code_;
};

const DoubleRegister xmm0 = { 0 };
const DoubleRegister xmm1 = { 1 };
const DoubleRegister xmm2 = { 2 };
const DoubleRegister xmm3 = { 3 };
const DoubleRegister xmm4 = { 4 };
const DoubleRegister xmm5 = { 5 };
const DoubleRegister xmm6 = { 6 };
const DoubleRegister xmm7 = { 7 };

const DoubleRegister fscratch = xmm7;

class Immediate : public ZoneObject {
 public:
  Immediate(uint32_t value) : value_(value) {
  }

  inline uint32_t value() { return value_; }

 private:
  uint32_t value_;

  friend class Assembler;
};

class Operand : public ZoneObject {
 public:
  enum Scale {
    one = 0,
    two = 1,
    four = 2
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
    kLong
  };

  enum RelocationInfoType {
    kAbsolute,
    kValue,
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
  Label(Assembler* a) : pos_(0), asm_(a) {
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
  kAbove,
  kBelow,
  kAe,
  kBe,
  kCarry,
  kOverflow,
  kNoOverflow
};

enum RoundMode {
  kRoundNearest = 0x00,
  kRoundDown    = 0x01,
  kRoundUp      = 0x02,
  kRoundToward  = 0x03
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
  void nop();
  void cpuid();

  void push(Register src);
  void push(Operand& src);
  void push(Immediate imm);
  void pop(Register dst);
  void ret(uint16_t imm);

  void bind(Label* label);
  void jmp(Label* label);
  void jmp(Condition cond, Label* label);

  void cmpl(Register dst, Register src);
  void cmpl(Register dst, Operand& src);
  void cmpl(Register dst, Immediate src);
  void cmpl(Operand& dst, Immediate src);
  void cmpb(Register dst, Operand& src);
  void cmpb(Register dst, Immediate src);
  void cmpb(Operand& dst, Immediate src);

  void testb(Register dst, Immediate src);
  void testl(Register dst, Immediate src);

  void movl(Register dst, Register src);
  void movl(Register dst, Operand& src);
  void movl(Operand& dst, Register src);
  void movl(Register dst, Immediate src);
  void movl(Operand& dst, Immediate src);
  void movb(Register dst, Immediate src);
  void movb(Operand& dst, Immediate src);
  void movb(Operand& dst, Register src);
  void movzxb(Register dst, Operand& src);

  void xchg(Register dst, Register src);

  void addl(Register dst, Register src);
  void addl(Register dst, Operand& src);
  void addl(Register dst, Immediate src);
  void subl(Register dst, Register src);
  void subl(Register dst, Immediate src);
  void imull(Register src);
  void idivl(Register src);

  void andl(Register dst, Register src);
  void orl(Register dst, Register src);
  void orlb(Register dst, Immediate src);
  void xorl(Register dst, Register src);

  void inc(Register dst);
  void dec(Register dst);
  void shl(Register dst, Immediate src);
  void shr(Register dst, Immediate src);
  void shl(Register dst);
  void shr(Register dst);
  void sal(Register dst, Immediate src);
  void sar(Register dst, Immediate src);
  void sal(Register dst);
  void sar(Register dst);

  void call(Register dst);
  void call(Operand& dst);

  // Floating point instructions
  void movdqu(Operand& dst, DoubleRegister src);
  void movdqu(DoubleRegister dst, Operand &src);
  void addld(DoubleRegister dst, DoubleRegister src);
  void subld(DoubleRegister dst, DoubleRegister src);
  void mulld(DoubleRegister dst, DoubleRegister src);
  void divld(DoubleRegister dst, DoubleRegister src);
  void xorld(DoubleRegister dst, DoubleRegister src);
  void cvtsi2sd(DoubleRegister dst, Register src);
  void cvtsd2si(Register dst, DoubleRegister src);
  void cvttsd2si(Register dst, DoubleRegister src);
  void roundsd(DoubleRegister dst, DoubleRegister src, RoundMode mode);
  void ucomisd(DoubleRegister dst, DoubleRegister src);

  // Routines
  inline void emit_modrm(Register dst);
  inline void emit_modrm(Operand &dst);
  inline void emit_modrm(Register dst, Register src);
  inline void emit_modrm(Register dst, Operand& src);
  inline void emit_modrm(Register dst, uint32_t op);
  inline void emit_modrm(Operand& dst, uint32_t op);
  inline void emit_modrm(DoubleRegister dst, Register src);
  inline void emit_modrm(Register dst, DoubleRegister src);
  inline void emit_modrm(DoubleRegister dst, DoubleRegister src);
  inline void emit_modrm(DoubleRegister dst, Operand& src);
  inline void emit_modrm(Operand& dst, DoubleRegister src);

  inline void emitb(uint8_t v);
  inline void emitw(uint16_t v);
  inline void emitl(uint32_t v);

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

} // namespace internal
} // namespace candor

#endif // _SRC_IA32_ASSEMBLER_H_
