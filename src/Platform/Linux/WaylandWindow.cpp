#include "Core/PlatformWindowCreate.hpp"

#include <Flux/Core/Application.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/Events.hpp>
#include <Flux/Core/KeyCodes.hpp>
#include <Flux/Core/Window.hpp>

#include "Graphics/Vulkan/VulkanCanvas.hpp"

#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

#include <linux/input-event-codes.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace flux {
namespace {

std::atomic<unsigned int> gNextHandle{1};
std::int64_t nowNanos() {
  using namespace std::chrono;
  return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

float safeScale(float scale) { return std::max(0.25f, scale); }

Point logicalPointFromFixed(wl_fixed_t x, wl_fixed_t y, float scaleX, float scaleY) {
  (void)scaleX;
  (void)scaleY;
  return {static_cast<float>(wl_fixed_to_double(x)), static_cast<float>(wl_fixed_to_double(y))};
}

MouseButton mouseButtonFromLinux(std::uint32_t button) {
  if (button == BTN_LEFT) return MouseButton::Left;
  if (button == BTN_RIGHT) return MouseButton::Right;
  if (button == BTN_MIDDLE) return MouseButton::Middle;
  return MouseButton::Other;
}

KeyCode keyFromXkb(xkb_keysym_t sym) {
  if (sym >= XKB_KEY_a && sym <= XKB_KEY_z) {
    return static_cast<KeyCode>(keys::A + (sym - XKB_KEY_a));
  }
  if (sym >= XKB_KEY_A && sym <= XKB_KEY_Z) {
    return static_cast<KeyCode>(keys::A + (sym - XKB_KEY_A));
  }
  switch (sym) {
  case XKB_KEY_Return: return keys::Return;
  case XKB_KEY_Tab: return keys::Tab;
  case XKB_KEY_space: return keys::Space;
  case XKB_KEY_BackSpace: return keys::Delete;
  case XKB_KEY_Delete: return keys::ForwardDelete;
  case XKB_KEY_Escape: return keys::Escape;
  case XKB_KEY_Left: return keys::LeftArrow;
  case XKB_KEY_Right: return keys::RightArrow;
  case XKB_KEY_Down: return keys::DownArrow;
  case XKB_KEY_Up: return keys::UpArrow;
  case XKB_KEY_Home: return keys::Home;
  case XKB_KEY_End: return keys::End;
  case XKB_KEY_Page_Up: return keys::PageUp;
  case XKB_KEY_Page_Down: return keys::PageDown;
  default: return 0;
  }
}

std::string utf8FromKeysym(xkb_keysym_t sym) {
  char buffer[8]{};
  int const n = xkb_keysym_to_utf8(sym, buffer, sizeof(buffer));
  return n > 1 ? std::string(buffer, static_cast<std::size_t>(n - 1)) : std::string{};
}

bool debugDecorations() {
  char const* value = std::getenv("FLUX_DEBUG_WAYLAND_DECORATIONS");
  return value && *value && std::strcmp(value, "0") != 0;
}

} // namespace

class WaylandWindow final : public PlatformWindow {
public:
  explicit WaylandWindow(WindowConfig const& config)
      : handle_(gNextHandle.fetch_add(1)), size_(config.size), title_(config.title),
        fullscreen_(config.fullscreen) {
    if (pipe(wakePipe_) != 0) {
      throw std::runtime_error("Failed to create Wayland wake pipe");
    }
    fcntl(wakePipe_[0], F_SETFL, fcntl(wakePipe_[0], F_GETFL, 0) | O_NONBLOCK);
    fcntl(wakePipe_[1], F_SETFL, fcntl(wakePipe_[1], F_GETFL, 0) | O_NONBLOCK);
    display_ = wl_display_connect(nullptr);
    if (!display_) {
      throw std::runtime_error("Failed to connect to Wayland display");
    }
    xkbContext_ = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    registry_ = wl_display_get_registry(display_);
    wl_registry_add_listener(registry_, &registryListener_, this);
    wl_display_roundtrip(display_);
    if (!compositor_ || !wmBase_) {
      throw std::runtime_error("Wayland compositor does not expose required xdg-shell globals");
    }

    surface_ = wl_compositor_create_surface(compositor_);
    wl_surface_add_listener(surface_, &surfaceListener_, this);
    wl_surface_set_buffer_scale(surface_, static_cast<std::int32_t>(std::lround(dpiScaleX_)));
    xdgSurface_ = xdg_wm_base_get_xdg_surface(wmBase_, surface_);
    xdg_surface_add_listener(xdgSurface_, &xdgSurfaceListener_, this);
    toplevel_ = xdg_surface_get_toplevel(xdgSurface_);
    xdg_toplevel_add_listener(toplevel_, &toplevelListener_, this);
    xdg_toplevel_set_title(toplevel_, title_.c_str());
    xdg_toplevel_set_app_id(toplevel_, "flux");
    if (config.minSize.width > 0.f || config.minSize.height > 0.f) setMinSize(config.minSize);
    if (config.maxSize.width > 0.f || config.maxSize.height > 0.f) setMaxSize(config.maxSize);
    requestServerSideDecorations();
    if (fullscreen_) xdg_toplevel_set_fullscreen(toplevel_, nullptr);
    wl_surface_commit(surface_);
    while (!configured_) {
      if (wl_display_dispatch(display_) < 0) {
        throw std::runtime_error("Wayland initial configure failed");
      }
    }
  }

  ~WaylandWindow() override {
    if (frameCallback_) wl_callback_destroy(frameCallback_);
    if (decoration_) zxdg_toplevel_decoration_v1_destroy(decoration_);
    if (toplevel_) xdg_toplevel_destroy(toplevel_);
    if (xdgSurface_) xdg_surface_destroy(xdgSurface_);
    if (surface_) wl_surface_destroy(surface_);
    if (pointer_) wl_pointer_destroy(pointer_);
    if (keyboard_) wl_keyboard_destroy(keyboard_);
    if (seat_) wl_seat_destroy(seat_);
    for (auto& output : outputs_) {
      if (output->output) wl_output_destroy(output->output);
    }
    if (decorationManager_) zxdg_decoration_manager_v1_destroy(decorationManager_);
    if (wmBase_) xdg_wm_base_destroy(wmBase_);
    if (compositor_) wl_compositor_destroy(compositor_);
    if (registry_) wl_registry_destroy(registry_);
    if (xkbState_) xkb_state_unref(xkbState_);
    if (xkbKeymap_) xkb_keymap_unref(xkbKeymap_);
    if (xkbContext_) xkb_context_unref(xkbContext_);
    if (display_) wl_display_disconnect(display_);
    if (wakePipe_[0] >= 0) close(wakePipe_[0]);
    if (wakePipe_[1] >= 0) close(wakePipe_[1]);
  }

  void setFluxWindow(Window* window) override { fluxWindow_ = window; }

  void show() override {
    updateCanvasDpi();
    Application::instance().requestWindowRedraw(handle_);
    Application::instance().flushRedraw();
  }

  std::unique_ptr<Canvas> createCanvas(Window&) override {
    auto canvas = createVulkanCanvas(display_, surface_, handle_, Application::instance().textSystem());
    canvas->updateDpiScale(dpiScaleX_, dpiScaleY_);
    canvas->resize(static_cast<int>(size_.width), static_cast<int>(size_.height));
    canvas_ = canvas.get();
    return canvas;
  }

  void resize(Size const& newSize) override {
    size_ = newSize;
    if (fluxWindow_) fluxWindow_->updateCanvasDpiScale(dpiScaleX_, dpiScaleY_);
    if (canvas_) canvas_->resize(static_cast<int>(std::lround(size_.width)),
                                 static_cast<int>(std::lround(size_.height)));
    Application::instance().eventQueue().post(WindowEvent{WindowEvent::Kind::Resize, handle_, size_});
    requestResizeRedraw();
  }

  void setMinSize(Size size) override {
    if (toplevel_) {
      xdg_toplevel_set_min_size(toplevel_, static_cast<int>(std::lround(size.width)),
                                static_cast<int>(std::lround(size.height)));
    }
  }

  void setMaxSize(Size size) override {
    if (toplevel_) {
      xdg_toplevel_set_max_size(toplevel_, static_cast<int>(std::lround(size.width)),
                                static_cast<int>(std::lround(size.height)));
    }
  }

  void setFullscreen(bool fullscreen) override {
    fullscreen_ = fullscreen;
    if (!toplevel_) return;
    if (fullscreen_) xdg_toplevel_set_fullscreen(toplevel_, nullptr);
    else xdg_toplevel_unset_fullscreen(toplevel_);
  }

  void setTitle(std::string const& title) override {
    title_ = title;
    if (toplevel_) xdg_toplevel_set_title(toplevel_, title_.c_str());
  }

  Size currentSize() const override { return size_; }
  bool isFullscreen() const override { return fullscreen_; }
  unsigned int handle() const override { return handle_; }
  void* nativeGraphicsSurface() const override { return surface_; }

  void processEvents() override {
    drainWakePipe();
    dispatchReadyEvents(0);
    flushDeferredRedraw();
  }

  void waitForEvents(int timeoutMs) override {
    dispatchReadyEvents(timeoutMs);
    flushDeferredRedraw();
  }

  void wakeEventLoop() override {
    char const c = 1;
    (void)write(wakePipe_[1], &c, 1);
  }
  int eventFd() const override { return display_ ? wl_display_get_fd(display_) : -1; }
  int wakeFd() const override { return wakePipe_[0]; }

  void requestAnimationFrame() override {
    if (framePending_ || !surface_) return;
    framePending_ = true;
    frameCallback_ = wl_surface_frame(surface_);
    wl_callback_add_listener(frameCallback_, &frameCallbackListener_, this);
    wl_surface_commit(surface_);
    wl_display_flush(display_);
  }

  void acknowledgeAnimationFrameTick() override {
    framePending_ = false;
  }

  void completeAnimationFrame(bool needsAnotherFrame) override {
    wl_display_flush(display_);
    if (needsAnotherFrame) requestAnimationFrame();
  }

private:
  static void registryGlobal(void* data, wl_registry* registry, std::uint32_t name,
                             char const* interface, std::uint32_t version) {
    auto* self = static_cast<WaylandWindow*>(data);
    if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
      self->compositor_ = static_cast<wl_compositor*>(
          wl_registry_bind(registry, name, &wl_compositor_interface, std::min(version, 4u)));
    } else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0) {
      self->wmBase_ = static_cast<xdg_wm_base*>(
          wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));
      xdg_wm_base_add_listener(self->wmBase_, &wmBaseListener_, self);
    } else if (std::strcmp(interface, wl_seat_interface.name) == 0) {
      self->seat_ = static_cast<wl_seat*>(
          wl_registry_bind(registry, name, &wl_seat_interface, std::min(version, 7u)));
      wl_seat_add_listener(self->seat_, &seatListener_, self);
    } else if (std::strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0) {
      self->decorationManager_ = static_cast<zxdg_decoration_manager_v1*>(
          wl_registry_bind(registry, name, &zxdg_decoration_manager_v1_interface, 1));
    } else if (std::strcmp(interface, wl_output_interface.name) == 0) {
      auto output = std::make_unique<Output>();
      output->owner = self;
      output->name = name;
      output->output = static_cast<wl_output*>(
          wl_registry_bind(registry, name, &wl_output_interface, std::min(version, 2u)));
      wl_output_add_listener(output->output, &outputListener_, output.get());
      self->outputs_.push_back(std::move(output));
    }
  }

  static void registryRemove(void* data, wl_registry*, std::uint32_t name) {
    auto* self = static_cast<WaylandWindow*>(data);
    auto it = std::find_if(self->outputs_.begin(), self->outputs_.end(),
                           [&](auto const& output) { return output->name == name; });
    if (it != self->outputs_.end()) {
      if ((*it)->entered && (*it)->scale >= self->dpiScaleX_) {
        (*it)->entered = false;
        self->updateEnteredScale();
      }
      if ((*it)->output) wl_output_destroy((*it)->output);
      self->outputs_.erase(it);
    }
  }

  static void wmPing(void*, xdg_wm_base* base, std::uint32_t serial) {
    xdg_wm_base_pong(base, serial);
  }

  static void frameDone(void* data, wl_callback* callback, std::uint32_t) {
    auto* self = static_cast<WaylandWindow*>(data);
    if (callback == self->frameCallback_) {
      wl_callback_destroy(self->frameCallback_);
      self->frameCallback_ = nullptr;
    } else {
      wl_callback_destroy(callback);
    }
    if (!self->framePending_) return;
    Application::instance().eventQueue().post(FrameEvent{nowNanos(), self->handle_});
    self->wakeEventLoop();
  }

  static void xdgConfigure(void* data, xdg_surface* surface, std::uint32_t serial) {
    auto* self = static_cast<WaylandWindow*>(data);
    xdg_surface_ack_configure(surface, serial);
    self->configured_ = true;
    if (self->pendingWidth_ > 0 && self->pendingHeight_ > 0) {
      self->applyConfiguredSize(self->pendingWidth_, self->pendingHeight_);
      self->pendingWidth_ = self->pendingHeight_ = 0;
    }
  }

  static void topConfigure(void* data, xdg_toplevel*, std::int32_t width, std::int32_t height,
                           wl_array*) {
    auto* self = static_cast<WaylandWindow*>(data);
    if (width > 0 && height > 0) {
      self->pendingWidth_ = width;
      self->pendingHeight_ = height;
    }
  }

  static void topClose(void* data, xdg_toplevel*) {
    auto* self = static_cast<WaylandWindow*>(data);
    Application::instance().eventQueue().post(WindowEvent{WindowEvent::Kind::CloseRequest, self->handle_});
  }
  static void topConfigureBounds(void*, xdg_toplevel*, std::int32_t, std::int32_t) {}
  static void topCapabilities(void*, xdg_toplevel*, wl_array*) {}

  static void decorationConfigure(void* data, zxdg_toplevel_decoration_v1*, std::uint32_t mode) {
    auto* self = static_cast<WaylandWindow*>(data);
    self->serverSideDecorationsActive_ = mode == ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
    if (self->serverSideDecorationsActive_) {
      if (debugDecorations() && !self->loggedDecorationMode_) {
        self->loggedDecorationMode_ = true;
        std::fprintf(stderr, "Flux Wayland: compositor accepted server-side decorations.\n");
      }
    } else if (!self->warnedDecorationFallback_) {
      self->warnedDecorationFallback_ = true;
      std::fprintf(stderr, "Flux Wayland: compositor refused server-side decorations; resize chrome may be absent.\n");
    }
  }

  static void seatCapabilities(void* data, wl_seat* seat, std::uint32_t caps) {
    auto* self = static_cast<WaylandWindow*>(data);
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !self->pointer_) {
      self->pointer_ = wl_seat_get_pointer(seat);
      wl_pointer_add_listener(self->pointer_, &pointerListener_, self);
    }
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !self->keyboard_) {
      self->keyboard_ = wl_seat_get_keyboard(seat);
      wl_keyboard_add_listener(self->keyboard_, &keyboardListener_, self);
    }
  }

  static void seatName(void*, wl_seat*, char const*) {}

  static void surfaceEnter(void* data, wl_surface*, wl_output* output) {
    auto* self = static_cast<WaylandWindow*>(data);
    if (Output* out = self->findOutput(output)) {
      out->entered = true;
      self->updateEnteredScale();
    }
  }
  static void surfaceLeave(void* data, wl_surface*, wl_output* output) {
    auto* self = static_cast<WaylandWindow*>(data);
    if (Output* out = self->findOutput(output)) {
      out->entered = false;
      self->updateEnteredScale();
    }
  }
  static void surfacePreferredBufferScale(void* data, wl_surface*, std::int32_t factor) {
    auto* self = static_cast<WaylandWindow*>(data);
    factor = std::max(1, factor);
    self->dpiScaleX_ = safeScale(static_cast<float>(factor));
    self->dpiScaleY_ = self->dpiScaleX_;
    wl_surface_set_buffer_scale(self->surface_, factor);
    if (self->fluxWindow_) self->fluxWindow_->updateCanvasDpiScale(self->dpiScaleX_, self->dpiScaleY_);
    Application::instance().eventQueue().post(WindowEvent{WindowEvent::Kind::DpiChanged, self->handle_, {},
                                                          self->dpiScaleX_});
    Application::instance().eventQueue().post(WindowEvent{WindowEvent::Kind::Resize, self->handle_, self->size_});
    self->requestResizeRedraw();
  }
  static void surfacePreferredBufferTransform(void*, wl_surface*, std::uint32_t) {}

  static void outputGeometry(void*, wl_output*, std::int32_t, std::int32_t, std::int32_t, std::int32_t,
                             std::int32_t, char const*, char const*, std::int32_t) {}
  static void outputMode(void*, wl_output*, std::uint32_t, std::int32_t, std::int32_t, std::int32_t) {}
  static void outputDone(void*, wl_output*) {}
  static void outputScale(void* data, wl_output*, std::int32_t scale) {
    auto* output = static_cast<Output*>(data);
    output->scale = safeScale(static_cast<float>(std::max(1, scale)));
    if (output->entered) output->owner->updateEnteredScale();
  }
  static void outputName(void*, wl_output*, char const*) {}
  static void outputDescription(void*, wl_output*, char const*) {}

  static void pointerEnter(void* data, wl_pointer*, std::uint32_t, wl_surface*, wl_fixed_t x, wl_fixed_t y) {
    auto* self = static_cast<WaylandWindow*>(data);
    self->pointerPos_ = logicalPointFromFixed(x, y, self->dpiScaleX_, self->dpiScaleY_);
  }
  static void pointerLeave(void*, wl_pointer*, std::uint32_t, wl_surface*) {}
  static void pointerMotion(void* data, wl_pointer*, std::uint32_t, wl_fixed_t x, wl_fixed_t y) {
    auto* self = static_cast<WaylandWindow*>(data);
    self->pointerPos_ = logicalPointFromFixed(x, y, self->dpiScaleX_, self->dpiScaleY_);
    Application::instance().eventQueue().post(InputEvent{.kind = InputEvent::Kind::PointerMove,
                                                         .handle = self->handle_,
                                                         .position = self->pointerPos_,
                                                         .pressedButtons = self->pressedButtons_});
  }
  static void pointerButton(void* data, wl_pointer*, std::uint32_t, std::uint32_t, std::uint32_t button,
                            std::uint32_t state) {
    auto* self = static_cast<WaylandWindow*>(data);
    std::uint8_t const bit = button == BTN_LEFT ? 1u : button == BTN_RIGHT ? 2u : button == BTN_MIDDLE ? 4u : 0u;
    if (state == WL_POINTER_BUTTON_STATE_PRESSED) self->pressedButtons_ |= bit;
    else self->pressedButtons_ &= static_cast<std::uint8_t>(~bit);
    Application::instance().eventQueue().post(InputEvent{.kind = state == WL_POINTER_BUTTON_STATE_PRESSED
                                                                     ? InputEvent::Kind::PointerDown
                                                                     : InputEvent::Kind::PointerUp,
                                                         .handle = self->handle_,
                                                         .position = self->pointerPos_,
                                                         .button = mouseButtonFromLinux(button),
                                                         .pressedButtons = self->pressedButtons_});
  }
  static void pointerAxis(void* data, wl_pointer*, std::uint32_t, std::uint32_t axis, wl_fixed_t value) {
    auto* self = static_cast<WaylandWindow*>(data);
    float dx = 0.f, dy = 0.f;
    float const v = static_cast<float>(wl_fixed_to_double(value));
    if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) dx = v;
    else dy = v;
    Application::instance().eventQueue().post(InputEvent{.kind = InputEvent::Kind::Scroll,
                                                         .handle = self->handle_,
                                                         .position = self->pointerPos_,
                                                         .scrollDelta = {dx, dy},
                                                         .preciseScrollDelta = true,
                                                         .pressedButtons = self->pressedButtons_});
  }
  static void pointerFrame(void*, wl_pointer*) {}
  static void pointerAxisSource(void*, wl_pointer*, std::uint32_t) {}
  static void pointerAxisStop(void*, wl_pointer*, std::uint32_t, std::uint32_t) {}
  static void pointerAxisDiscrete(void*, wl_pointer*, std::uint32_t, std::int32_t) {}
  static void pointerAxisValue120(void*, wl_pointer*, std::uint32_t, std::int32_t) {}
  static void pointerAxisRelativeDirection(void*, wl_pointer*, std::uint32_t, std::uint32_t) {}

  static void keymap(void* data, wl_keyboard*, std::uint32_t format, int fd, std::uint32_t size) {
    auto* self = static_cast<WaylandWindow*>(data);
    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
      close(fd);
      return;
    }
    char* map = static_cast<char*>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
    if (map == MAP_FAILED) {
      close(fd);
      return;
    }
    xkb_keymap* keymap = xkb_keymap_new_from_string(self->xkbContext_, map, XKB_KEYMAP_FORMAT_TEXT_V1,
                                                    XKB_KEYMAP_COMPILE_NO_FLAGS);
    munmap(map, size);
    close(fd);
    if (!keymap) return;
    xkb_state* state = xkb_state_new(keymap);
    if (!state) {
      xkb_keymap_unref(keymap);
      return;
    }
    if (self->xkbState_) xkb_state_unref(self->xkbState_);
    if (self->xkbKeymap_) xkb_keymap_unref(self->xkbKeymap_);
    self->xkbKeymap_ = keymap;
    self->xkbState_ = state;
  }

  static void keyboardEnter(void*, wl_keyboard*, std::uint32_t, wl_surface*, wl_array*) {}
  static void keyboardLeave(void*, wl_keyboard*, std::uint32_t, wl_surface*) {}
  static void keyboardKey(void* data, wl_keyboard*, std::uint32_t, std::uint32_t, std::uint32_t key,
                          std::uint32_t state) {
    auto* self = static_cast<WaylandWindow*>(data);
    xkb_keysym_t sym = self->xkbState_ ? xkb_state_key_get_one_sym(self->xkbState_, key + 8) : XKB_KEY_NoSymbol;
    bool const pressed = state == WL_KEYBOARD_KEY_STATE_PRESSED;
    Application::instance().eventQueue().post(InputEvent{.kind = pressed ? InputEvent::Kind::KeyDown
                                                                          : InputEvent::Kind::KeyUp,
                                                         .handle = self->handle_,
                                                         .key = keyFromXkb(sym),
                                                         .modifiers = self->currentModifiers_});
    if (pressed) {
      std::string text = utf8FromKeysym(sym);
      if (!text.empty()) {
        Application::instance().eventQueue().post(InputEvent{.kind = InputEvent::Kind::TextInput,
                                                             .handle = self->handle_,
                                                             .text = std::move(text)});
      }
    }
  }
  static void keyboardModifiers(void* data, wl_keyboard*, std::uint32_t, std::uint32_t depressed,
                                std::uint32_t latched, std::uint32_t locked, std::uint32_t group) {
    auto* self = static_cast<WaylandWindow*>(data);
    if (self->xkbState_) {
      xkb_state_update_mask(self->xkbState_, depressed, latched, locked, 0, 0, group);
      Modifiers mods = Modifiers::None;
      if (xkb_state_mod_name_is_active(self->xkbState_, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE) > 0) {
        mods = mods | Modifiers::Shift;
      }
      if (xkb_state_mod_name_is_active(self->xkbState_, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE) > 0) {
        mods = mods | Modifiers::Ctrl;
      }
      if (xkb_state_mod_name_is_active(self->xkbState_, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE) > 0) {
        mods = mods | Modifiers::Alt;
      }
      if (xkb_state_mod_name_is_active(self->xkbState_, XKB_MOD_NAME_LOGO, XKB_STATE_MODS_EFFECTIVE) > 0) {
        mods = mods | Modifiers::Meta;
      }
      self->currentModifiers_ = mods;
    }
  }
  static void keyboardRepeatInfo(void*, wl_keyboard*, std::int32_t, std::int32_t) {}

  void applyConfiguredSize(int width, int height) {
    size_ = {static_cast<float>(std::max(1, width)), static_cast<float>(std::max(1, height))};
    if (canvas_) canvas_->resize(static_cast<int>(std::lround(size_.width)),
                                 static_cast<int>(std::lround(size_.height)));
    if (fluxWindow_) fluxWindow_->updateCanvasDpiScale(dpiScaleX_, dpiScaleY_);
    Application::instance().eventQueue().post(WindowEvent{WindowEvent::Kind::Resize, handle_, size_});
    requestResizeRedraw();
  }

  void updateCanvasDpi() {
    if (fluxWindow_) fluxWindow_->updateCanvasDpiScale(dpiScaleX_, dpiScaleY_);
  }

  void requestServerSideDecorations() {
    if (!decorationManager_) {
      if (!warnedDecorationFallback_) {
        warnedDecorationFallback_ = true;
        std::fprintf(stderr, "Flux Wayland: compositor does not expose xdg-decoration; server-side decorations are unavailable.\n");
      }
      return;
    }
    decoration_ = zxdg_decoration_manager_v1_get_toplevel_decoration(decorationManager_, toplevel_);
    zxdg_toplevel_decoration_v1_add_listener(decoration_, &decorationListener_, this);
    zxdg_toplevel_decoration_v1_set_mode(decoration_, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
  }

  struct Output {
    WaylandWindow* owner = nullptr;
    wl_output* output = nullptr;
    std::uint32_t name = 0;
    float scale = 1.f;
    bool entered = false;
  };

  Output* findOutput(wl_output* output) {
    for (auto& candidate : outputs_) {
      if (candidate->output == output) return candidate.get();
    }
    return nullptr;
  }

  void updateEnteredScale() {
    float scale = 1.f;
    for (auto const& output : outputs_) {
      if (output->entered) scale = std::max(scale, output->scale);
    }
    if (std::abs(scale - dpiScaleX_) < 0.001f && std::abs(scale - dpiScaleY_) < 0.001f) return;
    dpiScaleX_ = scale;
    dpiScaleY_ = scale;
    wl_surface_set_buffer_scale(surface_, static_cast<std::int32_t>(std::max(1.f, std::round(scale))));
    if (fluxWindow_) fluxWindow_->updateCanvasDpiScale(dpiScaleX_, dpiScaleY_);
    Application::instance().eventQueue().post(WindowEvent{WindowEvent::Kind::DpiChanged, handle_, {}, dpiScaleX_});
    Application::instance().eventQueue().post(WindowEvent{WindowEvent::Kind::Resize, handle_, size_});
    requestResizeRedraw();
  }

  void requestResizeRedraw() {
    resizeRedrawPending_ = true;
    Application::instance().requestWindowRedraw(handle_);
    wakeEventLoop();
  }

  void drainWakePipe() {
    char buffer[64];
    while (read(wakePipe_[0], buffer, sizeof(buffer)) > 0) {}
  }

  void dispatchReadyEvents(int timeoutMs) {
    while (wl_display_prepare_read(display_) != 0) {
      wl_display_dispatch_pending(display_);
    }
    wl_display_flush(display_);

    pollfd fds[2]{{wl_display_get_fd(display_), POLLIN, 0}, {wakePipe_[0], POLLIN, 0}};
    int const rc = poll(fds, 2, timeoutMs < 0 ? -1 : timeoutMs);
    if (rc > 0 && (fds[1].revents & POLLIN)) {
      drainWakePipe();
    }
    if (rc > 0 && (fds[0].revents & POLLIN)) {
      wl_display_read_events(display_);
    } else {
      wl_display_cancel_read(display_);
    }
    wl_display_dispatch_pending(display_);
  }

  void flushDeferredRedraw() {
    if (!resizeRedrawPending_) return;
    resizeRedrawPending_ = false;
    Application::instance().eventQueue().dispatch();
    Application::instance().flushRedraw();
  }

  static inline wl_registry_listener registryListener_{registryGlobal, registryRemove};
  static inline xdg_wm_base_listener wmBaseListener_{wmPing};
  static inline wl_callback_listener frameCallbackListener_{frameDone};
  static inline wl_surface_listener surfaceListener_{surfaceEnter, surfaceLeave, surfacePreferredBufferScale,
                                                    surfacePreferredBufferTransform};
  static inline wl_output_listener outputListener_{outputGeometry, outputMode, outputDone, outputScale,
                                                  outputName, outputDescription};
  static inline xdg_surface_listener xdgSurfaceListener_{xdgConfigure};
  static inline xdg_toplevel_listener toplevelListener_{topConfigure, topClose, topConfigureBounds,
                                                       topCapabilities};
  static inline zxdg_toplevel_decoration_v1_listener decorationListener_{decorationConfigure};
  static inline wl_seat_listener seatListener_{seatCapabilities, seatName};
  static inline wl_pointer_listener pointerListener_{pointerEnter, pointerLeave, pointerMotion, pointerButton,
                                                     pointerAxis, pointerFrame, pointerAxisSource,
                                                     pointerAxisStop, pointerAxisDiscrete, pointerAxisValue120,
                                                     pointerAxisRelativeDirection};
  static inline wl_keyboard_listener keyboardListener_{keymap, keyboardEnter, keyboardLeave, keyboardKey,
                                                       keyboardModifiers, keyboardRepeatInfo};

  wl_display* display_ = nullptr;
  wl_registry* registry_ = nullptr;
  wl_compositor* compositor_ = nullptr;
  xdg_wm_base* wmBase_ = nullptr;
  zxdg_decoration_manager_v1* decorationManager_ = nullptr;
  wl_seat* seat_ = nullptr;
  std::vector<std::unique_ptr<Output>> outputs_;
  wl_pointer* pointer_ = nullptr;
  wl_keyboard* keyboard_ = nullptr;
  wl_surface* surface_ = nullptr;
  wl_callback* frameCallback_ = nullptr;
  xdg_surface* xdgSurface_ = nullptr;
  xdg_toplevel* toplevel_ = nullptr;
  zxdg_toplevel_decoration_v1* decoration_ = nullptr;
  Canvas* canvas_ = nullptr;
  xkb_context* xkbContext_ = nullptr;
  xkb_keymap* xkbKeymap_ = nullptr;
  xkb_state* xkbState_ = nullptr;

  Window* fluxWindow_ = nullptr;
  unsigned int handle_ = 0;
  Size size_{};
  std::string title_;
  float dpiScaleX_ = 1.f;
  float dpiScaleY_ = 1.f;
  bool fullscreen_ = false;
  bool configured_ = false;
  bool serverSideDecorationsActive_ = false;
  bool warnedDecorationFallback_ = false;
  bool loggedDecorationMode_ = false;
  int pendingWidth_ = 0;
  int pendingHeight_ = 0;
  Point pointerPos_{};
  std::uint8_t pressedButtons_ = 0;
  Modifiers currentModifiers_ = Modifiers::None;
  bool resizeRedrawPending_ = false;
  bool framePending_ = false;
  int wakePipe_[2]{-1, -1};
};

namespace detail {

std::unique_ptr<PlatformWindow> createPlatformWindow(WindowConfig const& config) {
  return std::make_unique<WaylandWindow>(config);
}

} // namespace detail
} // namespace flux
