#pragma once

/// \file Flux/UI/Environment.hpp
///
/// Part of the Flux public API.


#include <any>
#include <typeinfo>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace flux {

class EnvironmentLayer {
public:
  template<typename T>
  void set(T value) {
    values_[std::type_index(typeid(T))] = std::move(value);
  }

  template<typename T>
  T const* get() const {
    auto it = values_.find(std::type_index(typeid(T)));
    if (it == values_.end()) {
      return nullptr;
    }
    return std::any_cast<T>(&it->second);
  }

  bool empty() const { return values_.empty(); }

private:
  std::unordered_map<std::type_index, std::any> values_;
};

class EnvironmentStack {
public:
  static EnvironmentStack& current();

  void push(EnvironmentLayer layer);
  void pop();

  template<typename T>
  T const* find() const {
    for (auto it = layers_.rbegin(); it != layers_.rend(); ++it) {
      if (T const* v = it->get<T>()) {
        return v;
      }
    }
    return nullptr;
  }

  bool empty() const { return layers_.empty(); }

  /// Stack copy for incremental rebuild snapshot comparison.
  [[nodiscard]] std::vector<EnvironmentLayer> snapshotLayers() const { return layers_; }

private:
  std::vector<EnvironmentLayer> layers_;
};

} // namespace flux
