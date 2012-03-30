#include "lir-instructions-x64.h"

namespace candor {
namespace internal {

#define __ masm()->

void LIREntry::Generate() {
}


void LIRReturn::Generate() {
}


void LIRGoto::Generate() {
}


void LIRStoreLocal::Generate() {
}


void LIRStoreContext::Generate() {
}


void LIRStoreProperty::Generate() {
}


void LIRLoadRoot::Generate() {
}


void LIRLoadLocal::Generate() {
}


void LIRLoadContext::Generate() {
}


void LIRBranchBool::Generate() {
}


void LIRAllocateObject::Generate() {
}

} // namespace internal
} // namespace candor
