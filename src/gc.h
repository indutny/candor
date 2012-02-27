#ifndef _SRC_GC_H_
#define _SRC_GC_H_

namespace dotlang {

// Forward declarations
class Heap;
class HContext;

class GC {
 public:
  void CollectGarbage(HContext* context);
};

} // namespace dotlang

#endif // _SRC_GC_H_
