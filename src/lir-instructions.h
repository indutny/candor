#ifndef _SRC_LIR_INSTRUCTIONS_H_
#define _SRC_LIR_INSTRUCTIONS_H_

#include "zone.h" // Zone
#include "utils.h" // Lists and etc

namespace candor {
namespace internal {

// Forward-declarations
class LGen;
class LBlock;
class LInstruction;

typedef ZoneList<LInstruction*> LInstructionList;

class LInstruction : public ZoneObject {
 public:
  LInstruction(LGen* g);

  int id;

 private:
  LGen* g_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_LIR_INSTRUCTIONS_H_
