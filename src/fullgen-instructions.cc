#include "fullgen.h"
#include "fullgen-inl.h"
#include "fullgen-instructions.h"
#include "fullgen-instructions-inl.h"

namespace candor {
namespace internal {

FInstruction::FInstruction(Type type) : id(-1),
                                        f_(NULL),
                                        ast_(NULL),
                                        type_(type),
                                        input_count_(0) {
  result = NULL;
  inputs[0] = NULL;
  inputs[1] = NULL;
  inputs[2] = NULL;
}


void FInstruction::Init(Fullgen* f) {
  f_ = f;
  id = f->instr_id();
}


void FInstruction::Print(PrintBuffer* p) {
  p->Print("%d ", id);
  if (result != NULL) {
    result->Print(p);
    p->Print(" = ");
  }
  p->Print("%s", TypeToStr(type_));
  if (input_count_ > 0) {
    p->Print("(");
    for (int i = 0; i < input_count_; i++) {
      inputs[i]->Print(p);
      if (i + 1 < input_count_) p->Print(", ");
    }
    p->Print(")");
  }
  p->Print("\n");
}


void FBreak::Print(PrintBuffer* p) {
  p->Print("%d Break => %d\n", id, label_->id);
}


void FContinue::Print(PrintBuffer* p) {
  p->Print("%d Continue => %d\n", id, label_->id);
}


void FGoto::Print(PrintBuffer* p) {
  p->Print("%d Goto => %d\n", id, label_->id);
}


void FIf::Print(PrintBuffer* p) {
  p->Print("%d If (", id);
  inputs[0]->Print(p);
  p->Print(") => %d Else %d\n", t_->id, f_->id);
}

} // internal
} // candor
