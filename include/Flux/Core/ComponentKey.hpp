#pragma once

/// \file Flux/Core/ComponentKey.hpp
///
/// Part of the Flux public API.

#include <Flux/Core/LocalId.hpp>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <vector>

namespace flux {

/// Structural path of a component: sequence of per-parent local ids from the root.
///
/// Positional children use `LocalId::fromIndex(i)`. Explicit keys use `LocalId::fromString(...)`
/// and remain stable across reorder.
class ComponentKey {
public:
  using value_type = LocalId;

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

  void clear() noexcept;
  void push_back(value_type value);
  void pop_back() noexcept;
  void reserve(std::size_t) noexcept {}

  [[nodiscard]] ComponentKey prefix(std::size_t length) const;
  [[nodiscard]] bool hasPrefix(ComponentKey const& prefix) const noexcept;
  [[nodiscard]] bool sharesPrefix(ComponentKey const& other) const noexcept;
  [[nodiscard]] value_type tail() const noexcept;
  void appendPrefixTo(std::vector<value_type>& out, std::size_t length) const;
  [[nodiscard]] std::vector<value_type> materialize() const;

  friend bool operator==(ComponentKey const& lhs, ComponentKey const& rhs) noexcept;
  friend struct ComponentKeyHash;

  friend bool operator!=(ComponentKey const& lhs, ComponentKey const& rhs) noexcept {
    return !(lhs == rhs);
  }

private:
  template<typename It>
  void assign(It first, It last) {
    std::vector<value_type> values{};
    for (It it = first; it != last; ++it) {
      values.push_back(*it);
    }
    assignFromValues(values.data(), values.size());
  }

  void assignFromValues(value_type const* values, std::size_t count);
  [[nodiscard]] static ComponentKey fromHandle(std::uint32_t handle, std::uint32_t size) noexcept;

  std::uint32_t handle_ = 0;
  std::uint32_t size_ = 0;
};

struct ComponentKeyHash {
  std::size_t operator()(ComponentKey const& k) const noexcept;
};

} // namespace flux
