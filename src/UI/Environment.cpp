#include <Flux/UI/Environment.hpp>

namespace flux {

EnvironmentStack& EnvironmentStack::current() {
  thread_local EnvironmentStack stack;
  return stack;
}

void EnvironmentStack::push(EnvironmentLayer layer) {
  layers_.push_back(std::move(layer));
}

void EnvironmentStack::pop() {
  if (!layers_.empty()) {
    layers_.pop_back();
  }
}

} // namespace flux
