#pragma once

/// \file Flux/Detail/LruMap.hpp
///
/// Fixed-capacity LRU cache: \p Cap entries max; least-recently-used evicted on overflow.

#include <cstddef>
#include <functional>
#include <list>
#include <optional>
#include <unordered_map>
#include <utility>

namespace flux::detail {

template <typename Key, typename Value, std::size_t Cap, typename Hash = std::hash<Key>,
          typename KeyEq = std::equal_to<Key>>
class LruMap {
  static_assert(Cap > 0, "Cap must be positive");

public:
  using key_type = Key;
  using mapped_type = Value;
  using size_type = std::size_t;

  [[nodiscard]] bool empty() const noexcept { return map_.empty(); }

  [[nodiscard]] std::size_t size() const noexcept { return map_.size(); }

  void clear() {
    map_.clear();
    order_.clear();
  }

  /// Returns pointer to stored value (cache owns it). nullptr if missing.
  [[nodiscard]] Value* find(Key const& key) noexcept {
    auto it = map_.find(key);
    if (it == map_.end()) {
      return nullptr;
    }
    order_.splice(order_.begin(), order_, it->second);
    return &it->second->second;
  }

  [[nodiscard]] Value const* find(Key const& key) const noexcept {
    auto it = map_.find(key);
    if (it == map_.end()) {
      return nullptr;
    }
    // const find cannot splice — use const_cast for LRU touch (mutable LRU order)
    auto& self = const_cast<LruMap&>(*this);
    self.order_.splice(self.order_.begin(), self.order_, it->second);
    return &it->second->second;
  }

  /// Insert or update. Evicts LRU if at capacity and key is new.
  template <typename V>
  Value& insert(Key key, V&& value) {
    auto it = map_.find(key);
    if (it != map_.end()) {
      it->second->second = std::forward<V>(value);
      order_.splice(order_.begin(), order_, it->second);
      return it->second->second;
    }

    while (map_.size() >= Cap) {
      evictLru();
    }

    order_.emplace_front(std::move(key), std::forward<V>(value));
    auto listIt = order_.begin();
    map_[listIt->first] = listIt;
    return listIt->second;
  }

  /// Remove one entry by key. Returns true if erased.
  bool erase(Key const& key) {
    auto it = map_.find(key);
    if (it == map_.end()) {
      return false;
    }
    order_.erase(it->second);
    map_.erase(it);
    return true;
  }

  void forEach(std::function<void(Key const&, Value&)> const& fn) {
    for (auto& p : order_) {
      fn(p.first, p.second);
    }
  }

private:
  void evictLru() {
    if (order_.empty()) {
      return;
    }
    auto const& backKey = order_.back().first;
    map_.erase(backKey);
    order_.pop_back();
  }

  using List = std::list<std::pair<Key, Value>>;
  using ListIter = typename List::iterator;

  List order_{};
  std::unordered_map<Key, ListIter, Hash, KeyEq> map_{};
};

} // namespace flux::detail
