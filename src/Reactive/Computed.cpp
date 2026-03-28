#include <Flux/Reactive/Detail/DependencyTracker.hpp>

namespace flux::detail {

namespace {

thread_local std::vector<DependencyTracker*> gTrackerStack;

} // namespace

DependencyTracker* DependencyTracker::current() {
  return gTrackerStack.empty() ? nullptr : gTrackerStack.back();
}

void DependencyTracker::push(DependencyTracker* tracker) {
  gTrackerStack.push_back(tracker);
}

void DependencyTracker::pop() {
  if (!gTrackerStack.empty()) {
    gTrackerStack.pop_back();
  }
}

void DependencyTracker::track(Observable* o) {
  if (DependencyTracker* t = current()) {
    t->deps.push_back(o);
  }
}

} // namespace flux::detail
