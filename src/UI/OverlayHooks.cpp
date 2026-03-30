#include <Flux/Detail/Runtime.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/UI/Overlay.hpp>
#include <Flux/UI/StateStore.hpp>

#include <cassert>
#include <tuple>
#include <utility>

namespace flux {

namespace {

struct OverlayHookSlot {
  OverlayId id{};
  Window* window = nullptr;

  ~OverlayHookSlot() {
    if (id.isValid() && window) {
      OverlayId const rid = id;
      id = kInvalidOverlayId;
      window->removeOverlay(rid);
    }
  }
};

} // namespace

std::tuple<std::function<void(Element, OverlayConfig)>, std::function<void()>, bool> useOverlay() {
  StateStore* store = StateStore::current();
  Runtime* rt = Runtime::current();
  assert(store && "useOverlay called outside of a build pass");
  assert(rt && "useOverlay called outside of a build pass");

  OverlayHookSlot& slot = store->claimSlot<OverlayHookSlot>();
  Window& w = rt->window();
  slot.window = &w;

  OverlayHookSlot* slotPtr = &slot;
  Window* wPtr = &w;

  auto show = [slotPtr, wPtr](Element el, OverlayConfig cfg) {
    if (slotPtr->id.isValid()) {
      OverlayId const rid = slotPtr->id;
      slotPtr->id = kInvalidOverlayId;
      wPtr->removeOverlay(rid);
    }
    slotPtr->id = wPtr->pushOverlay(std::move(el), std::move(cfg));
  };

  auto hide = [slotPtr, wPtr]() {
    if (slotPtr->id.isValid()) {
      OverlayId const rid = slotPtr->id;
      slotPtr->id = kInvalidOverlayId;
      wPtr->removeOverlay(rid);
    }
  };

  bool const presented = slot.id.isValid();
  return { std::move(show), std::move(hide), presented };
}

} // namespace flux
