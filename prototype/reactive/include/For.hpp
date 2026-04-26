#pragma once

#include "ReactiveCore.hpp"

#include <cassert>
#include <functional>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fluxv5 {

template <typename T, typename KeyFn, typename Factory>
class ForView {
public:
  using Items = std::vector<T>;
  using Key = std::invoke_result_t<KeyFn&, T const&>;
  using Output = std::invoke_result_t<Factory&, T const&, Signal<std::size_t>>;

  ForView(Signal<Items> items, KeyFn keyFn, Factory factory)
      : items_(std::move(items)),
        keyFn_(std::move(keyFn)),
        factory_(std::move(factory)) {
    effect_ = withOwner(scope_, [&] {
      return Effect([this] {
        reconcile(items_.get());
      });
    });
  }

  ForView(ForView const&) = delete;
  ForView& operator=(ForView const&) = delete;
  ForView(ForView&&) = delete;
  ForView& operator=(ForView&&) = delete;

  std::vector<Output> const& outputs() const {
    return outputs_;
  }

  std::size_t size() const {
    return rows_.size();
  }

private:
  struct Row {
    Key key;
    T item;
    Scope scope;
    Signal<std::size_t> index;
    Output output;

    Row(Key nextKey, T nextItem, Scope nextScope,
        Signal<std::size_t> nextIndex, Output nextOutput)
        : key(std::move(nextKey)),
          item(std::move(nextItem)),
          scope(std::move(nextScope)),
          index(std::move(nextIndex)),
          output(std::move(nextOutput)) {}

    Row(Row&&) noexcept = default;
    Row& operator=(Row&&) noexcept = default;
    Row(Row const&) = delete;
    Row& operator=(Row const&) = delete;
  };

  void reconcile(Items const& nextItems) {
    std::unordered_map<Key, std::size_t> oldRows;
    oldRows.reserve(rows_.size());
    for (std::size_t i = 0; i < rows_.size(); ++i) {
      oldRows.emplace(rows_[i].key, i);
    }

    std::vector<Row> nextRows;
    nextRows.reserve(nextItems.size());

    for (std::size_t i = 0; i < nextItems.size(); ++i) {
      auto key = keyFn_(nextItems[i]);
      auto old = oldRows.find(key);
      if (old != oldRows.end()) {
        auto row = std::move(rows_[old->second]);
        row.item = nextItems[i];
        row.index.set(i);
        nextRows.push_back(std::move(row));
        oldRows.erase(old);
        continue;
      }

      Scope rowScope;
      auto index = withOwner(rowScope, [&] {
        return Signal<std::size_t>(i);
      });
      auto output = withOwner(rowScope, [&] {
        return factory_(nextItems[i], index);
      });
      nextRows.emplace_back(std::move(key), nextItems[i], std::move(rowScope),
        std::move(index), std::move(output));
    }

    rows_ = std::move(nextRows);
    outputs_.clear();
    outputs_.reserve(rows_.size());
    for (auto const& row : rows_) {
      outputs_.push_back(row.output);
    }
  }

  Signal<Items> items_;
  KeyFn keyFn_;
  Factory factory_;
  Scope scope_;
  Effect effect_;
  std::vector<Row> rows_;
  std::vector<Output> outputs_;
};

template <typename T, typename KeyFn, typename Factory>
auto For(Signal<std::vector<T>> items, KeyFn keyFn, Factory factory) {
  return ForView<T, KeyFn, Factory>(
    std::move(items), std::move(keyFn), std::move(factory));
}

} // namespace fluxv5
