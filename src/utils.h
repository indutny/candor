#ifndef _SRC_UTILS_H_
#define _SRC_UTILS_H_

#include <stdlib.h> // NULL
#include <stdint.h> // uint32_t
#include <string.h> // strncmp, memset

namespace dotlang {

template <class T>
class List {
 public:
  class Item {
   public:
    Item(T value) : value_(value), prev_(NULL), next_(NULL) {
    }

    inline T value() {
      return value_;
    }

    T value_;
    Item* prev_;
    Item* next_;
  };

  List() : head(NULL), current(NULL), length(0), allocated(false) {
  }

  ~List() {
    if (allocated) {
      while (length > 0) delete Shift();
    } else {
      while (length > 0) Shift();
    }
  }

  void Push(T item) {
    Item* next = new Item(item);
    next->prev_ = current;
    if (current != NULL) current->next_ = next;
    if (head == NULL) head = next;

    current = next;
    length++;
  }

  void Unshift(T item) {
    Item* next = new Item(item);
    next->prev_ = NULL;
    next->next_ = head;
    head = next;
  }

  T Shift() {
    if (head == NULL) return NULL;

    Item* tmp = head;
    T value = head->value();

    if (head == current) current = NULL;
    if (head->next_ != NULL) head->next_->prev_ = NULL;

    head = head->next_;
    delete tmp;
    length--;

    return value;
  }

  Item* Head() {
    return head;
  }

  Item* Next(Item* item) {
    return item->next_;
  }

  Item* Prev(Item* item) {
    return item->prev_;
  }

  uint32_t Length() {
    return length;
  }

  Item* head;
  Item* current;
  uint32_t length;

  bool allocated;
};


template <class T>
class HashMap {
 public:
  class Item {
   public:
    Item(const char* key, uint32_t length, T value) : key_(key),
                                                      length_(length),
                                                      value_(value),
                                                      next_(NULL) {
    }

    const char* key_;
    uint32_t length_;
    T value_;

    Item* next_;
  };

  HashMap() {
    memset(&map_, 0, sizeof(map_));
  }

  static uint32_t Hash(const char* key, uint32_t length) {
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

  void Set(const char* key, uint32_t length, T value) {
    uint32_t index = Hash(key) & size_;
    Item* i = map_[index];
    Item* next = new Item(key, length, value);

    if (i == NULL) {
      map_[index] = next;
    } else {
      while (i->next_ != NULL) i = i->next_;
      i->next_ = next;
    }
  }

  T Get(const char* key, uint32_t length) {
    uint32_t index = Hash(key) & size_;
    Item* i = map_[index];

    while (i != NULL) {
      if (length == i->length_ &&
          strncmp(i->key_, key, length) == 0) {
        return i->value_;
      }
      i = i->next_;
    }

    return NULL;
  }

 private:
  static const uint32_t size_ = 64;
  Item* map_[size_];
};

} // namespace dotlang

#endif // _SRC_UTILS_H_
