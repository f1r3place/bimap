#pragma once

#include "set.h"

#include <cstddef>
#include <stdexcept>
#include <type_traits>

template <
    typename Left,
    typename Right,
    typename CompareLeft = std::less<Left>,
    typename CompareRight = std::less<Right>>
class bimap
    : public map_detail::set_over<Left, map_detail::left, CompareLeft>
    , public map_detail::set_over<Right, map_detail::right, CompareRight> {
public:
  using left_t = Left;
  using right_t = Right;

  using left_set = map_detail::set_over<left_t, map_detail::left, CompareLeft>;
  using right_set = map_detail::set_over<right_t, map_detail::right, CompareRight>;

  using node_t = map_detail::node_base;
  using binode_t = map_detail::binode<left_t, right_t>;
  using left_node_t = map_detail::node_over<left_t, map_detail::left>;
  using right_node_t = map_detail::node_over<right_t, map_detail::right>;

  template <typename Over>
  class iterator_base {
    template <typename, typename, typename, typename>
    friend class bimap;

    using T = std::conditional_t<std::same_as<Over, map_detail::left>, left_t, right_t>;

  public:
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using reference = const T&;
    using pointer = const T*;
    using iterator_category = std::bidirectional_iterator_tag;

    using opposite_type = std::conditional_t<std::same_as<T, left_t>, right_t, left_t>;
    using opposite_over = std::conditional_t<std::same_as<Over, map_detail::left>, map_detail::right, map_detail::left>;
    using main_set = std::conditional_t<std::same_as<Over, map_detail::left>, left_set, right_set>;

    iterator_base() noexcept
        : _n(nullptr) {}

    reference operator*() const noexcept {
      return main_set::get_over(_n);
    }

    pointer operator->() const noexcept {
      return &main_set::get_over(_n);
    }

    iterator_base& operator++() noexcept {
      _n = main_set::next_node(_n);
      return *this;
    }

    iterator_base operator++(int) noexcept {
      iterator_base tmp(_n);
      ++*this;
      return tmp;
    }

    iterator_base& operator--() noexcept {
      _n = main_set::prev_node(_n);
      return *this;
    }

    iterator_base operator--(int) noexcept {
      iterator_base tmp(*this);
      --*this;
      return tmp;
    }

    iterator_base<opposite_over> flip() const noexcept {
      return main_set::get_service(_n)
               ? main_set::get_service(_n)
               : static_cast<map_detail::node_over<opposite_type, opposite_over>*>(
                     static_cast<map_detail::binode<left_t, right_t>*>(static_cast<map_detail::node_over<T, Over>*>(_n))
                 );
    }

    friend bool operator==(const iterator_base&, const iterator_base&) noexcept = default;

    friend bool operator!=(const iterator_base&, const iterator_base&) noexcept = default;

  private:
    iterator_base(node_t* n) noexcept
        : _n(n) {}

    node_t* _n;
  };

  using left_iterator = iterator_base<map_detail::left>;
  using right_iterator = iterator_base<map_detail::right>;

public:
  bimap(CompareLeft compare_left = CompareLeft(), CompareRight compare_right = CompareRight())
      : left_set(std::move(compare_left))
      , right_set(std::move(compare_right))
      , _size(0) {
    link_sets();
  }

  bimap(const bimap& other)
      : bimap(other.left_set::comparator, other.right_set::comparator) {
    for (left_iterator i = other.begin_left(); i != other.end_left(); i++) {
      insert(*i, *(i.flip()));
    }
  }

  bimap(bimap&& other) noexcept
      : left_set(std::move(other))
      , right_set(std::move(other))
      , _size(other._size) {
    link_sets();
    other._size = 0;
  }

  bimap& operator=(const bimap& other) {
    if (this == &other) {
      return *this;
    }
    bimap tmp(other);
    swap(*this, tmp);
    return *this;
  }

  bimap& operator=(bimap&& other) noexcept {
    if (this == &other) {
      return *this;
    }
    bimap tmp(std::move(other));
    swap(tmp, *this);
    return *this;
  }

  ~bimap() {
    erase_left(begin_left(), end_left());
  }

  friend void swap(bimap& lhs, bimap& rhs) {
    lhs.left_set::swap_set(rhs);
    lhs.right_set::swap_set(rhs);
    lhs.link_sets();
    rhs.link_sets();
    std::swap(lhs._size, rhs._size);
  }

  left_iterator insert(const left_t& left, const right_t& right) {
    return forwarding_insert(left, right);
  }

  left_iterator insert(const left_t& left, right_t&& right) {
    return forwarding_insert(left, std::move(right));
  }

  left_iterator insert(left_t&& left, const right_t& right) {
    return forwarding_insert(std::move(left), right);
  }

  left_iterator insert(left_t&& left, right_t&& right) {
    return forwarding_insert(std::move(left), std::move(right));
  }

  left_iterator erase_left(left_iterator it) noexcept {
    right_set::erase(it.flip()._n);
    left_iterator ret = left_set::erase(it._n);
    delete static_cast<binode_t*>(static_cast<left_node_t*>(it._n));
    _size--;
    return ret;
  }

  right_iterator erase_right(right_iterator it) noexcept {
    left_set::erase(it.flip()._n);
    right_iterator ret = right_set::erase(it._n);
    delete static_cast<binode_t*>(static_cast<right_node_t*>(it._n));
    _size--;
    return ret;
  }

  bool erase_left(const left_t& left) {
    left_iterator found = find_left(left);
    if (found == end_left()) {
      return false;
    }
    erase_left(found);
    return true;
  }

  bool erase_right(const right_t& right) {
    right_iterator found = find_right(right);
    if (found == end_right()) {
      return false;
    }
    erase_right(found);
    return true;
  }

  left_iterator erase_left(left_iterator first, left_iterator last) noexcept {
    for (left_iterator i = first; i != last;) {
      i = erase_left(i);
    }
    return last;
  }

  right_iterator erase_right(right_iterator first, right_iterator last) noexcept {
    for (right_iterator i = first; i != last;) {
      i = erase_right(i);
    }
    return last;
  }

  left_iterator find_left(const left_t& left) const {
    return left_set::find(left);
  }

  right_iterator find_right(const right_t& right) const {
    return right_set::find(right);
  }

  const right_t& at_left(const left_t& key) const {
    left_iterator l = find_left(key);
    if (l == end_left()) {
      throw std::out_of_range("invalid key");
    }
    return *(l.flip());
  }

  const left_t& at_right(const right_t& key) const {
    right_iterator r = find_right(key);
    if (r == end_right()) {
      throw std::out_of_range("invalid key");
    }
    return *(r.flip());
  }

  const right_t& at_left_or_default(const left_t& key)
    requires (std::is_default_constructible_v<right_t>)
  {
    left_iterator k = find_left(key);
    if (k != end_left()) {
      return *(k.flip());
    }
    right_t def = right_t();
    right_iterator found = find_right(def);
    if (found != end_right()) {
      // insert directly to bypass duplicate key check
      auto it = forwarding_insert(key, std::move(def), true);
      erase_right(found);
      return *(it.flip());
    } else {
      return *(insert(key, std::move(def)).flip());
    }
  }

  const left_t& at_right_or_default(const right_t& key)
    requires (std::is_default_constructible_v<left_t>)
  {
    right_iterator k = find_right(key);
    if (k != end_right()) {
      return *(k.flip());
    }
    left_t def = left_t();
    left_iterator found = find_left(def);
    if (found != end_left()) {
      auto it = forwarding_insert(std::move(def), key, true);
      erase_left(found);
      return *it;
    } else {
      return *insert(std::move(def), key);
    }
  }

  left_iterator lower_bound_left(const left_t& left) const {
    return left_set::lower_bound(left);
  }

  left_iterator upper_bound_left(const left_t& left) const {
    return left_set::upper_bound(left);
  }

  right_iterator lower_bound_right(const right_t& right) const {
    return right_set::lower_bound(right);
  }

  right_iterator upper_bound_right(const right_t& right) const {
    return right_set::upper_bound(right);
  }

  left_iterator begin_left() const noexcept {
    return left_set::begin();
  }

  left_iterator end_left() const noexcept {
    return left_set::end();
  }

  right_iterator begin_right() const noexcept {
    return right_set::begin();
  }

  right_iterator end_right() const noexcept {
    return right_set::end();
  }

  bool empty() const noexcept {
    return left_set::empty();
  }

  std::size_t size() const noexcept {
    return _size;
  }

  friend bool operator==(const bimap& lhs, const bimap& rhs) {
    if (lhs.size() != rhs.size()) {
      return false;
    }
    left_iterator lhs_i = lhs.begin_left(), rhs_i = rhs.begin_left();
    while (lhs_i != lhs.end_left()) {
      if (!(lhs.left_equals(*lhs_i, *rhs_i) && (lhs.right_equals(*(lhs_i.flip()), *(rhs_i.flip()))))) {
        return false;
      }

      lhs_i++;
      rhs_i++;
    }
    return true;
  }

  friend bool operator!=(const bimap& lhs, const bimap& rhs) = default;

private:
  bool left_equals(const left_t& lhs, const left_t& rhs) const {
    return (!left_set::comparator(lhs, rhs) && !left_set::comparator(rhs, lhs));
  }

  bool right_equals(const right_t& lhs, const right_t& rhs) const {
    return (!right_set::comparator(lhs, rhs) && !right_set::comparator(rhs, lhs));
  }

  template <typename L1, typename R1>
  left_iterator forwarding_insert(L1&& left, R1&& right, bool direct = false) {
    if (!check(left, right) && !direct) {
      return end_left();
    }
    map_detail::node_placement left_np = left_set::find_place(left);
    map_detail::node_placement right_np = right_set::find_place(right);
    binode_t* n = new binode_t(std::forward<L1>(left), std::forward<R1>(right));

    // nothrow from this point
    left_set::place_node(left_np, static_cast<left_node_t*>(n));
    right_set::place_node(right_np, static_cast<right_node_t*>(n));
    _size++;

    return static_cast<left_node_t*>(n);
  }

  bool check(const left_t& left, const right_t& right) {
    return left_set::find(left) == left_set::end() && right_set::find(right) == right_set::end();
  }

  void link_sets() noexcept {
    left_set::set_service(&(right_set::sentinel));
    right_set::set_service(&(left_set::sentinel));
  }

  std::size_t _size;
};
