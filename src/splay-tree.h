#ifndef _SRC_SPLAY_TREE_H_
#define _SRC_SPLAY_TREE_H_

#include <stdlib.h>
#include <assert.h>

namespace candor {
namespace internal {

template <class Key, class Value, class Policy, class ItemBase>
class SplayTree {
 public:
  class Item : public ItemBase {
   public:
    Item() : value(NULL), parent(NULL), left(NULL), right(NULL) {}
    ~Item() {}

    Key* key;
    Value* value;

    Item* parent;
    Item* left;
    Item* right;
  };

  SplayTree() : root_(NULL) {
  }

  ~SplayTree() {
    Item* next = root_;
    while (next != NULL) {
      Item* current = next;

      // Visit left subtree first
      if (current->left != NULL) {
        next = current->left;
      } else if (current->right != NULL) {
        next = current->right;
      } else {
        next = current->parent;
      }

      if (current->left != NULL) {
        current->left->parent = current->right == NULL ? current->parent :
                                                         current->right;
      }
      if (current->right != NULL) current->right->parent = current->parent;
      current->parent = NULL;

      Policy::Delete(current->value);
      delete current;
    }
  }

  inline bool Insert(Key* key, Value* value) {
    Item* place = BinarySearch(key, true);
    assert(place != NULL);
    bool is_new = place->value == NULL;

    place->value = value;

    Splay(place);

    return is_new;
  }

  inline Value* Find(Key* key) {
    Item* place = BinarySearch(key, false);

    if (place != NULL) {
      Splay(place);
      return place->value;
    }

    return NULL;
  }

 private:
  inline Item* BinarySearch(Key* key, bool insert) {
    // Fast case - empty tree
    if (root_ == NULL) {
      if (!insert) return NULL;

      Item* result = new Item();
      result->key = key;
      root_ = result;

      return result;
    }

    Item* current;
    Item* next = root_;
    Item* last_positive = root_;
    int cmp;
    do {
      current = next;
      cmp = Key::Compare(key, current->key);
      if (cmp >= 0) {
        last_positive = current;
        next = cmp == 0 ? NULL : current->right;
      } else {
        next = current->left;
      }
      assert(next == NULL || next->parent == current);
    } while (next != NULL);

    // Item found - return it, or ..
    // Item not found - return closest item that's key < given key
    if (cmp == 0 || !insert) return last_positive;

    // Item not found, but insertion is requested
    assert(current != NULL);
    Item* result = new Item();
    result->key = key;
    result->parent = current;
    if (cmp > 0) {
      current->right = result;
    } else {
      current->left = result;
    }

    return result;
  }

  inline void RotateLeft(Item* p, Item* c) {
    p->left = c->right;
    if (c->right != NULL) c->right->parent = p;
    c->right = p;
    p->parent = c;
  }

  inline void RotateRight(Item* p, Item* c) {
    p->right = c->left;
    if (c->left != NULL) c->left->parent = p;
    c->left = p;
    p->parent = c;
  }

  inline void Splay(Item* x) {
    // x is already root - ignore
    if (x->parent == NULL) {
      assert(x == root_);
      return;
    }

    while (x != root_) {
      Item* p = x->parent;
      assert(p != NULL);
      if (p == root_) {
        // Zig Step
        if (p->left == x) {
          RotateLeft(p, x);
        } else {
          RotateRight(p, x);
        }
        x->parent = NULL;
        root_ = x;

        return;
      }

      Item* g = p->parent;
      assert(g != NULL);
      Item* gg = g->parent;
      if (x == p->left && p == g->left) {
        // Zig-zig both left
        RotateLeft(p, x);
        RotateLeft(g, p);
      } else if (x == p->right && p == g->right) {
        // Zig-zig both right
        RotateRight(p, x);
        RotateRight(g, p);
      } else {
        // Zig-zag
        if (x == p->left) {
          RotateLeft(p, x);
          RotateRight(g, x);
        } else {
          RotateRight(p, x);
          RotateLeft(g, x);
        }
      }

      // Update grand-grand parent
      if (gg == NULL) {
        root_ = x;
        x->parent = NULL;
      } else {
        x->parent = gg;
        if (gg->left == g) {
          gg->left = x;
        } else {
          gg->right = x;
        }
      }

      // Continue splaying
    }
  }

  Item* root_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_SPLAY_TREE_H_
