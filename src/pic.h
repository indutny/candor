#ifndef _SRC_PIC_H_
#define _SRC_PIC_H_

#include <unistd.h> // intptr_t
#include <stdint.h> // uint32_t

namespace candor {
namespace internal {

// Forward declarations
class CodeSpace;
class Masm;

class PIC {
 public:
  PIC(CodeSpace* space);
  ~PIC();

  char* Generate();

 protected:
  typedef void (*MissCallback)(PIC* pic,
                               char* object,
                               intptr_t result,
                               char* ip);

  void Generate(Masm* masm);

  static void Miss(PIC* pic, char* object, intptr_t result, char* ip);
  void Miss(char* object, intptr_t result, char* ip);

  static const int kMaxSize = 5;

  CodeSpace* space_;
  char* addr_;
  char** protos_;
  char** proto_offsets_[kMaxSize];
  intptr_t* results_;
  int size_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_PIC_H_
