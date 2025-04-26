#pragma once

#include <memory>

template <typename Left, typename Right, typename CompareLeft, typename CompareRight>
class bimap;

namespace map_detail {
struct left {};

struct right {};

template <typename K, typename Over, typename Compare>
class set_over;

class node_base {
  template <typename, typename, typename>
  friend class set_over;

public:
  node_base() = default;

  node_base(node_base&& other) noexcept = default;

  node_base(const node_base& other) noexcept = default;

  void clear() noexcept {
    _left = _right = _parent = nullptr;
  }

  node_base* leftmost() noexcept {
    node_base* n = this;
    if (n == nullptr) {
      return nullptr;
    }
    while (n->_left != nullptr) {
      n = n->_left;
    }
    return n;
  }

  node_base* rightmost() noexcept {
    node_base* n = this;
    if (n == nullptr) {
      return nullptr;
    }
    while (n->_right != nullptr) {
      n = n->_right;
    }
    return n;
  }

private:
  node_base* _left = nullptr;
  node_base* _right = nullptr;
  node_base* _parent = nullptr;
};

template <typename V, typename Over>
class node_over : public node_base {
  template <typename, typename, typename, typename>
  friend class ::bimap;
  template <typename, typename, typename>
  friend class set_over;

public:
  template <typename U>
  explicit node_over(U&& v)
      : value(std::forward<U>(v)) {}

private:
  V value;
};

template <typename V1, typename V2>
class binode
    : public node_over<V1, left>
    , public node_over<V2, right> {
public:
  template <typename U1, typename U2>
  binode(U1&& one, U2&& two)
      : node_over<V1, left>(std::forward<U1>(one))
      , node_over<V2, right>(std::forward<U2>(two)) {}
};

struct node_placement {
  node_base* attach_to;
  node_base** place;
};

// set helper implementation that bimap derives from
// doesn't take ownership of the nodes
// by itself does not assume it's part of a specific class (i.e bimap)
// hence the mentions of "service data"
template <typename K, typename Over, typename Compare>
class set_over {
  template <typename, typename, typename, typename>
  friend class ::bimap;

  using node = node_base;

public:
  set_over() = default;

  explicit set_over(Compare&& compare)
      : comparator(std::move(compare)) {}

  // because it doesn't take ownership of the nodes, it can't really copy them
  set_over(const set_over& other) = delete;

  set_over(set_over&& other) noexcept
      : comparator(std::move(other.comparator)) {
    node_base* tmp = other.sentinel._left;
    swap_nodes(&sentinel, &other.sentinel);
    other.sentinel.clear();
    // service data is still valid in a moved from set
    other.sentinel._left = tmp;
  }

  // node helper functions that correlate directly
  // to set invariant
  static K& get_over(node* n) noexcept {
    return static_cast<node_over<K, Over>*>(n)->value;
  }

  static bool is_sentinel(node* n) noexcept {
    return n->_parent == nullptr;
  }

  static node* next_node(node* n) noexcept {
    if (n->_right) {
      return n->_right->leftmost();
    } else {
      while (!(is_sentinel(n->_parent)) && n == n->_parent->_right) {
        n = n->_parent;
      }
      return n->_parent;
    }
  }

  static node* prev_node(node* n) noexcept {
    if (is_sentinel(n)) {
      return n->_right->rightmost();
    }
    if (n->_left) {
      return n->_left->rightmost();
    } else {
      while (!is_sentinel(n->_parent) && n == n->_parent->_left) {
        n = n->_parent;
      }
      return n->_parent;
    }
  }

  static node* get_service(node* n) noexcept {
    if (is_sentinel(n)) {
      return n->_left;
    }
    return nullptr;
  }

  static node* place_node(const node_placement& placement, node* n) noexcept {
    n->_parent = placement.attach_to;
    *(placement.place) = n;
    return n;
  }

  node_placement find_place(const K& key) {
    if (sentinel._right == nullptr) {
      return {&sentinel, &sentinel._right};
    }

    node* p = nullptr;
    node* m = root();
    while (m != nullptr) {
      p = m;
      if (comparator(key, get_over(m))) {
        m = m->_left;
      } else {
        m = m->_right;
      }
    }

    if (comparator(key, get_over(p))) {
      return {p, &p->_left};
    }
    return {p, &p->_right};
  }

  node* root() const noexcept {
    return sentinel._right;
  }

  static void replace_parent(node* to, node* with) noexcept {
    if (to->_parent->_left == to) {
      to->_parent->_left = with;
    } else {
      to->_parent->_right = with;
    }
  }

  static void reparent(node* old_p, node* new_p) noexcept {
    if (old_p->_left && !is_sentinel(new_p) && !is_sentinel(old_p)) {
      old_p->_left->_parent = new_p;
    }
    if (old_p->_right) {
      old_p->_right->_parent = new_p;
    }
  }

  static void swap_nodes(node* n1, node* n2) noexcept {
    node* n1_parent = n2->_parent;
    node* n2_parent = n1->_parent;
    node* n1_left = n2->_left;
    node* n2_left = n1->_left;
    node* n1_right = n2->_right;
    node* n2_right = n1->_right;

    if (n2 == n1->_parent) {
      n2_parent = n1;
      if (n2->_left == n1) {
        n1_left = n2;
      } else {
        n1_right = n2;
      }

      replace_parent(n2, n1);
    } else if (n1 == n2->_parent) {
      n1_parent = n2;
      if (n1->_left == n2) {
        n2_left = n1;
      } else {
        n2_right = n1;
      }

      replace_parent(n1, n2);
    } else if (n1->_parent && n2->_parent) {
      replace_parent(n1, n2);
      replace_parent(n2, n1);
    }

    reparent(n1, n2);
    reparent(n2, n1);

    n1->_parent = n1_parent;
    n1->_left = n1_left;
    n1->_right = n1_right;
    n2->_parent = n2_parent;
    n2->_left = n2_left;
    n2->_right = n2_right;
  }

  void swap_set(set_over& other) noexcept {
    std::swap(comparator, other.comparator);
    swap_nodes(&sentinel, &other.sentinel);
  }

  static node* erase(node* n) noexcept {
    // no children
    node* nxt = next_node(n);
    node** placement = n->_parent->_left == n ? &(n->_parent->_left) : &(n->_parent->_right);
    if (!n->_left && !n->_right) {
      *placement = nullptr;
      return nxt;
    }

    // one child
    if (!(n->_left && n->_right)) {
      node* replacer = n->_left ? n->_left : n->_right;
      *placement = replacer;

      replacer->_parent = n->_parent;
      return nxt;
    }

    // both children
    swap_nodes(n, next_node(n));
    erase(n);
    return nxt;
  }

  node* begin() const noexcept {
    if (!root()) {
      // workaround because gcc in one specific case believes
      // this function returns end() and then gets dereferenced
      // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=106446
      // apparently this is intended but that means
      // this whole node design is flawed
      return end()->_left->_left;
    }
    return root()->leftmost();
  }

  node* end() const noexcept {
    return const_cast<node*>(&sentinel);
  }

  node* find(const K& key) const {
    node* found = lower_bound(key);
    if (found == end() || !(comparator(get_over(found), key) || comparator(key, get_over(found)))) {
      return found;
    }
    return end();
  }

  node* lower_bound(const K& key) const {
    node* n = root();
    node* last_left = nullptr;
    while (n != nullptr) {
      if (comparator(key, get_over(n))) {
        last_left = n;
        n = n->_left;
      } else if (comparator(get_over(n), key)) {
        n = n->_right;
      } else {
        break;
      }
    }
    if (n) {
      return n;
    }
    return last_left ? last_left : end();
  }

  node* upper_bound(const K& key) const {
    node* n = lower_bound(key);
    if (n == end()) {
      return end();
    }
    if (get_over(n) == key) {
      return next_node(n);
    }
    return n;
  }

  bool empty() const noexcept {
    return sentinel._right == nullptr;
  }

  void set_service(node* n) noexcept {
    sentinel._left = n;
  }

private:
  // sentinel:
  // _parent == nullptr;
  // _left == service data
  // _right == root_node;
  node sentinel;
  [[no_unique_address]] Compare comparator;
};
} // namespace map_detail
