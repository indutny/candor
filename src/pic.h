#ifndef _SRC_PIC_H_
#define _SRC_PIC_H_

#include <unistd.h> // intptr_t

namespace candor {
namespace internal {

// Forward declarations
class CodeSpace;
class Masm;

class PIC {
 public:
  PIC(CodeSpace* space);

  char* Generate();

 protected:
  typedef void (*MissCallback)(PIC* pic, char* object, intptr_t result);

  void Generate(Masm* masm);

  static void Miss(PIC* pic, char* object, intptr_t result);
  void Miss(char* object, intptr_t result);
  void Invalidate(char** ip);

  static const int kMaxSize = 5;

  CodeSpace* space_;
  char** protos_[kMaxSize];
  intptr_t* results_[kMaxSize];
  intptr_t* index_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_PIC_H_
