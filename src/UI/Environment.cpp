#include <Flux/UI/Environment.hpp>

#include <Flux/Core/Window.hpp>
#include <Flux/Detail/Runtime.hpp>

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

namespace detail {

EnvironmentLayer const* windowEnvironmentLayerForCurrentRuntime() {
  Runtime* const rt = Runtime::current();
  if (!rt) {
    return nullptr;
  }
  return &rt->window().environmentLayer();
}

} // namespace detail

} // namespace flux
