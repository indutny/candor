#include "isolate.h"

#include <pthread.h>
#include <stdlib.h> // abort
#include <assert.h> // assert

namespace candor {
namespace internal {

static pthread_once_t isolate_once = PTHREAD_ONCE_INIT;
static pthread_key_t isolate_key;

IsolateData::IsolateData() : isolate(NULL), zone(NULL) {
}


IsolateData::~IsolateData() {
  pthread_setspecific(isolate_key, NULL);
}


void IsolateData::Init() {
  if (pthread_key_create(&isolate_key, NULL)) {
    abort();
  }
}


IsolateData* IsolateData::GetCurrent() {
  if (pthread_once(&isolate_once, IsolateData::Init))
    abort();

  IsolateData* data =
      reinterpret_cast<IsolateData*>(pthread_getspecific(isolate_key));
  if (data == NULL) {
    data = new IsolateData();
    pthread_setspecific(isolate_key, data);
  }

  return data;
}

} // namespace internal
} // namespace candor
