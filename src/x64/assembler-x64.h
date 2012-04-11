#ifndef _SRC_X64_ASSEMBLER_H_
#define _SRC_X64_ASSEMBLER_H_

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

const Register context_reg = rsi;
const Register root_reg = rdi;
const Register scratch = r14;

static inline Register RegisterByIndex(int index) {
  // rsi, rdi, r14, r15 are reserved
  switch (index) {
   case 0: return rax;
   case 1: return rbx;
   case 2: return rcx;
   case 3: return rdx;
   case 4: return r8;
   case 5: return r9;
   case 6: return r10;
   case 7: return r11;
   case 8: return r12;
   case 9: return r13;
   default: UNEXPECTED return reg_nil;
  }
}


static inline const char* RegisterNameByIndex(int index) {
  // rsi, rdi, r14, r15 are reserved
  switch (index) {
   case 0: return "rax";
   case 1: return "rbx";
   case 2: return "rcx";
   case 3: return "rdx";
   case 4: return "r8 ";
   case 5: return "r9 ";
   case 6: return "r10";
   case 7: return "r11";
   case 8: return "r12";
   case 9: return "r13";
   default: UNEXPECTED return "rnil";
  }
}


static inline int IndexByRegister(Register reg) {
  // rsi, rdi, r14, r15 are reserved
  switch (reg.code()) {
   case 0: return 0;
   case 1: return 2;
   case 2: return 3;
   case 3: return 1;
   case 8: return 4;
   case 9: return 5;
   case 10: return 6;
   case 11: return 7;
   case 12: return 8;
   case 13: return 9;
   default: UNEXPECTED return -1;
  }
}


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
const DoubleRegister xmm8 = { 8 };
const DoubleRegister xmm9 = { 9 };
const DoubleRegister xmm10 = { 10 };
const DoubleRegister xmm11 = { 11 };
const DoubleRegister xmm12 = { 12 };
const DoubleRegister xmm13 = { 13 };
const DoubleRegister xmm14 = { 14 };
const DoubleRegister xmm15 = { 15 };

const DoubleRegister fscratch = xmm11;

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

  void cmpq(Register dst, Register src);
  void cmpq(Register dst, Operand& src);
  void cmpq(Register dst, Immediate src);
  void cmpq(Operand& dst, Immediate src);
  void cmpb(Register dst, Operand& src);
  void cmpb(Register dst, Immediate src);
  void cmpb(Operand& dst, Immediate src);

  void testb(Register dst, Immediate src);
  void testl(Register dst, Immediate src);

  void mov(Register dst, Register src);
  void mov(Register dst, Operand& src);
  void mov(Operand& dst, Register src);
  void mov(Register dst, Immediate src);
  void mov(Operand& dst, Immediate src);
  void movl(Operand& dst, Immediate src);
  void movb(Register dst, Immediate src);
  void movb(Operand& dst, Immediate src);
  void movb(Operand& dst, Register src);
  void movzxb(Register dst, Operand& src);

  void xchg(Register dst, Register src);

  void addq(Register dst, Register src);
  void addl(Register dst, Register src);
  void addq(Register dst, Operand& src);
  void addq(Register dst, Immediate src);
  void subq(Register dst, Register src);
  void subq(Register dst, Immediate src);
  void imulq(Register src);
  void idivq(Register src);

  void andq(Register dst, Register src);
  void orq(Register dst, Register src);
  void orqb(Register dst, Immediate src);
  void xorq(Register dst, Register src);
  void xorl(Register dst, Register src);

  void inc(Register dst);
  void dec(Register dst);
  void shl(Register dst, Immediate src);
  void shr(Register dst, Immediate src);
  void shll(Register dst, Immediate src);
  void shrl(Register dst, Immediate src);
  void shl(Register dst);
  void shr(Register dst);
  void sal(Register dst, Immediate src);
  void sar(Register dst, Immediate src);
  void sal(Register dst);
  void sar(Register dst);

  void callq(Register dst);
  void callq(Operand& dst);

  // Floating point instructions
  void movd(DoubleRegister dst, Register src);
  void movd(Register dst, DoubleRegister src);
  void movd(Operand& dst, DoubleRegister src);
  void addqd(DoubleRegister dst, DoubleRegister src);
  void subqd(DoubleRegister dst, DoubleRegister src);
  void mulqd(DoubleRegister dst, DoubleRegister src);
  void divqd(DoubleRegister dst, DoubleRegister src);
  void xorqd(DoubleRegister dst, DoubleRegister src);
  void cvtsi2sd(DoubleRegister dst, Register src);
  void cvtsd2si(Register dst, DoubleRegister src);
  void cvttsd2si(Register dst, DoubleRegister src);
  void roundsd(DoubleRegister dst, DoubleRegister src, RoundMode mode);
  void ucomisd(DoubleRegister dst, DoubleRegister src);

  // Routines
  inline void emit_rex_if_high(Register src);
  inline void emit_rexw(Register dst);
  inline void emit_rexw(Operand& dst);
  inline void emit_rexw(Register dst, Register src);
  inline void emit_rexw(Register dst, Operand& src);
  inline void emit_rexw(Operand& dst, Register src);
  inline void emit_rexw(DoubleRegister dst, Register src);
  inline void emit_rexw(DoubleRegister dst, DoubleRegister src);
  inline void emit_rexw(Register dst, DoubleRegister src);
  inline void emit_rexw(DoubleRegister dst, Operand& src);

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

  ZoneList<RelocationInfo*> relocation_info_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_X64_ASSEMBLER_H_
