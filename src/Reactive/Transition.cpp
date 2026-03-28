#include <Flux/Reactive/Transition.hpp>

namespace flux {

namespace {

thread_local std::vector<Transition> gTransitionStack;

} // namespace

WithTransition::WithTransition(Transition t) {
  gTransitionStack.push_back(t);
}

WithTransition::~WithTransition() {
  if (!gTransitionStack.empty()) {
    gTransitionStack.pop_back();
  }
}

Transition WithTransition::current() {
  if (gTransitionStack.empty()) {
    return Transition::instant();
  }
  return gTransitionStack.back();
}

} // namespace flux
