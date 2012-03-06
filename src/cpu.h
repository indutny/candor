#ifndef _SRC_CPU_H_
#define _SRC_CPU_H_

namespace candor {
namespace internal {

class CPU {
 public:
  struct CPUFeatures {
    bool SSE4_1;
  };

  static CPUFeatures cpu_features_;
  static bool probed_;

  static void Probe();
  static inline bool HasSSE4_1() {
    if (!probed_) Probe();
    return cpu_features_.SSE4_1;
  }
};

} // namespace internal
} // namespace candor

#endif // _SRC_CPU_H_
