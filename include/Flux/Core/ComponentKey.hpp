#pragma once

/// \file Flux/Core/ComponentKey.hpp
///
/// Part of the Flux public API.

#include <Flux/Core/LocalId.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

namespace flux {

/// Structural path of a component: sequence of per-parent local ids from the root.
///
/// Positional children use `LocalId::fromIndex(i)`. Explicit keys use `LocalId::fromString(...)`
/// and remain stable across reorder.
class ComponentKey {
public:
  using value_type = LocalId;
  using iterator = value_type*;
  using const_iterator = value_type const*;

  ComponentKey() = default;

  ComponentKey(std::initializer_list<value_type> init) {
    copyFrom(init.begin(), init.size());
  }

  ComponentKey(std::vector<value_type> const& values) {
    copyFrom(values.data(), values.size());
  }

  ComponentKey(std::vector<value_type> const& prefix, value_type tail) {
    reserve(prefix.size() + 1U);
    std::copy_n(prefix.data(), prefix.size(), storage());
    size_ = prefix.size();
    storage()[size_++] = tail;
  }

  template<typename It>
  ComponentKey(It first, It last) {
    assign(first, last);
  }

  ComponentKey(ComponentKey const& other) {
    copyFrom(other.data(), other.size());
  }

  ComponentKey(ComponentKey const& prefix, value_type tail) {
    reserve(prefix.size() + 1U);
    std::copy_n(prefix.data(), prefix.size(), storage());
    size_ = prefix.size();
    storage()[size_++] = tail;
  }

  ComponentKey(ComponentKey&& other) noexcept {
    moveFrom(std::move(other));
  }

  ComponentKey& operator=(ComponentKey const& other) {
    if (this != &other) {
      copyFrom(other.data(), other.size());
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
    std::size_t const count = static_cast<std::size_t>(std::distance(first, last));
    reserve(count);
    std::copy(first, last, storage());
    size_ = count;
  }

  void copyFrom(value_type const* values, std::size_t count) {
    if (count <= kInlineCapacity) {
      resetHeap();
    } else if (count > capacity_) {
      resetHeap();
      heap_ = new value_type[count];
      capacity_ = count;
    }
    if (count > 0) {
      std::copy_n(values, count, storage());
    }
    size_ = count;
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

  static constexpr std::size_t kInlineCapacity = 12;

  std::size_t size_ = 0;
  std::size_t capacity_ = kInlineCapacity;
  value_type* heap_ = nullptr;
  std::array<value_type, kInlineCapacity> inline_{};
};

struct ComponentKeyHash {
  std::size_t operator()(ComponentKey const& k) const noexcept;
};

} // namespace flux
