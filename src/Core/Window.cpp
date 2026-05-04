#include <Flux/Core/Application.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/Events.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Detail/RootHolder.hpp>
#include <Flux/Detail/Runtime.hpp>
#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Reactive/Signal.hpp>
#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/SceneGraph/SceneRenderer.hpp>
#include <Flux/UI/Overlay.hpp>
#include <Flux/UI/EnvironmentBinding.hpp>
#include <Flux/UI/EnvironmentKeys.hpp>
#include <Flux/UI/Theme.hpp>

#include <memory>
#include <utility>

#include "Core/PlatformWindow.hpp"
#include "Core/PlatformWindowCreate.hpp"
#include "Core/WindowRender.hpp"
#include <optional>

namespace flux {

struct Window::Impl {
  std::unique_ptr<PlatformWindow> platform_;
  std::unique_ptr<Canvas> canvas_;
  std::unique_ptr<scenegraph::SceneRenderer> sceneRenderer_;
  std::optional<scenegraph::SceneGraph> sceneGraph_;
  Color clearColor_ {Theme::light().windowBackgroundColor};
  bool hasCustomClearColor_ = false;
  /// Declared before `runtime_` so `~Runtime` (and `OverlayHookSlot` teardown calling `removeOverlay`)
  /// runs while `OverlayManager` is still alive. Reverse member destruction order would destroy
  /// `overlayMgr_` first and use-after-free on window close with an open overlay.
  OverlayManager overlayMgr_;
  /// Declared before `runtime_` so the ring outlives `~Runtime` if teardown ever touches the overlay buffer.
  TextCacheRingBuffer textCacheRing_{};
  std::unique_ptr<Runtime> runtime_;
  std::unordered_map<std::string, ActionDescriptor> actions_;
  Reactive::Signal<Theme> themeSignal_{Theme::light()};
  EnvironmentBinding windowEnvironmentBinding_{};
  std::string restoreId_;
  bool shutdown_ = false;

  explicit Impl(Window&, WindowConfig const& config)
      : restoreId_(config.restoreId) {
    windowEnvironmentBinding_ = EnvironmentBinding{}.withSignal<ThemeKey>(themeSignal_);
  }
  ~Impl();

  PlatformWindow* platformWindow() const { return platform_.get(); }
  void setViewRoot(Window& window, std::unique_ptr<RootHolder> holder);
  void shutdown();
};

void Window::Impl::setViewRoot(Window& window, std::unique_ptr<RootHolder> holder) {
  if (!runtime_) {
    runtime_ = std::make_unique<Runtime>(window);
  }
  runtime_->setRoot(std::move(holder));
}

Window::Impl::~Impl() {
  shutdown();
}

void Window::Impl::shutdown() {
  if (shutdown_) {
    return;
  }
  shutdown_ = true;
  if (runtime_) {
    runtime_->beginShutdown(sceneGraph_ ? &*sceneGraph_ : nullptr);
    overlayMgr_.clear(nullptr, false);
    runtime_.reset();
  } else {
    overlayMgr_.clear(nullptr, false);
  }
}

Window::Window(const WindowConfig& config) {
  d = std::make_unique<Impl>(*this, config);
  d->platform_ = detail::createPlatformWindow(config);
  d->platform_->setFluxWindow(this);
  Application::instance().eventQueue().post(WindowLifecycleEvent{
      WindowLifecycleEvent::Kind::Registered,
      handle(),
      this,
  });
}

Window::~Window() {
  if (d) {
    d->shutdown();
  }
  const unsigned int id = handle();
  if (Application::hasInstance()) {
    Application::instance().unregisterWindowHandle(id);
    Application::instance().eventQueue().post(
        WindowLifecycleEvent{WindowLifecycleEvent::Kind::Unregistered, id, nullptr});
  }
}

Size Window::getSize() const {
  return d->platform_->currentSize();
}

void Window::setTitle(std::string title) {
  d->platform_->setTitle(std::move(title));
}

void Window::setFullscreen(bool fullscreen) {
  d->platform_->setFullscreen(fullscreen);
}

unsigned int Window::handle() const {
  return d->platform_->handle();
}

Canvas& Window::canvas() {
  if (!d->canvas_) {
    d->canvas_ = d->platform_->createCanvas(*this);
  }
  return *d->canvas_;
}

bool Window::hasSceneGraph() const { return d->sceneGraph_.has_value(); }

scenegraph::SceneGraph& Window::sceneGraph() {
  if (!d->sceneGraph_) {
    d->sceneGraph_.emplace();
  }
  return *d->sceneGraph_;
}

scenegraph::SceneGraph const& Window::sceneGraph() const {
  return const_cast<Window*>(this)->sceneGraph();
}

void Window::requestRedraw() { postRedraw(handle()); }

void Window::setCursor(Cursor kind) {
  d->platform_->setCursor(kind);
}

PlatformWindow* Window::platformWindow() const {
  return d->platformWindow();
}

void Window::postRedraw(unsigned int handle) {
  if (!Application::hasInstance()) {
    return;
  }
  Application::instance().requestWindowRedraw(handle);
}

void Window::setClearColor(Color color) {
  d->clearColor_ = color;
  d->hasCustomClearColor_ = true;
}

Color Window::clearColor() const { return d->clearColor_; }

void Window::setTheme(Theme theme) {
  Color const clearColor = theme.windowBackgroundColor;
  d->themeSignal_.set(std::move(theme));
  d->windowEnvironmentBinding_ = EnvironmentBinding{}.withSignal<ThemeKey>(d->themeSignal_);
  if (!d->hasCustomClearColor_) {
    d->clearColor_ = clearColor;
  }
  requestRedraw();
}

Theme const& Window::theme() const {
  return d->themeSignal_.peek();
}

bool Window::wantsTextInput() const {
  return d->runtime_ && d->runtime_->wantsTextInput();
}

OverlayId Window::pushOverlay(Element content, OverlayConfig config) {
  if (!d) {
    return kInvalidOverlayId;
  }
  Runtime* rt = d->runtime_.get();
  return d->overlayMgr_.push(std::move(content), std::move(config), rt);
}

void Window::removeOverlay(OverlayId id) {
  if (!d) {
    return;
  }
  Runtime* rt = d->runtime_.get();
  d->overlayMgr_.remove(id, rt);
}

void Window::clearOverlays() {
  if (!d) {
    return;
  }
  Runtime* rt = d->runtime_.get();
  d->overlayMgr_.clear(rt);
}

OverlayManager& Window::overlayManager() { return d->overlayMgr_; }

OverlayManager const& Window::overlayManager() const { return d->overlayMgr_; }

void Window::registerAction(std::string name, ActionDescriptor descriptor) {
  d->actions_[std::move(name)] = std::move(descriptor);
}

bool Window::isActionEnabled(std::string const& name) const {
  auto it = d->actions_.find(name);
  if (it == d->actions_.end()) {
    return false;
  }
  if (it->second.isEnabled && !it->second.isEnabled()) {
    return false;
  }
  if (!d->runtime_) {
    return true;
  }
  return d->runtime_->isActionCurrentlyEnabled(name);
}

bool Window::dispatchAction(std::string const& name) {
  return d->runtime_ && d->runtime_->dispatchAction(name);
}

std::unordered_map<std::string, ActionDescriptor> const& Window::actionDescriptors() const {
  return d->actions_;
}

std::string const& Window::restoreId() const {
  return d->restoreId_;
}

WindowState Window::currentWindowState() const {
  WindowState state;
  if (auto frame = d->platform_->currentFrame()) {
    state.frame = *frame;
  }
  state.fullscreen = d->platform_->isFullscreen();
  state.contentSize = d->platform_->currentSize();
  return state;
}

void Window::applyRestoredWindowState(WindowState const& state) {
  if (state.frame.width > 0.f && state.frame.height > 0.f) {
    d->platform_->setFrame(state.frame);
  }
}

void Window::setViewRoot(std::unique_ptr<RootHolder> holder) {
  d->setViewRoot(*this, std::move(holder));
}

EnvironmentBinding const& Window::environmentBinding() const {
  return d->windowEnvironmentBinding_;
}

EnvironmentBinding& Window::environmentBindingMut() {
  return d->windowEnvironmentBinding_;
}

void Window::render(Canvas& canvas) {
  if (!d->sceneRenderer_) {
    d->sceneRenderer_ = std::make_unique<scenegraph::SceneRenderer>(canvas);
  }
  renderWindowFrame(*d->sceneRenderer_, canvas, d->sceneGraph_, d->overlayMgr_, d->runtime_.get(), d->clearColor_,
                    d->textCacheRing_);
}

} // namespace flux
