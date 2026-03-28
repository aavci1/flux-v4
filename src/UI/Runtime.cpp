#include <Flux/Detail/Runtime.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Core/Events.hpp>
#include <Flux/Scene/SceneGraph.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Scene/HitTester.hpp>
#include <Flux/UI/BuildContext.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/EventMap.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include <algorithm>
#include <cmath>

namespace flux {

void Runtime::setRoot(std::unique_ptr<RootHolder> holder) {
  rootHolder_ = std::move(holder);
  rebuild();
}

namespace {

/// Whole-point layout size avoids subpixel oscillation in `NSView.bounds` during live resize.
Size snapRootLayoutSize(Size s) {
  return {std::max(1.f, std::round(s.width)), std::max(1.f, std::round(s.height))};
}

} // namespace

Runtime::Runtime(Window& window) : window_(window) {
  subscribeToRebuild();
  subscribeInput();
  subscribeResize();
}

Runtime::~Runtime() {
  if (rebuildHandle_.isValid()) {
    Application::instance().unobserveNextFrame(rebuildHandle_);
  }
}

void Runtime::rebuild(std::optional<Size> sizeOverride) {
  SceneGraph& graph = window_.sceneGraph();
  graph.clear();

  layoutEngine_.resetForBuild();

  EventMap newMap;
  BuildContext ctx{graph, newMap, Application::instance().textSystem(), layoutEngine_};
  Size const raw = sizeOverride.value_or(window_.getSize());
  Size const sz = snapRootLayoutSize(raw);
  LayoutConstraints rootCs{};
  rootCs.maxWidth = sz.width;
  rootCs.maxHeight = sz.height;
  ctx.pushConstraints(rootCs);
  if (rootHolder_) {
    rootHolder_->buildInto(ctx);
  }
  ctx.popConstraints();
  eventMap_ = std::move(newMap);
  window_.requestRedraw();
}

void Runtime::subscribeToRebuild() {
  rebuildHandle_ = Application::instance().onNextFrameNeeded([this] { rebuild(); });
}

void Runtime::subscribeInput() {
  if (inputRegistered_) {
    return;
  }
  inputRegistered_ = true;
  unsigned int const hid = window_.handle();
  Application::instance().eventQueue().on<InputEvent>([this, hid](InputEvent const& e) {
    if (e.handle != hid) {
      return;
    }
    handleInput(e);
  });
}

void Runtime::subscribeResize() {
  unsigned int const hid = window_.handle();
  Application::instance().eventQueue().on<WindowEvent>([this, hid](WindowEvent const& ev) {
    if (ev.handle != hid || ev.kind != WindowEvent::Kind::Resize) {
      return;
    }
    // Use the size captured when the event was posted; re-querying the view during dispatch can
    // differ slightly from bounds updates and cause visible jitter during live resize.
    rebuild(ev.size);
  });
}

void Runtime::handleInput(InputEvent const& e) {
  if (e.kind != InputEvent::Kind::PointerDown && e.kind != InputEvent::Kind::PointerUp &&
      e.kind != InputEvent::Kind::PointerMove) {
    return;
  }
  Point const p{e.position.x, e.position.y};
  auto hit = HitTester{}.hitTest(window_.sceneGraph(), p, [this](NodeId id) {
    return eventMap_.find(id) != nullptr;
  });
  if (!hit) {
    return;
  }
  EventHandlers const* handlers = eventMap_.find(hit->nodeId);
  if (!handlers) {
    return;
  }
  switch (e.kind) {
  case InputEvent::Kind::PointerDown:
    if (handlers->onPointerDown) {
      handlers->onPointerDown(hit->localPoint);
    }
    break;
  case InputEvent::Kind::PointerUp:
    if (handlers->onPointerUp) {
      handlers->onPointerUp(hit->localPoint);
    }
    if (handlers->onTap) {
      handlers->onTap();
    }
    break;
  case InputEvent::Kind::PointerMove:
    if (handlers->onPointerMove) {
      handlers->onPointerMove(hit->localPoint);
    }
    break;
  default:
    break;
  }
}

} // namespace flux
