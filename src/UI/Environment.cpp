#include <Flux/UI/Environment.hpp>

namespace flux {

EnvironmentStack& EnvironmentStack::current() {
  thread_local EnvironmentStack stack;
  return stack;
}

void EnvironmentStack::push(EnvironmentLayer layer) {
  Entry entry;
  entry.owned = std::move(layer);
  layers_.push_back(std::move(entry));
}

void EnvironmentStack::pushBorrowed(EnvironmentLayer const& layer) {
  Entry entry;
  entry.borrowed = &layer;
  layers_.push_back(std::move(entry));
}

void EnvironmentStack::pop() {
  if (!layers_.empty()) {
    layers_.pop_back();
  }
}

} // namespace flux
