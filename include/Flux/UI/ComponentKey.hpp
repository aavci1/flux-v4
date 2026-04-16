#pragma once

/// \file Flux/UI/ComponentKey.hpp
///
/// Part of the Flux public API.

#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <utility>
#include <vector>

namespace flux {

/// Structural position of a composite component: sequence of child indices
/// from the root, assigned at build time. Stable as long as tree structure
/// is stable (same container types and child counts at each level).
class ComponentKey {
public:
  using value_type = std::size_t;
  using iterator = value_type*;
  using const_iterator = value_type const*;

  ComponentKey() = default;

  ComponentKey(std::initializer_list<value_type> init) {
    assign(init.begin(), init.end());
  }

  ComponentKey(std::vector<value_type> const& values) {
    assign(values.begin(), values.end());
  }

  template<typename It>
  ComponentKey(It first, It last) {
    assign(first, last);
  }

  ComponentKey(ComponentKey const& other) {
    assign(other.begin(), other.end());
  }

  ComponentKey(ComponentKey&& other) noexcept {
    moveFrom(std::move(other));
  }

  ComponentKey& operator=(ComponentKey const& other) {
    if (this != &other) {
      clear();
      assign(other.begin(), other.end());
    }
    return *this;
  }

  ComponentKey& operator=(ComponentKey&& other) noexcept {
    if (this != &other) {
      resetHeap();
      moveFrom(std::move(other));
    }
    return *this;
  }

  ~ComponentKey() {
    resetHeap();
  }

  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

  iterator begin() noexcept { return storage(); }
  iterator end() noexcept { return storage() + size_; }
  const_iterator begin() const noexcept { return storage(); }
  const_iterator end() const noexcept { return storage() + size_; }
  const_iterator cbegin() const noexcept { return begin(); }
  const_iterator cend() const noexcept { return end(); }

  value_type* data() noexcept { return storage(); }
  value_type const* data() const noexcept { return storage(); }

  value_type& operator[](std::size_t index) noexcept { return storage()[index]; }
  value_type const& operator[](std::size_t index) const noexcept { return storage()[index]; }

  value_type& back() noexcept { return storage()[size_ - 1U]; }
  value_type const& back() const noexcept { return storage()[size_ - 1U]; }

  void clear() noexcept { size_ = 0; }

  void push_back(value_type value) {
    reserve(size_ + 1U);
    storage()[size_++] = value;
  }

  void pop_back() noexcept {
    if (size_ > 0) {
      --size_;
    }
  }

  void reserve(std::size_t capacity) {
    if (capacity <= capacity_) {
      return;
    }
    std::size_t const newCapacity = std::max(capacity, capacity_ * 2U);
    std::unique_ptr<value_type[]> next = std::make_unique<value_type[]>(newCapacity);
    std::copy(begin(), end(), next.get());
    resetHeap();
    heap_ = next.release();
    capacity_ = newCapacity;
  }

  friend bool operator==(ComponentKey const& lhs, ComponentKey const& rhs) noexcept {
    return lhs.size_ == rhs.size_ && std::equal(lhs.begin(), lhs.end(), rhs.begin());
  }

  friend bool operator!=(ComponentKey const& lhs, ComponentKey const& rhs) noexcept {
    return !(lhs == rhs);
  }

private:
  template<typename It>
  void assign(It first, It last) {
    for (; first != last; ++first) {
      push_back(*first);
    }
  }

  value_type* storage() noexcept {
    return heap_ ? heap_ : inline_.data();
  }

  value_type const* storage() const noexcept {
    return heap_ ? heap_ : inline_.data();
  }

  void resetHeap() noexcept {
    delete[] heap_;
    heap_ = nullptr;
    capacity_ = inline_.size();
  }

  void moveFrom(ComponentKey&& other) noexcept {
    size_ = other.size_;
    if (other.heap_) {
      heap_ = other.heap_;
      capacity_ = other.capacity_;
      other.heap_ = nullptr;
      other.capacity_ = other.inline_.size();
      other.size_ = 0;
      return;
    }
    std::copy(other.begin(), other.end(), inline_.data());
    capacity_ = inline_.size();
    other.size_ = 0;
  }

  static constexpr std::size_t kInlineCapacity = 8;

  std::size_t size_ = 0;
  std::size_t capacity_ = kInlineCapacity;
  value_type* heap_ = nullptr;
  std::array<value_type, kInlineCapacity> inline_{};
};

struct ComponentKeyHash {
  std::size_t operator()(ComponentKey const& k) const noexcept;
};

} // namespace flux
