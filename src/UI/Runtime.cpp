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

namespace flux {

Runtime::Runtime(Window& window) : window_(window) {
  subscribeToRebuild();
  subscribeInput();
}

Runtime::~Runtime() {
  if (rebuildHandle_.isValid()) {
    Application::instance().unobserveNextFrame(rebuildHandle_);
  }
}

void Runtime::rebuild() {
  SceneGraph& graph = window_.sceneGraph();
  graph.clear();

  EventMap newMap;
  BuildContext ctx{graph, newMap, Application::instance().textSystem(), layoutEngine_};
  Size const sz = window_.getSize();
  LayoutConstraints rootCs{};
  rootCs.maxWidth = std::max(1.f, sz.width);
  rootCs.maxHeight = std::max(1.f, sz.height);
  ctx.pushConstraints(rootCs);
  if (root_) {
    root_->build(ctx);
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

void Runtime::handleInput(InputEvent const& e) {
  if (e.kind != InputEvent::Kind::PointerDown && e.kind != InputEvent::Kind::PointerUp &&
      e.kind != InputEvent::Kind::PointerMove) {
    return;
  }
  Point const p{e.position.x, e.position.y};
  auto hit = HitTester{}.hitTest(window_.sceneGraph(), p);
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
