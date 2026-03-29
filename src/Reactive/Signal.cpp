#include <Flux/Core/Application.hpp>
#include <Flux/Reactive/Detail/Notify.hpp>

#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace flux::detail {

namespace {

thread_local int gNotifyDepth = 0;
thread_local std::vector<std::function<void()>> gDeferred;

} // namespace

void notifyObserverList(std::vector<std::pair<std::uint64_t, std::function<void()>>>& observers) {
  // No observers => nothing to invalidate; skip markReactiveDirty (see Signal::set with non-comparable T).
  if (observers.empty()) {
    return;
  }
  if (Application::hasInstance()) {
    Application::instance().markReactiveDirty();
  }
  auto snapshot = observers;
  if (gNotifyDepth > 0) {
    gDeferred.push_back([snap = std::move(snapshot)]() mutable {
      for (auto& p : snap) {
        if (p.second) {
          p.second();
        }
      }
    });
    return;
  }

  gNotifyDepth++;
  for (auto& p : snapshot) {
    if (p.second) {
      p.second();
    }
  }
  gNotifyDepth--;

  while (!gDeferred.empty()) {
    auto batch = std::move(gDeferred);
    gDeferred.clear();
    for (auto& f : batch) {
      gNotifyDepth++;
      if (f) {
        f();
      }
      gNotifyDepth--;
    }
  }
}

} // namespace flux::detail
