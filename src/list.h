#ifndef _SRC_SORTED_LIST_H_
#define _SRC_SORTED_LIST_H_

#include <stdlib.h> // qsort

namespace candor {
namespace internal {

template <class T, class Shape, class Policy, class Allocator>
class SortableList {
 public:
  SortableList(int size) : map_(NULL), size_(0), grow_(size), len_(0) {
    Grow();
  }

  ~SortableList() {
    delete[] map_;
    map_ = NULL;
  }

  inline T* At(int i) {
    if (i < 0 || i >= len_) return NULL;
    return map_[i];
  }


  inline void RemoveAt(int i) {
    if (i < 0 || i >= len_) return;

    // Shift left
    len_--;
    for (int j = i; j < len_; j++) {
      map_[j] = map_[j + 1];
    }
  }


  inline void Push(T* item) {
    if (len_ == size_) Grow();

    map_[len_++] = item;
  }


  inline void Unshift(T* item) {
    if (len_ == size_) Grow();

    // Shift right
    len_++;
    for (int i = len_ - 1; i > 0; i--) {
      map_[i] = map_[i - 1];
    }
    map_[0] = item;
  }


  inline T* Pop() {
    if (len_ == 0) return NULL;

    return map_[--len_];
  }


  inline T* Shift() {
    if (len_ == 0) return NULL;

    T* res = map_[0];

    // Shift left
    len_--;
    for (int i = 0; i < len_; i++) {
      map_[i] = map_[i + 1];
    }

    return res;
  }


  inline void Sort() {
    typedef int (*RawComparator)(const void*, const void*);
    qsort(map_,
          length(),
          sizeof(*map_),
          reinterpret_cast<RawComparator>(Comparator));
  }


  inline void InsertSorted(T* value) {
    if (len_ == 0) {
      Push(value);
      return;
    }

    if (len_ == size_) Grow();

    // Perform binary search for correct position
    int middle_pos;
    int cmp;
    for (int i = 0, j = length() - 1; i <= j; ) {
      middle_pos = (i + j) >> 1;
      T* middle = map_[middle_pos];

      cmp = Shape::Compare(value, middle);
      if (cmp < 0) {
        j = middle_pos - 1;
      } else if (cmp > 0) {
        i = middle_pos + 1;
      } else {
        break;
      }
    }

    int insert_pos;
    if (cmp <= 0) {
      insert_pos = middle_pos;
    } else {
      insert_pos = middle_pos + 1;
    }

    len_++;
    assert(insert_pos >= 0 && insert_pos < len_);

    for (int i = len_ - 1; i > insert_pos; i--) {
      map_[i] = map_[i - 1];
    }
    map_[insert_pos] = value;
  }

  inline T* head() { return At(0); }
  inline T* tail() { return At(length() - 1); }
  inline int length() { return len_; }

 protected:
  inline void Grow() {
    // Allocate new map
    int new_size = size_ + grow_;
    T** new_map = new T*[new_size];

    // Copy old entries
    if (map_ != NULL) memcpy(new_map, map_, sizeof(*new_map) * size_);

    // Replace map
    delete[] map_;
    map_ = new_map;
    size_ = new_size;
  }

  static inline int Comparator(T** a, T** b) {
    return Shape::Compare(*a, *b);
  }

  T** map_;
  int size_;
  int grow_;
  int len_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_SORTED_LIST_H_
