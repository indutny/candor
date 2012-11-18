/**
 * Copyright (c) 2012, Fedor Indutny.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "lir.h"
#include "lir-inl.h"
#include "lir-instructions.h"
#include "lir-instructions-inl.h"

namespace candor {
namespace internal {

void LInstruction::Print(PrintBuffer* p) {
  p->Print("%d: ", id);

  if (result) {
    result->Print(p);
    p->Print(" = ");
  }

  p->Print("%s", TypeToStr(type()));
  if (type() == kLiteral && hir()->ast() != NULL) {
    p->Print("[");
    p->PrintValue(hir()->ast()->value(), hir()->ast()->length());
    p->Print("]");
  }

  for (int i = 0; i < input_count(); i++) {
    if (i == 0) p->Print(" ");
    inputs[i]->Print(p);
    if (i + 1 < input_count()) p->Print(", ");
  }

  if (scratch_count()) {
    p->Print(" # scratches: ");
    for (int i = 0; i < scratch_count(); i++) {
      scratches[i]->Print(p);
      if (i + 1 < scratch_count()) p->Print(", ");
    }
  }

  p->Print("\n");
}


void LGap::MovePair(LGap::Pair* pair) {
  // Fast case - two equal intervals
  if (pair->src_->IsEqual(pair->dst_)) {
    pair->status = kMoved;
    return;
  }

  pair->status = kBeingMoved;

  PairList::Item* head = unhandled_pairs_.head();
  for (; head != NULL; head = head->next()) {
    Pair* other = head->value();

    if (pair->dst_->IsEqual(other->src_)) {
      switch (other->status) {
       case kToMove:
        MovePair(other);
        break;
       case kBeingMoved:
        // Loop detected, add scratch here
        pairs_.Push(new Pair(other->src_, tmp_));
        other->src_ = tmp_;
        break;
       case kMoved:
        // No loop
        break;
       default:
        UNEXPECTED
      }
    }
  }

  pair->status = kMoved;
  pairs_.Push(new Pair(pair->src_, pair->dst_));
}


void LGap::Resolve() {
  PairList::Item* head = unhandled_pairs_.head();
  for (; head != NULL; head = head->next()) {
    Pair* pair = head->value();

    if (pair->status == kToMove) MovePair(pair);
  }

  // Remove all pairs
  while (unhandled_pairs_.length() > 0) {
    assert(unhandled_pairs_.tail()->value()->status == kMoved);
    unhandled_pairs_.Pop();
  }
}


void LGap::Print(PrintBuffer* p) {
  p->Print("%d: Gap[", id);

  PairList::Item* head;
  if (unhandled_pairs_.length() > 0) {
    assert(pairs_.length() == 0);

    head = unhandled_pairs_.head();
  } else {
    head = pairs_.head();
  }

  for (; head != NULL; head = head->next()) {
    Pair* pair = head->value();
    pair->src_->Print(p);
    p->Print(" => ");
    pair->dst_->Print(p);

    if (head->next() != NULL) p->Print(", ");
  }

  p->Print("]\n");
}


void LControlInstruction::Print(PrintBuffer* p) {
  p->Print("%d: %s ", id, TypeToStr(type()));

  for (int i = 0; i < input_count(); i++) {
    inputs[i]->Print(p);
    if (i + 1 < input_count()) {
      p->Print(", ");
    } else {
      p->Print(" ");
    }
  }

  for (int i = 0; i < target_count_; i++) {
    p->Print("(%d)", targets_[i]->id);
    if (i + 1 < target_count_) p->Print(", ");
  }
  p->Print("\n");
}

}  // namespace internal
}  // namespace candor
