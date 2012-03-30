#ifndef _SRC_LIR_INSTRUCTIONS_H_
#define _SRC_LIR_INSTRUCTIONS_H_

#include "zone.h"

namespace candor {
namespace internal {

class Masm;

class LInstruction : public ZoneObject {
 public:
  virtual void Generate(Masm* masm) = 0;


};

} // namespace internal
} // namespace candor

#endif // _SRC_LIR_INSTRUCTIONS_H_
