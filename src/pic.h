#ifndef _SRC_PIC_H_
#define _SRC_PIC_H_

#include <unistd.h> // intptr_t
#include <stdint.h> // uint32_t

namespace candor {
namespace internal {

// Forward declarations
class CodeSpace;
class CodeChunk;
class Masm;

class PIC {
 public:
  typedef void (*MissCallback)(PIC* pic,
                               char* object,
                               intptr_t result,
                               char* ip);

  PIC(CodeSpace* space);
  ~PIC();

  char* Generate();
  static void Miss(PIC* pic, char* object, intptr_t result, char* ip);

 protected:

  void Generate(Masm* masm);

  void Miss(char* object, intptr_t result, char* ip);

  static const int kMaxSize = 5;

  CodeSpace* space_;
  CodeChunk* chunk_;
  char** protos_;
  char** proto_offsets_[kMaxSize];
  intptr_t* results_;
  int size_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_PIC_H_
