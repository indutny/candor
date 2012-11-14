#ifndef _SRC_UTILS_H_
#define _SRC_UTILS_H_

#include <stdlib.h> // NULL
#include <stdarg.h> // va_list
#include <stdint.h> // uint32_t
#include <stdio.h> // fprintf, vsnprintf
#include <string.h> // strncmp, memset
#include <unistd.h> // sysconf or getpagesize, intptr_t
#include <assert.h> // assert

namespace candor {
namespace internal {

#define UNEXPECTED { assert(0 && "Unexpected"); abort(); }

inline uint32_t ComputeHash(int64_t key) {
  uint32_t hash = 0;

  // high
  hash += key >> 32;
  hash += (hash << 10);
  hash ^= (hash >> 6);

  // low
  hash += key & 0xffffffff;
  hash += (hash << 10);
  hash ^= (hash >> 6);

  // mixup
  hash += (hash << 3);
  hash ^= (hash >> 11);
  hash += (hash << 15);

  return hash;
}

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


class EmptyClass { };

template <class T, class ItemParent>
class Stack {
 public:
  class Item : public ItemParent {
   public:
    Item(T value, Item* prev) : value_(value), prev_(prev) {
    }

    inline T value() { return value_; }
    inline void value(T value) { value_ = value; }

    inline Item* prev() { return prev_; }

   protected:
    T value_;
    Item* prev_;
  };

  Stack() : allocated(false), head_(NULL) {
  }

  ~Stack() {
    if (allocated) {
      T i;
      while ((i = Pop()) != NULL) delete i;
    } else {
      while (Pop() != NULL) {
      }
    }
  }

  inline void Push(T value) {
    head_ = new Item(value, head());
  }

  inline T Pop() {
    if (head() == NULL) return NULL;

    Item* current = head();
    head_ = current->prev();

    delete current;

    return current->value();
  }

  inline Item* head() { return head_; }

  bool allocated;

 private:
  Item* head_;
};

class NopPolicy {
 public:
  static inline void Delete(void* value) {}
};

template <class T>
class DeletePolicy {
 public:
  static inline void Delete(T value) {
    delete value;
  }
};

template <class T, class ItemParent, class Policy>
class GenericList {
 public:
  class Item : public ItemParent {
   public:
    Item(T value) : value_(value), prev_(NULL), next_(NULL) {
    }

    inline T value() { return value_; }
    inline void value(T value) { value_ = value; }

    inline Item* prev() { return prev_; }
    inline Item* next() { return next_; }
    inline void prev(Item* prev) { prev_ = prev; }
    inline void next(Item* next) { next_ = next; }

    inline void remove() {
      if (prev_ != NULL) prev_->next_ = next_;
      if (next_ != NULL) next_->prev_ = prev_;
    }

   protected:
    T value_;
    Item* prev_;
    Item* next_;

    friend class GenericList;
  };

  GenericList() : head_(NULL), tail_(NULL), length_(0) {
  }


  ~GenericList() {
    while (length() > 0) Policy::Delete(Shift());
  }


  inline void Push(T item) {
    Item* next = new Item(item);
    next->prev_ = tail_;

    if (head_ == NULL) {
      head_ = next;
    } else if (tail_ != NULL) {
      tail_->next_ = next;
    }

    tail_ = next;
    length_++;
  }


  inline void Remove(Item* item) {
    if (tail_ == item) tail_ = item->prev_;
    if (head_ == item) head_ = item->next_;
    Policy::Delete(item->value());

    item->remove();
    delete item;

    length_--;
  }


  inline void Unshift(T item) {
    Item* next = new Item(item);

    if (head_ != NULL) {
      head_->prev_ = next;
    }
    next->prev_ = NULL;
    next->next_ = head_;
    head_ = next;
    if (tail_ == NULL) tail_ = head_;
    length_++;
  }


  inline void InsertBefore(Item* next, T value) {
    Item* item = new Item(value);

    // `value` is a new head of list
    if (next->prev() == NULL) return Unshift(value);

    next->prev()->next(item);
    item->prev(next->prev());
    item->next(next);
    next->prev(item);

    // Increase length
    length_++;
  }


  // Sorted insertion
  template <class Shape>
  inline void InsertSorted(T value) {
    if (head() == NULL) return Push(value);

    Item* current = tail();
    Item* insert_node = NULL;
    while (current != NULL && Shape::Compare(value, current->value()) < 0) {
      insert_node = current;
      current = current->prev();
    }

    // `value` is a new tail of list
    if (insert_node == NULL) return Push(value);

    // Insert before `insert_node`
    InsertBefore(insert_node, value);
  }


  inline T Pop() {
    if (tail_ == NULL) return NULL;

    Item* tmp = tail_;
    T value = tail_->value();

    if (tail_ == head_) head_ = NULL;
    if (tail_->prev_ != NULL) tail_->prev()->next_ = NULL;

    tail_ = tail_->prev();
    delete tmp;
    length_--;

    return value;
  }


  inline T Shift() {
    if (head_ == NULL) return NULL;

    Item* tmp = head_;
    T value = head_->value();

    if (head_ == tail_) tail_ = NULL;
    if (head_->next_ != NULL) head_->next()->prev_ = NULL;

    head_ = head_->next();
    delete tmp;
    length_--;

    return value;
  }


  // Quicksort
  template <class Shape>
  inline void Sort() {
    // XXX: Replace it with qsort eventually
    Item* a;
    Item* b;
    for (a = head(); a != NULL; a = a->next()) {
      for (b = a->next(); b != NULL; b = b->next()) {
        if (Shape::Compare(a->value(), b->value()) > 0) {
          T tmp = a->value();
          a->value(b->value());
          b->value(tmp);
        }
      }
    }
  }


  inline Item* head() { return head_; }
  inline Item* tail() { return tail_; }
  inline int32_t length() { return length_; }

 private:
  Item* head_;
  Item* tail_;
  int32_t length_;
};


template <class T, class I>
class List : public GenericList<T, I, DeletePolicy<T> > {
 public:
};


template <class Base>
class StringKey : public Base {
 public:
  StringKey(const char* value, uint32_t length) : value_(value),
                                                  length_(length) {
  }

  static inline uint32_t Hash(StringKey* key) {
    return ComputeHash(key->value(), key->length());
  }

  static inline bool IsEqual(StringKey* left, StringKey* right) {
    return Compare(left, right) == 0;
  }

  static inline int Compare(StringKey* left, StringKey* right) {
    if (left->length() != right->length()) return - 1;
    return strncmp(left->value(), right->value(), left->length());
  }

  inline const char* value() { return value_; }
  inline uint32_t length() { return length_; }

 private:
  const char* value_;
  uint32_t length_;
};


template <class Key, class Value, class ItemParent, class Policy>
class GenericHashMap {
 public:
  typedef void (*EnumerateCallback)(void* map, Value* value);

  class Item : public ItemParent {
   public:
    Item(Key* key, Value* value) : key_(key),
                                   value_(value),
                                   prev_(NULL),
                                   next_(NULL),
                                   prev_scalar_(NULL),
                                   next_scalar_(NULL) {
    }

    inline Key* key() { return key_; }
    inline Value* value() { return value_; }
    inline void value(Value* value) { value_ = value; }
    inline Item* prev() { return prev_; }
    inline Item* next() { return next_; }
    inline Item* prev_scalar() { return prev_scalar_; }
    inline Item* next_scalar() { return next_scalar_; }

   protected:
    Key* key_;
    Value* value_;

    Item* prev_;
    Item* next_;
    Item* prev_scalar_;
    Item* next_scalar_;

    friend class GenericHashMap;
  };

  GenericHashMap() : head_(NULL), current_(NULL) {
    memset(&map_, 0, sizeof(map_));
  }

  ~GenericHashMap() {
    Item* i = head_;

    while (i != NULL) {
      Item* prev = i;
      Value* value = i->value();

      i = i->next_scalar();

      delete prev;
      Policy::Delete(value);
    }
  }

  inline void Set(Key* key, Value* value) {
    uint32_t index = Key::Hash(key) & mask_;
    Item* i = map_[index];
    Item* next = NULL;

    if (i == NULL) {
      next = new Item(key, value);
      map_[index] = next;
    } else {
      while (!Key::IsEqual(i->key_, key) && i->next() != NULL) {
        i = i->next();
      }

      // Overwrite key
      if (Key::IsEqual(i->key_, key)) {
        i->value_ = value;
      } else {
        next = new Item(key, value);

        assert(i->next_ == NULL);
        i->next_ = next;
        next->prev_ = i;
      }
    }

    // In case of overwrite value already present in scalar list
    if (next == NULL) return;

    // Setup head or append item to linked list
    // (Needed for enumeration)
    if (head_ == NULL) {
      head_ = next;
    } else {
      current_->next_scalar_ = next;
      next->prev_scalar_ = current_;
    }
    current_ = next;
  }


  inline Value* Get(Key* key) {
    uint32_t index = Key::Hash(key) & mask_;
    Item* i = map_[index];

    while (i != NULL) {
      if (Key::IsEqual(i->key_, key)) {
        return i->value();
      }
      i = i->next();
    }

    return NULL;
  }


  inline void RemoveOne(Key* key) {
    uint32_t index = Key::Hash(key) & mask_;
    Item* i = map_[index];

    while (i != NULL) {
      if (Key::IsEqual(i->key_, key)) {
        // Remove item from linked lists
        if (i->prev_scalar() != NULL) {
          i->prev_scalar()->next_scalar_ = i->next_scalar();
        }
        if (i->next_scalar() != NULL) {
          i->next_scalar()->prev_scalar_ = i->prev_scalar();
        }
        if (i->prev() != NULL) i->prev()->next_ = i->next();
        if (i->next() != NULL) i->next()->prev_ = i->prev();
        if (i == head_) head_ = i->next_scalar();
        if (i == current_) {
          current_ = i->prev_scalar();
          if (current_ == NULL) current_ = head_;
        }

        // Replace item in map if it was the first with such index
        if (i->prev() == NULL) {
          assert(map_[index] == i);
          map_[index] = i->next();
        }

        // Remove any allocated data
        Policy::Delete(i->value());
        delete i;

        return;
      }
      i = i->next();
    }
  }


  inline void Enumerate(EnumerateCallback cb) {
    Item* i = head_;

    while (i != NULL) {
      cb(this, i->value());
      i = i->next_scalar();
    }
  }

  inline Item* head() { return head_; }

 private:
  static const uint32_t size_ = 32;
  static const uint32_t mask_ = 31;
  Item* map_[size_];
  Item* head_;
  Item* current_;
};


template <class Key, class Value, class ItemParent>
class HashMap : public GenericHashMap<Key,
                                      Value,
                                      ItemParent,
                                      DeletePolicy<Value*> > {
 public:
};

template <class Key, class Value, class ItemParent>
class ZoneMap : public GenericHashMap<Key, Value, ItemParent, NopPolicy> {
 public:
};

class NumberKey {
 public:
  static inline NumberKey* New(const intptr_t value) {
    return reinterpret_cast<NumberKey*>(value);
  }

  static inline NumberKey* New(void* value) {
    return reinterpret_cast<NumberKey*>(value);
  }

  intptr_t inline value() {
    return reinterpret_cast<intptr_t>(this);
  }

  static inline uint32_t Hash(NumberKey* key) {
    return ComputeHash(key->value());
  }

  static inline bool IsEqual(NumberKey* left, NumberKey* right) {
    return left == right;
  }

  static inline int Compare(NumberKey* left, NumberKey* right) {
    intptr_t l = reinterpret_cast<intptr_t>(left);
    intptr_t r = reinterpret_cast<intptr_t>(right);
    return l == r ? 0 : l > r ? 1 : - 1;
  }
};


template <class Key, class Value, class BaseItem>
class AVLTree {
 public:
  class Item : public BaseItem {
   public:
    Item(Key* key) : balance_(0),
                     parent_(NULL),
                     left_(NULL),
                     right_(NULL),
                     key_(key),
                     value_(NULL) {
    }

    inline int balance() { return balance_; }
    inline void inc_balance() { balance_++; }
    inline void dec_balance() { balance_--; }

    inline Item* parent() { return parent_; }
    inline Item* left() { return left_; }
    inline Item* right() { return right_; }
    inline void parent(Item* parent) { parent_ = parent; }
    inline void left(Item* left) {
      left_ = left;
      if (left != NULL) left->parent(this);
    }
    inline void right(Item* right) {
      right_ = right;
      if (right != NULL) right->parent(this);
    }

    inline Key* key() { return key_; }
    inline Value* value() { return value_; }
    inline void key(Key* key) { key_ = key; }
    inline void value(Value* value) { value_ = value; }

   private:
    int balance_;
    Item* parent_;
    Item* left_;
    Item* right_;

    Key* key_;
    Value* value_;
  };

  AVLTree() : allocated(false), head_(NULL) {
  }


  ~AVLTree() {
    List<Item*, EmptyClass> queue;

    queue.Push(head());
    Item* item;
    while ((item = queue.Shift()) != NULL) {
      if (item->left() != NULL) queue.Push(item->left());
      if (item->right() != NULL) queue.Push(item->right());

      if (allocated) delete item->value();
      delete item;
    }
  }

  inline void Insert(Key* key, Value* value) {
    // Fast case - empty tree
    if (head() == NULL) {
      Item* item = new Item(key);
      item->value(value);
      head(item);
      return;
    }

    // Slow case - insert node into the tree
    Item* match = Search(key, true);
    if (match == NULL) {
      if (allocated) delete value;
      return;
    }
    match->value(value);

    // Go up and preserve balance
    while (match != NULL) {
      if (match->balance() == 2) {
        switch (match->left()->balance()) {
         case -1:
          // Left Right
          {
            Item* left = match->left();
            match->left(left->right());
            left->right(match->left()->left());
            match->left()->left(left);

            left->inc_balance();
            match->left()->inc_balance();
          }
         case 1:
          // Left left
          {
            Item* left = match->left();
            match->left(left->right());
            left->right(match);

            left->parent(match->parent());
            match->parent(left);

            match = left;

            match->dec_balance();
            match->right()->dec_balance();
          }
         default:
          break;
        }
      } else if (match->balance() == -2) {
        switch (match->left()->balance()) {
         case 1:
          // Right Left
          {
            Item* right = match->right();
            match->right(right->left());
            right->left(match->right()->right());
            match->right()->right(right);

            right->dec_balance();
            match->right()->dec_balance();
          }
         case -1:
          // Right right
          {
            Item* right = match->right();
            match->right(right->left());
            right->left(match);

            right->parent(match->parent());
            match->parent(right);

            match = right;

            match->inc_balance();
            match->left()->inc_balance();
          }
         default:
          break;
        }
      }

      match = match->parent();
    }
  }

  inline Item* Search(Key* key, bool insert) {
    Item* current = NULL;
    Item* next = head();
    Item* new_node = NULL;

    if (insert) {
      new_node = new Item(key);
    }

    while (next != NULL) {
      current = next;
      next = NULL;

      int cmp = Key::Compare(key, current->key());
      if (cmp == 0) {
        // Can't insert same value twice
        if (insert) {
          delete new_node;
          return NULL;
        }

        // Do nothing to leave loop
      } else if (cmp < 0) {
        // key < current->key()
        next = current->left();
        if (next == NULL && insert) {
          current->left(new_node);
          current->inc_balance();
        }
      } else if (cmp > 0) {
        // key > current->key()
        next = current->right();
        if (next == NULL && insert) {
          current->right(new_node);
          current->dec_balance();
        }
      }
    }

    if (insert) {
      return new_node;
    } else {
      // Find more appropriate node (with key less than asked for)
      while (current->parent() != NULL &&
             Key::Compare(key, current->key()) < 0) {
        current = current->parent();
      }
    }

    return current;
  }

  inline Value* Get(Key* key) {
    return Search(key, false)->value();
  }

  inline Item* head() { return head_; }
  inline void head(Item* head) { head_ = head; }

  bool allocated;

 private:
  Item* head_;
};


template <class T, int size>
class FreeList {
 public:
  FreeList() : length_(0) {
  }

  inline bool IsEmpty() { return length_ == 0; }
  inline T Get() { return list_[--length_]; }
  inline void Release(T value) {
    assert(!Has(value));
    list_[length_++] = value;
  }

  inline void Remove(T value) {
    for (int i = 0; i < length_; i++) {
      // TODO: Use Shape class here
      if (list_[i] != value) continue;

      // Shift all registers to the left
      for (int j = i + 1; j < length_; j++) {
        list_[j - 1] = list_[j];
      }

      // Decrement length
      length_--;
      return;
    }
  }

  inline bool Has(T value) {
    for (int i = 0; i < length_; i++) {
      // TODO: Use Shape class here
      if (list_[i] == value) return true;
    }

    return false;
  }

 private:
  T list_[size];
  int length_;
};

template <class Base>
class BitMap : public Base {
 public:
  BitMap(int size) : size_(size / 32) {
    space_ = new uint32_t[size_];
    memset(space_, 0, sizeof(*space_) * size_);
  }

  ~BitMap() {
    delete[] space_;
    space_ = NULL;
  }

  inline void Set(int key) {
    assert(key >= 0);

    // Grow map if needed
    Grow((key / 32) + 1);

    int index = key / 32;
    int pos = key % 32;
    uint32_t mask = 1;
    while (pos-- > 0) {
      mask <<= 1;
    }

    assert(size_ > index);
    space_[index] |= mask;
  }

  inline void Grow(int size) {
    if (size <= size_) return;

    // Create new space
    int new_size = RoundUp(size, 16);
    uint32_t* new_space = new uint32_t[new_size];

    // Copy old data in
    memcpy(new_space, space_, size_ * sizeof(*new_space));
    memset(new_space + size_, 0, sizeof(*new_space) * (new_size - size_));

    delete[] space_;
    space_ = new_space;
    size_ = new_size;
  }

  inline bool Test(int key) {
    if ((key / 32) >= size_) return false;

    int index = key / 32;
    int pos = key % 32;
    uint32_t mask = 1;
    while (pos-- > 0) {
      mask <<= 1;
    }

    assert(index < size_);
    return (space_[index] & mask) != 0;
  }

  inline bool Copy(BitMap<Base>* to) {
    bool change = false;
    to->Grow(size_);
    assert(to->size_ >= size_);
    for (int i = 0; i < size_; i++) {
      if ((to->space_[i] & space_[i]) != space_[i]) change = true;

      to->space_[i] |= space_[i];
    }
    return change;
  }

 protected:
  int size_;
  uint32_t* space_;
};

// For debug printing AST and other things
class PrintBuffer {
 public:
  PrintBuffer(char* buffer, int32_t size) : out_(NULL),
                                            buffer_(buffer),
                                            left_(size),
                                            total_(0) {
  }

  PrintBuffer(FILE* out) : out_(out) {
  }

  bool Print(const char* format, ...) {
    va_list arguments;
    if (out_ != NULL) {
      va_start(arguments, format);
      vfprintf(out_, format, arguments);
      va_end(arguments);
      return true;
    }

    va_start(arguments, format);
    int32_t written = vsnprintf(buffer_, left_, format, arguments);
    va_end(arguments);
    return Consume(written);
  }

  bool PrintValue(const char* value, int32_t size) {
    if (out_ != NULL) {
      fprintf(out_, "%.*s", size, value);
      return true;
    }

    Consume(size);
    if (ended()) return false;
    memcpy(buffer_ - size, value, size);

    return !ended();
  }

  bool Consume(int32_t bytes) {
    if (out_ != NULL) return true;

    buffer_ += bytes;
    total_ += bytes;
    left_ -= bytes;

    return !ended();
  }

  void Finalize() {
    if (out_ != NULL) return;
    if (ended()) return;
    buffer_[total_] = 0;
  }

  inline bool ended() { return left_ <= 0; }
  inline int32_t total() { return total_ <= 0; }

 private:
  FILE* out_;
  char* buffer_;
  int32_t left_;
  int32_t total_;
};

class ErrorHandler {
 public:
  ErrorHandler() : error_pos_(0), error_msg_(NULL) {}

  inline bool has_error() { return error_msg_ != NULL; }
  inline const char* error_msg() { return error_msg_; }
  inline int32_t error_pos() { return error_pos_; }

  inline void SetError(const char* msg, int offset) {
    if (msg == NULL) {
      error_msg_ = NULL;
      error_pos_ = 0;
    }

    if (error_pos_ < offset) {
      error_pos_ = offset;
      error_msg_ = msg;
    }
  }

 protected:
  int32_t error_pos_;
  const char* error_msg_;
};


// Naive only for lexer generated number strings
inline bool StringIsDouble(const char* value, uint32_t length) {
  for (uint32_t index = 0; index < length; index++) {
    if (value[index] == '.') return true;
  }

  return false;
}


inline uint32_t StringGetNumSign(const char* value, uint32_t length, bool* s) {
  uint32_t index = 0;
  // Skip spaces
  while (index < length && value[index] == ' ') index++;

  // Check if we found sign
  if (index < length && value[index] == '-') {
    *s = true;
    index++;
  }

  return index;
}


inline bool is_num(const char c) {
  unsigned char uc = c;

  return uc >= '0' && uc <= '9';
}


inline bool is_hex(const char c) {
  unsigned char uc = c;

  return is_num(c) || (uc >= 'a' && uc <= 'f') || (uc >= 'A' && uc <= 'F');
}


inline int hex_to_num(const char c) {
  switch (c) {
   case '0': return 0;
   case '1': return 1;
   case '2': return 2;
   case '3': return 3;
   case '4': return 4;
   case '5': return 5;
   case '6': return 6;
   case '7': return 7;
   case '8': return 8;
   case '9': return 9;
   case 'a': return 10;
   case 'b': return 11;
   case 'c': return 12;
   case 'd': return 13;
   case 'e': return 14;
   case 'f': return 15;
   case 'A': return 10;
   case 'B': return 11;
   case 'C': return 12;
   case 'D': return 13;
   case 'E': return 14;
   case 'F': return 15;
  }
  return 0;
}


inline int64_t StringToInt(const char* value, uint32_t length) {
  int64_t result = 0;
  bool sign = false;

  uint32_t index = StringGetNumSign(value, length, &sign);
  for (; index < length; index++) {
    if (!is_num(value[index])) break;
    result *= 10;
    result += value[index] - '0';
  }

  return sign ? -result : result;
}


inline double StringToDouble(const char* value, uint32_t length) {
  double integral = 0;
  double floating = 0;
  bool sign = false;

  uint32_t index = StringGetNumSign(value, length, &sign);
  for (; index < length; index++) {
    if (value[index] == '.') break;
    if (!is_num(value[index])) break;
    integral *= 10;
    integral += value[index] - '0';
  }

  if (index < length && value[index] == '.') {
    for (uint32_t i = length - 1; i > index; i--) {
      if (!is_num(value[i])) break;
      floating += value[i] - '0';
      floating /= 10;
    }
  }

  return sign ? -(integral + floating) : integral + floating;
}


inline int GetSourceLineByOffset(const char* source,
                                 uint32_t offset,
                                 int* pos) {
  int result = 1;
  uint32_t line_start = 0;

  for (uint32_t i = 0; i < offset; i++) {
    if (source[i] == '\r') {
      if (i + 1 < offset && source[i] == '\n') i++;
      result++;
      line_start = i;
    } else if (source[i] == '\n') {
      result++;
      line_start = i;
    }
  }

  *pos = offset - line_start;

  return result;
}


inline const char* Unescape(const char* value, uint32_t length, uint32_t* res) {
  char* result = new char[length];
  uint32_t offset = 0;
  for (uint32_t i = 0; i < length; i++) {
    if (value[i] == '\\') {
      i++;
      switch (value[i]) {
       case 'b': result[offset] = '\b'; break;
       case 'r': result[offset] = '\r'; break;
       case 'n': result[offset] = '\n'; break;
       case 't': result[offset] = '\t'; break;
       case 'v': result[offset] = '\v'; break;
       case '0': result[offset] = '\0'; break;
       case 'u':
        if (i + 4 < length && is_hex(value[i + 1]) && is_hex(value[i + 2]) &&
            is_hex(value[i + 3]) && is_hex(value[i + 4])) {
          result[offset] = (hex_to_num(value[i + 1]) << 4) +
                           hex_to_num(value[i + 2]);
          offset++;
          result[offset] = (hex_to_num(value[i + 3]) << 4) +
                           hex_to_num(value[i + 4]);
          i += 4;
        } else {
          result[offset] = value[i];
        }
        break;
       case 'x':
        if (i + 2 < length && is_hex(value[i + 1]) && is_hex(value[i + 2])) {
          result[offset] = (hex_to_num(value[i + 1]) << 4) +
                           hex_to_num(value[i + 2]);
          i += 2;
          break;
        }
       default: result[offset] = value[i]; break;
      }
    } else {
      result[offset] = value[i];
    }
    offset++;
  }

  *res = offset;
  return result;
}


inline uint32_t GetPageSize() {
#if CANDOR_PLATFORM_DARWIN
  return getpagesize();
#elif CANDOR_PLATFORM_LINUX
  return sysconf(_SC_PAGE_SIZE);
#endif
}

} // namespace internal
} // namespace candor

#endif // _SRC_UTILS_H_
