#ifndef _SRC_LIR_H_
#define _SRC_LIR_H_

namespace candor {
namespace internal {

// Forward declarations
class Heap;
class HIR;

class LIR {
 public:
  LIR(Heap* heap, HIR* hir);

  char* Generate();

 private:
  Heap* heap_;
  HIR* hir_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_LIR_H_
