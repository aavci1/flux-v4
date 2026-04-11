#include <Flux/Detail/Runtime.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/LayoutEngine.hpp>
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

Rect useBounds() {
  if (std::optional<Rect> const rect = useLayoutRect()) {
    return *rect;
  }

  LayoutConstraints const* cs = useLayoutConstraints();
  if (!cs) {
    return {};
  }

  Rect bounds {};
  if (std::isfinite(cs->maxWidth) && cs->maxWidth > 0.f) {
    bounds.width = cs->maxWidth;
  }
  if (std::isfinite(cs->maxHeight) && cs->maxHeight > 0.f) {
    bounds.height = cs->maxHeight;
  }
  return bounds;
}

LayoutConstraints const* useLayoutConstraints() {
  StateStore* store = StateStore::current();
  if (!store) {
    return nullptr;
  }
  return store->currentCompositeConstraints();
}

ElementModifiers const* useOuterElementModifiers() noexcept {
  StateStore* store = StateStore::current();
  if (!store) {
    return nullptr;
  }
  return store->currentCompositeElementModifiers();
}

} // namespace flux
