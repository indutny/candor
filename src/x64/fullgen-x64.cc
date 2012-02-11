#include "assembler-x64.h"
#include "fullgen.h"
#include "ast.h" // AstNode
#include "utils.h" // List

#include <assert.h>
#include <stdint.h>

namespace dotlang {

void Fullgen::GeneratePrologue(AstNode* stmt) {
  push(rbp);
  movq(rbp, rsp);

  // Allocate space for on stack variables
  subq(rsp, Immediate(RoundUp(stmt->stack_slots() * sizeof(void*), 16)));
}


void Fullgen::GenerateEpilogue(AstNode* stmt) {
  movq(rsp, rbp);
  pop(rbp);
  ret(0);
}


void Fullgen::GenerateFunction(AstNode* stmt) {
  GeneratePrologue(stmt);
  GenerateBlock(stmt);
  GenerateEpilogue(stmt);
}


void Fullgen::GenerateBlock(AstNode* stmt) {
  AstList::Item* i;

  // Iterate and generate each block child
  for (i = stmt->children()->Head(); i != NULL; i = stmt->children()->Next(i)) {
    switch (i->value()->type()) {
     case AstNode::kAssign:
      GenerateAssign(i->value());
      break;
     default:
      assert(0 && "not implemented");
      break;
    }
  }
}


void Fullgen::GenerateAssign(AstNode* stmt) {
}


void Fullgen::GenerateLookup(AstNode* name) {
  assert(name->is(AstNode::kName));
  AstValue* value = reinterpret_cast<AstValue*>(name);

  if (value->slot()->isStack()) {
  } else if (value->slot()->isContext()) {
  }
}


} // namespace dotlang
