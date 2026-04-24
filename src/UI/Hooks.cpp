#include <Flux/Detail/Runtime.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/StateStore.hpp>

#include "Debug/PerfCounters.hpp"

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
  debug::perf::recordComponentKeyPrefixCompare(key.size());
  if (!pressKey.hasPrefix(key)) {
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
  std::optional<OverlayId> const overlayScope =
      store->overlayScope().has_value() ? std::optional<OverlayId>{OverlayId{*store->overlayScope()}} : std::nullopt;

  return [rt, key, overlayScope] { rt->requestFocusInSubtree(key, overlayScope); };
}

std::function<void()> useClearFocus() {
  Runtime* rt = Runtime::current();
  StateStore* store = StateStore::current();
  assert(rt && "useClearFocus called outside of a build pass");
  assert(store && "useClearFocus called outside of a build pass");
  (void)store;

  return [rt] { rt->focus().clear(); };
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

detail::ElementModifiers const* useOuterElementModifiers() noexcept {
  StateStore* store = StateStore::current();
  if (!store) {
    return nullptr;
  }
  return store->currentCompositeElementModifiers();
}

} // namespace flux
