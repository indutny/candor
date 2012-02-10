#ifndef _SRC_UTILS_H_
#define _SRC_UTILS_H_

#include <stdlib.h> // NULL
#include <stdint.h> // uint32_t

template <class T>
class List {
 public:
  class Item {
   public:
    Item(T value_) : value(value_), prev(NULL), next(NULL) {
    }

    T value;
    Item* prev;
    Item* next;
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
    next->prev = current;
    if (current != NULL) current->next = next;
    if (head == NULL) head = next;

    current = next;
    length++;
  }

  void Unshift(T item) {
    Item* next = new Item(item);
    next->prev = NULL;
    next->next = head;
    head = next;
  }

  T Shift() {
    if (head == NULL) return NULL;

    Item* tmp = head;
    T value = head->value;

    if (head == current) current = NULL;
    if (head->next != NULL) head->next->prev = NULL;

    head = head->next;
    delete tmp;
    length--;

    return value;
  }

  Item* Head() {
    return head;
  }

  Item* Next(Item* item) {
    return item->next;
  }

  Item* Prev(Item* item) {
    return item->prev;
  }

  uint32_t Length() {
    return length;
  }

  Item* head;
  Item* current;
  uint32_t length;

  bool allocated;
};

#endif // _SRC_UTILS_H_
