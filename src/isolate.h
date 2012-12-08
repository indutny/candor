#ifndef _SRC_ISOLATE_H_
#define _SRC_ISOLATE_H_

namespace candor {

// Forward declaratons
class Isolate;

namespace internal {

// Forward declarations
class Zone;

class IsolateData {
 public:
  ~IsolateData();

  static IsolateData* GetCurrent();

  Isolate* isolate;
  Zone* zone;

 protected:
  IsolateData();

  static void Init();
};

} // namespace internal
} // namespace candor

#endif // _SRC_ISOLATE_H_
