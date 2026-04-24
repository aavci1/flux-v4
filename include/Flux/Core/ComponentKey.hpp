#pragma once

/// \file Flux/Core/ComponentKey.hpp
///
/// Part of the Flux public API.

#include <Flux/Core/LocalId.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
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
  using iterator = value_type const*;
  using const_iterator = value_type const*;

  ComponentKey() = default;

  ComponentKey(std::initializer_list<value_type> init);

  ComponentKey(std::vector<value_type> const& values);

  ComponentKey(std::vector<value_type> const& prefix, value_type tail);

  template<typename It>
  ComponentKey(It first, It last) {
    assign(first, last);
  }

  ComponentKey(ComponentKey const& other);

  ComponentKey(ComponentKey const& prefix, value_type tail);

  ComponentKey(ComponentKey&& other) noexcept;

  ComponentKey& operator=(ComponentKey const& other);

  ComponentKey& operator=(ComponentKey&& other) noexcept;

  ~ComponentKey();

  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

  iterator begin() noexcept { return data(); }
  iterator end() noexcept { return data() + size_; }
  const_iterator begin() const noexcept { return data(); }
  const_iterator end() const noexcept { return data() + size_; }
  const_iterator cbegin() const noexcept { return begin(); }
  const_iterator cend() const noexcept { return end(); }

  value_type const* data() noexcept;
  value_type const* data() const noexcept;

  value_type const& operator[](std::size_t index) const noexcept;

  value_type const& back() const noexcept;

  void clear() noexcept;

  void push_back(value_type value);

  void pop_back() noexcept;

  void reserve(std::size_t capacity);

  [[nodiscard]] ComponentKey prefix(std::size_t length) const;
  [[nodiscard]] bool hasPrefix(ComponentKey const& prefix) const noexcept;
  [[nodiscard]] bool sharesPrefix(ComponentKey const& other) const noexcept;

  friend bool operator==(ComponentKey const& lhs, ComponentKey const& rhs) noexcept;
  friend struct ComponentKeyHash;

  friend bool operator!=(ComponentKey const& lhs, ComponentKey const& rhs) noexcept {
    return !(lhs == rhs);
  }

private:
  template<typename It>
  void assign(It first, It last) {
    std::size_t const count = static_cast<std::size_t>(std::distance(first, last));
    materialized_.assign(first, last);
    cacheValid_ = true;
    assignFromValues(materialized_.data(), count);
    materialized_.clear();
    cacheValid_ = false;
  }

  void assignFromValues(value_type const* values, std::size_t count);
  void ensureMaterialized() const;
  [[nodiscard]] static ComponentKey fromHandle(std::uint32_t handle, std::uint32_t size) noexcept;

  std::uint32_t handle_ = 0;
  std::uint32_t size_ = 0;
  mutable bool cacheValid_ = false;
  mutable std::vector<value_type> materialized_{};
};

struct ComponentKeyHash {
  std::size_t operator()(ComponentKey const& k) const noexcept;
};

} // namespace flux
