#pragma once

#include "ReactiveCore.hpp"

#include <optional>
#include <type_traits>
#include <utility>

namespace fluxv5 {

template <typename Predicate, typename ThenFn, typename ElseFn>
class ShowView {
public:
  using ThenOutput = std::invoke_result_t<ThenFn&>;
  using ElseOutput = std::invoke_result_t<ElseFn&>;
  using Output = std::common_type_t<ThenOutput, ElseOutput>;

  ShowView(Predicate predicate, ThenFn thenFn, ElseFn elseFn)
      : predicate_(std::move(predicate)),
        thenFn_(std::move(thenFn)),
        elseFn_(std::move(elseFn)) {
    effect_ = withOwner(scope_, [&] {
      return Effect([this] {
        reconcile(predicate_());
      });
    });
  }

  ShowView(ShowView const&) = delete;
  ShowView& operator=(ShowView const&) = delete;
  ShowView(ShowView&&) = delete;
  ShowView& operator=(ShowView&&) = delete;

  Output const& output() const {
    return branch_->output;
  }

  bool showingThen() const {
    return branch_ && branch_->showingThen;
  }

private:
  struct Branch {
    bool showingThen = false;
    Scope scope;
    Output output;

    Branch(bool nextShowingThen, Scope nextScope, Output nextOutput)
        : showingThen(nextShowingThen),
          scope(std::move(nextScope)),
          output(std::move(nextOutput)) {}

    Branch(Branch&&) noexcept = default;
    Branch& operator=(Branch&&) noexcept = default;
    Branch(Branch const&) = delete;
    Branch& operator=(Branch const&) = delete;
  };

  void reconcile(bool nextShowingThen) {
    if (branch_ && branch_->showingThen == nextShowingThen) {
      return;
    }

    branch_.reset();
    Scope branchScope;
    auto output = withOwner(branchScope, [&] {
      if (nextShowingThen) {
        return Output(thenFn_());
      }
      return Output(elseFn_());
    });
    branch_.emplace(nextShowingThen, std::move(branchScope), std::move(output));
  }

  Predicate predicate_;
  ThenFn thenFn_;
  ElseFn elseFn_;
  Scope scope_;
  Effect effect_;
  std::optional<Branch> branch_;
};

template <typename Predicate, typename ThenFn, typename ElseFn>
auto Show(Predicate predicate, ThenFn thenFn, ElseFn elseFn) {
  return ShowView<Predicate, ThenFn, ElseFn>(
    std::move(predicate), std::move(thenFn), std::move(elseFn));
}

} // namespace fluxv5
