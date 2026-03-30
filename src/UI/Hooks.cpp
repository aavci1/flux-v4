#include <Flux/Detail/Runtime.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/StateStore.hpp>

#include <algorithm>
#include <cassert>
#include <functional>

namespace flux {

bool useFocus() {
  Runtime* rt = Runtime::current();
  StateStore* store = StateStore::current();
  if (!rt || !store) {
    return false;
  }
  return rt->focus().isInSubtree(store->currentComponentKey(), *store);
}

bool useKeyboardFocus() {
  Runtime* rt = Runtime::current();
  StateStore* store = StateStore::current();
  if (!rt || !store) {
    return false;
  }
  return rt->focus().isInSubtree(store->currentComponentKey(), *store) && rt->focus().hasKeyboardOrigin();
}

bool useHover() {
  Runtime* rt = Runtime::current();
  StateStore* store = StateStore::current();
  if (!rt || !store) {
    return false;
  }
  return rt->hover().isInSubtree(store->currentComponentKey(), *store);
}

bool usePress() {
  Runtime* rt = Runtime::current();
  StateStore* store = StateStore::current();
  if (!rt || !store) {
    return false;
  }
  ComponentKey const& key = store->currentComponentKey();
  ComponentKey const& pressKey = rt->gesture().activePressKey();
  if (pressKey.empty() || key.size() > pressKey.size()) {
    return false;
  }
  if (!std::equal(key.begin(), key.end(), pressKey.begin())) {
    return false;
  }
  return rt->gesture().pressMatchesStoreContext(*store);
}

std::function<void()> useRequestFocus() {
  Runtime* rt = Runtime::current();
  StateStore* store = StateStore::current();
  assert(rt && "useRequestFocus called outside of a build pass");
  assert(store && "useRequestFocus called outside of a build pass");

  ComponentKey const key = store->currentComponentKey();

  return [rt, key] { rt->requestFocusInSubtree(key); };
}

std::optional<Rect> useLayoutRect() {
  Runtime* rt = Runtime::current();
  StateStore* store = StateStore::current();
  if (!rt || !store) {
    return std::nullopt;
  }
  return rt->layoutRectForCurrentComponent();
}

} // namespace flux
