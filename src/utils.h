#ifndef _SRC_UTILS_H_
#define _SRC_UTILS_H_

#include <stdlib.h> // NULL
#include <stdarg.h> // va_list
#include <stdint.h> // uint32_t
#include <stdio.h> // vsnprintf
#include <string.h> // strncmp, memset
#include <unistd.h> // sysconf or getpagesize

namespace dotlang {

inline uint32_t ComputeHash(const char* key, uint32_t length) {
  uint32_t hash = 0;
  for (uint32_t i = 0; i < length; i++) {
    hash += key[i];
    hash += (hash << 10);
    hash ^= (hash >> 6);
  }
  hash += (hash << 3);
  hash ^= (hash >> 11);
  hash += (hash << 15);

  return hash;
}


class EmptyClass { };

template <class T, class ItemParent>
class List {
 public:
  class Item : public ItemParent {
   public:
    Item(T value) : value_(value), prev_(NULL), next_(NULL) {
    }

    inline T value() { return value_; }
    inline void value(T value) { value_ = value; }
    inline Item* next() { return next_; }
    inline Item* prev() { return prev_; }

   protected:
    T value_;
    Item* prev_;
    Item* next_;

    friend class List;
  };

  List() : allocated(false), head_(NULL), current_(NULL), length_(0) {
  }


  ~List() {
    if (allocated) {
      while (length_ > 0) delete Shift();
    } else {
      while (length_ > 0) Shift();
    }
  }


  void Push(T item) {
    Item* next = new Item(item);
    next->prev_ = current_;

    if (head_ == NULL) {
      head_ = next;
    } else {
      current_->next_ = next;
    }

    current_ = next;
    length_++;
  }


  void Unshift(T item) {
    Item* next = new Item(item);
    next->prev_ = NULL;
    next->next_ = head_;
    head_ = next;
    length_++;
  }


  T Shift() {
    if (head_ == NULL) return NULL;

    Item* tmp = head_;
    T value = head_->value();

    if (head_ == current_) current_ = NULL;
    if (head_->next_ != NULL) head_->next()->prev_ = NULL;

    head_ = head_->next();
    delete tmp;
    length_--;

    return value;
  }


  inline Item* head() { return head_; }
  inline uint32_t length() { return length_; }

  bool allocated;

 private:
  Item* head_;
  Item* current_;
  uint32_t length_;
};


template <class T, class ItemParent>
class HashMap {
 public:
  typedef void (*EnumerateCallback)(void* map, T value);

  class Item : public ItemParent {
   public:
    Item(const char* key, uint32_t length, T value) : key_(key),
                                                      length_(length),
                                                      value_(value),
                                                      next_(NULL),
                                                      next_linear_(NULL) {
    }

    inline T value() { return value_; }
    inline void value(T value) { value_ = value; }
    inline Item* next() { return next_; }
    inline Item* next_linear() { return next_linear_; }

   protected:
    const char* key_;
    uint32_t length_;
    T value_;

    Item* next_;
    Item* next_linear_;

    friend class HashMap;
  };

  HashMap() : head_(NULL), current_(NULL) {
    memset(&map_, 0, sizeof(map_));
  }


  void Set(const char* key, uint32_t length, T value) {
    uint32_t index = ComputeHash(key, length) & mask_;
    Item* i = map_[index];
    Item* next = new Item(key, length, value);

    // Setup head or append item to linked list
    // (Needed for enumeration)
    if (head_ == NULL) {
      head_ = next;
    } else {
      current_->next_linear_ = next;
    }
    current_ = next;

    if (i == NULL) {
      map_[index] = next;
    } else {
      while (i->next() != NULL) i = i->next();
      i->next_ = next;
    }
  }


  T Get(const char* key, uint32_t length) {
    uint32_t index = ComputeHash(key, length) & mask_;
    Item* i = map_[index];

    while (i != NULL) {
      if (length == i->length_ &&
          strncmp(i->key_, key, length) == 0) {
        return i->value();
      }
      i = i->next();
    }

    return NULL;
  }


  void Enumerate(EnumerateCallback cb) {
    Item* i = head_;

    while (i != NULL) {
      cb(this, i->value());
      i = i->next_linear();
    }
  }

 private:
  static const uint32_t size_ = 64;
  static const uint32_t mask_ = 63;
  Item* map_[size_];
  Item* head_;
  Item* current_;
};


// For debug printing AST and other things
class PrintBuffer {
 public:
  PrintBuffer(char* buffer, int32_t size) : buffer_(buffer),
                                            left_(size),
                                            total_(0) {
  }

  bool Print(const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    int32_t written = vsnprintf(buffer_, left_, format, arguments);
    va_end(arguments);
    return Consume(written);
  }

  bool PrintValue(const char* value, int32_t size) {
    Consume(size);
    if (ended()) return false;
    memcpy(buffer_ - size, value, size);

    return !ended();
  }

  bool Consume(int32_t bytes) {
    buffer_ += bytes;
    total_ += bytes;
    left_ -= bytes;

    return !ended();
  }

  void Finalize() {
    if (ended()) return;
    buffer_[total_] = 0;
  }

  inline bool ended() { return left_ <= 0; }
  inline int32_t total() { return total_ <= 0; }

 private:
  char* buffer_;
  int32_t left_;
  int32_t total_;
};



// Find minimum number that's greater than value and is dividable by to
inline uint32_t RoundUp(uint32_t value, uint32_t to) {
  if (value % to == 0) return value;

  return value + to - value % to;
}


inline uint32_t PowerOfTwo(uint32_t value) {
  uint32_t result = 2;

  while (result != 0 && result < value) result <<= 1;

  return result;
}


// XXX: Naive implementation
inline uint64_t StringToInt(const char* value, uint32_t length) {
  uint64_t result = 0;
  for (uint32_t index = 0; index < length; index++) {
    result *= 10;
    result += value[index] - '0';
  }
  return result;
}


inline uint32_t IntToString(uint64_t value, char* str) {
  uint32_t len;
  uint64_t num = value;

  // Insert reversed value 1234 => '4321'
  for (len = 0; num > 0; len++, num = num / 10) {
    str[len] = (num % 10) + '0';
  }

  // Reverse it
  for (uint32_t i = 0; i < len >> 1; i++) {
    char t = str[i];
    str[i] = str[len - i - 1];
    str[len - i - 1] = t;
  }
  str[len] = 0;

  return len;
}


inline uint32_t GetPageSize() {
#ifdef __DARWIN
  return getpagesize();
#else
  return sysconf(_SC_PAGE_SIZE);
#endif
}

} // namespace dotlang

#endif // _SRC_UTILS_H_
