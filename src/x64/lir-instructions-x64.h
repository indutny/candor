#ifndef _SRC_X64_LIR_INSTRUCTIONS_X64_H_
#define _SRC_X64_LIR_INSTRUCTIONS_X64_H_

#include "zone.h"
#include "utils.h"

namespace candor {
namespace internal {

// Forward delcarations
class LInstruction;
typedef ZoneList<LInstruction*> LInstructionList;

class LInstruction : public ZoneObject {
 public:
  int id;
};

} // namespace internal
} // namespace candor

#endif // _SRC_X64_LIR_INSTRUCTIONS_X64_H_
