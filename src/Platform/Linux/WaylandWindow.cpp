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
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace flux {

class WaylandWindow;

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
  switch (sym) {
  case XKB_KEY_a:
  case XKB_KEY_A: return keys::A;
  case XKB_KEY_b:
  case XKB_KEY_B: return keys::B;
  case XKB_KEY_c:
  case XKB_KEY_C: return keys::C;
  case XKB_KEY_d:
  case XKB_KEY_D: return keys::D;
  case XKB_KEY_e:
  case XKB_KEY_E: return keys::E;
  case XKB_KEY_f:
  case XKB_KEY_F: return keys::F;
  case XKB_KEY_g:
  case XKB_KEY_G: return keys::G;
  case XKB_KEY_h:
  case XKB_KEY_H: return keys::H;
  case XKB_KEY_i:
  case XKB_KEY_I: return keys::I;
  case XKB_KEY_j:
  case XKB_KEY_J: return keys::J;
  case XKB_KEY_k:
  case XKB_KEY_K: return keys::K;
  case XKB_KEY_l:
  case XKB_KEY_L: return keys::L;
  case XKB_KEY_m:
  case XKB_KEY_M: return keys::M;
  case XKB_KEY_n:
  case XKB_KEY_N: return keys::N;
  case XKB_KEY_o:
  case XKB_KEY_O: return keys::O;
  case XKB_KEY_p:
  case XKB_KEY_P: return keys::P;
  case XKB_KEY_q:
  case XKB_KEY_Q: return keys::Q;
  case XKB_KEY_r:
  case XKB_KEY_R: return keys::R;
  case XKB_KEY_s:
  case XKB_KEY_S: return keys::S;
  case XKB_KEY_t:
  case XKB_KEY_T: return keys::T;
  case XKB_KEY_u:
  case XKB_KEY_U: return keys::U;
  case XKB_KEY_v:
  case XKB_KEY_V: return keys::V;
  case XKB_KEY_w:
  case XKB_KEY_W: return keys::W;
  case XKB_KEY_x:
  case XKB_KEY_X: return keys::X;
  case XKB_KEY_y:
  case XKB_KEY_Y: return keys::Y;
  case XKB_KEY_z:
  case XKB_KEY_Z: return keys::Z;
  case XKB_KEY_0: return keys::Digit0;
  case XKB_KEY_1: return keys::Digit1;
  case XKB_KEY_2: return keys::Digit2;
  case XKB_KEY_3: return keys::Digit3;
  case XKB_KEY_4: return keys::Digit4;
  case XKB_KEY_5: return keys::Digit5;
  case XKB_KEY_6: return keys::Digit6;
  case XKB_KEY_7: return keys::Digit7;
  case XKB_KEY_8: return keys::Digit8;
  case XKB_KEY_9: return keys::Digit9;
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
  case XKB_KEY_F1: return keys::F1;
  case XKB_KEY_F2: return keys::F2;
  case XKB_KEY_F3: return keys::F3;
  case XKB_KEY_F4: return keys::F4;
  case XKB_KEY_F5: return keys::F5;
  case XKB_KEY_F6: return keys::F6;
  case XKB_KEY_F7: return keys::F7;
  case XKB_KEY_F8: return keys::F8;
  case XKB_KEY_F9: return keys::F9;
  case XKB_KEY_F10: return keys::F10;
  case XKB_KEY_F11: return keys::F11;
  case XKB_KEY_F12: return keys::F12;
  case XKB_KEY_minus: return keys::Minus;
  case XKB_KEY_equal: return keys::Equal;
  case XKB_KEY_bracketleft: return keys::LeftBracket;
  case XKB_KEY_bracketright: return keys::RightBracket;
  case XKB_KEY_semicolon: return keys::Semicolon;
  case XKB_KEY_apostrophe: return keys::Quote;
  case XKB_KEY_grave: return keys::Grave;
  case XKB_KEY_backslash: return keys::Backslash;
  case XKB_KEY_comma: return keys::Comma;
  case XKB_KEY_period: return keys::Period;
  case XKB_KEY_slash: return keys::Slash;
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

struct SharedWaylandConnection {
  wl_display* display = nullptr;
  wl_registry* registry = nullptr;
  wl_compositor* compositor = nullptr;
  xdg_wm_base* wmBase = nullptr;
  zxdg_decoration_manager_v1* decorationManager = nullptr;
  wl_seat* seat = nullptr;
  wl_pointer* pointer = nullptr;
  wl_keyboard* keyboard = nullptr;
  xkb_context* xkbContext = nullptr;
  xkb_keymap* xkbKeymap = nullptr;
  xkb_state* xkbState = nullptr;
  struct Output {
    wl_output* output = nullptr;
    std::uint32_t name = 0;
    float scale = 1.f;
  };
  std::vector<std::unique_ptr<Output>> outputs;
  std::vector<WaylandWindow*> windows;
  WaylandWindow* pointerFocus = nullptr;
  WaylandWindow* keyboardFocus = nullptr;
  unsigned int refs = 0;
};

std::mutex gWaylandConnectionMutex;
SharedWaylandConnection gWaylandConnection;

void sharedRegistryGlobal(void* data, wl_registry* registry, std::uint32_t name,
                          char const* interface, std::uint32_t version);
void sharedRegistryRemove(void* data, wl_registry*, std::uint32_t name);
void sharedWmPing(void*, xdg_wm_base* base, std::uint32_t serial);
void sharedSeatCapabilities(void* data, wl_seat* seat, std::uint32_t caps);
void sharedSeatName(void*, wl_seat*, char const*);
void sharedOutputGeometry(void*, wl_output*, std::int32_t, std::int32_t, std::int32_t, std::int32_t,
                          std::int32_t, char const*, char const*, std::int32_t);
void sharedOutputMode(void*, wl_output*, std::uint32_t, std::int32_t, std::int32_t, std::int32_t);
void sharedOutputDone(void*, wl_output*);
void sharedOutputScale(void* data, wl_output*, std::int32_t scale);
void sharedOutputName(void*, wl_output*, char const*);
void sharedOutputDescription(void*, wl_output*, char const*);
void sharedPointerEnter(void* data, wl_pointer*, std::uint32_t, wl_surface* surface, wl_fixed_t x, wl_fixed_t y);
void sharedPointerLeave(void* data, wl_pointer*, std::uint32_t, wl_surface* surface);
void sharedPointerMotion(void* data, wl_pointer*, std::uint32_t, wl_fixed_t x, wl_fixed_t y);
void sharedPointerButton(void* data, wl_pointer*, std::uint32_t, std::uint32_t, std::uint32_t button,
                         std::uint32_t state);
void sharedPointerAxis(void* data, wl_pointer*, std::uint32_t, std::uint32_t axis, wl_fixed_t value);
void sharedPointerFrame(void*, wl_pointer*);
void sharedPointerAxisSource(void*, wl_pointer*, std::uint32_t);
void sharedPointerAxisStop(void*, wl_pointer*, std::uint32_t, std::uint32_t);
void sharedPointerAxisDiscrete(void*, wl_pointer*, std::uint32_t, std::int32_t);
void sharedPointerAxisValue120(void*, wl_pointer*, std::uint32_t, std::int32_t);
void sharedPointerAxisRelativeDirection(void*, wl_pointer*, std::uint32_t, std::uint32_t);
void sharedKeymap(void* data, wl_keyboard*, std::uint32_t format, int fd, std::uint32_t size);
void sharedKeyboardEnter(void* data, wl_keyboard*, std::uint32_t, wl_surface* surface, wl_array*);
void sharedKeyboardLeave(void* data, wl_keyboard*, std::uint32_t, wl_surface* surface);
void sharedKeyboardKey(void* data, wl_keyboard*, std::uint32_t, std::uint32_t, std::uint32_t key,
                       std::uint32_t state);
void sharedKeyboardModifiers(void* data, wl_keyboard*, std::uint32_t, std::uint32_t depressed,
                             std::uint32_t latched, std::uint32_t locked, std::uint32_t group);
void sharedKeyboardRepeatInfo(void*, wl_keyboard*, std::int32_t, std::int32_t);

wl_registry_listener const sharedRegistryListener{sharedRegistryGlobal, sharedRegistryRemove};
xdg_wm_base_listener const sharedWmBaseListener{sharedWmPing};
wl_seat_listener const sharedSeatListener{sharedSeatCapabilities, sharedSeatName};
wl_output_listener const sharedOutputListener{sharedOutputGeometry, sharedOutputMode, sharedOutputDone,
                                             sharedOutputScale, sharedOutputName, sharedOutputDescription};
wl_pointer_listener const sharedPointerListener{sharedPointerEnter, sharedPointerLeave, sharedPointerMotion,
                                               sharedPointerButton, sharedPointerAxis, sharedPointerFrame,
                                               sharedPointerAxisSource, sharedPointerAxisStop,
                                               sharedPointerAxisDiscrete, sharedPointerAxisValue120,
                                               sharedPointerAxisRelativeDirection};
wl_keyboard_listener const sharedKeyboardListener{sharedKeymap, sharedKeyboardEnter, sharedKeyboardLeave,
                                                 sharedKeyboardKey, sharedKeyboardModifiers,
                                                 sharedKeyboardRepeatInfo};

SharedWaylandConnection* acquireWaylandConnection() {
  std::lock_guard lock(gWaylandConnectionMutex);
  if (!gWaylandConnection.display) {
    gWaylandConnection.display = wl_display_connect(nullptr);
    if (!gWaylandConnection.display) {
      throw std::runtime_error("Failed to connect to Wayland display");
    }
    gWaylandConnection.xkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!gWaylandConnection.xkbContext) {
      wl_display_disconnect(gWaylandConnection.display);
      gWaylandConnection.display = nullptr;
      throw std::runtime_error("Failed to create XKB context");
    }
    gWaylandConnection.registry = wl_display_get_registry(gWaylandConnection.display);
    wl_registry_add_listener(gWaylandConnection.registry, &sharedRegistryListener, &gWaylandConnection);
    wl_display_roundtrip(gWaylandConnection.display);
    if (!gWaylandConnection.compositor || !gWaylandConnection.wmBase) {
      throw std::runtime_error("Wayland compositor does not expose required xdg-shell globals");
    }
  }
  ++gWaylandConnection.refs;
  return &gWaylandConnection;
}

void releaseWaylandConnection() {
  std::lock_guard lock(gWaylandConnectionMutex);
  if (gWaylandConnection.refs == 0) return;
  --gWaylandConnection.refs;
  if (gWaylandConnection.refs != 0) return;
  if (gWaylandConnection.keyboard) {
    wl_keyboard_destroy(gWaylandConnection.keyboard);
    gWaylandConnection.keyboard = nullptr;
  }
  if (gWaylandConnection.pointer) {
    wl_pointer_destroy(gWaylandConnection.pointer);
    gWaylandConnection.pointer = nullptr;
  }
  if (gWaylandConnection.seat) {
    wl_seat_destroy(gWaylandConnection.seat);
    gWaylandConnection.seat = nullptr;
  }
  for (auto& output : gWaylandConnection.outputs) {
    if (output->output) wl_output_destroy(output->output);
  }
  gWaylandConnection.outputs.clear();
  if (gWaylandConnection.decorationManager) {
    zxdg_decoration_manager_v1_destroy(gWaylandConnection.decorationManager);
    gWaylandConnection.decorationManager = nullptr;
  }
  if (gWaylandConnection.wmBase) {
    xdg_wm_base_destroy(gWaylandConnection.wmBase);
    gWaylandConnection.wmBase = nullptr;
  }
  if (gWaylandConnection.compositor) {
    wl_compositor_destroy(gWaylandConnection.compositor);
    gWaylandConnection.compositor = nullptr;
  }
  if (gWaylandConnection.registry) {
    wl_registry_destroy(gWaylandConnection.registry);
    gWaylandConnection.registry = nullptr;
  }
  if (gWaylandConnection.xkbState) {
    xkb_state_unref(gWaylandConnection.xkbState);
    gWaylandConnection.xkbState = nullptr;
  }
  if (gWaylandConnection.xkbKeymap) {
    xkb_keymap_unref(gWaylandConnection.xkbKeymap);
    gWaylandConnection.xkbKeymap = nullptr;
  }
  if (gWaylandConnection.xkbContext) {
    xkb_context_unref(gWaylandConnection.xkbContext);
    gWaylandConnection.xkbContext = nullptr;
  }
  if (gWaylandConnection.display) {
    wl_display_disconnect(gWaylandConnection.display);
    gWaylandConnection.display = nullptr;
  }
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
    SharedWaylandConnection* shared = acquireWaylandConnection();
    shared_ = shared;
    display_ = shared->display;
    surface_ = wl_compositor_create_surface(shared->compositor);
    shared_->windows.push_back(this);
    wl_surface_add_listener(surface_, &surfaceListener_, this);
    wl_surface_set_buffer_scale(surface_, static_cast<std::int32_t>(std::lround(dpiScaleX_)));
    xdgSurface_ = xdg_wm_base_get_xdg_surface(shared->wmBase, surface_);
    xdg_surface_add_listener(xdgSurface_, &xdgSurfaceListener_, this);
    toplevel_ = xdg_surface_get_toplevel(xdgSurface_);
    xdg_toplevel_add_listener(toplevel_, &toplevelListener_, this);
    xdg_toplevel_set_title(toplevel_, title_.c_str());
    appId_ = Application::instance().name();
    xdg_toplevel_set_app_id(toplevel_, appId_.c_str());
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
    if (shared_) {
      shared_->windows.erase(std::remove(shared_->windows.begin(), shared_->windows.end(), this),
                             shared_->windows.end());
      if (shared_->pointerFocus == this) shared_->pointerFocus = nullptr;
      if (shared_->keyboardFocus == this) shared_->keyboardFocus = nullptr;
    }
    if (frameCallback_) wl_callback_destroy(frameCallback_);
    if (decoration_) zxdg_toplevel_decoration_v1_destroy(decoration_);
    if (toplevel_) xdg_toplevel_destroy(toplevel_);
    if (xdgSurface_) xdg_surface_destroy(xdgSurface_);
    if (surface_) wl_surface_destroy(surface_);
    if (shared_) releaseWaylandConnection();
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

  wl_surface* waylandSurface() const noexcept { return surface_; }

  void handlePointerEnter(wl_fixed_t x, wl_fixed_t y) {
    pointerPos_ = logicalPointFromFixed(x, y, dpiScaleX_, dpiScaleY_);
  }

  void handlePointerLeave() {
    pressedButtons_ = 0;
  }

  void handlePointerMotion(wl_fixed_t x, wl_fixed_t y) {
    pointerPos_ = logicalPointFromFixed(x, y, dpiScaleX_, dpiScaleY_);
    Application::instance().eventQueue().post(InputEvent{.kind = InputEvent::Kind::PointerMove,
                                                         .handle = handle_,
                                                         .position = pointerPos_,
                                                         .pressedButtons = pressedButtons_});
  }

  void handlePointerButton(std::uint32_t button, std::uint32_t state) {
    std::uint8_t const bit = button == BTN_LEFT ? 1u : button == BTN_RIGHT ? 2u : button == BTN_MIDDLE ? 4u : 0u;
    if (state == WL_POINTER_BUTTON_STATE_PRESSED) pressedButtons_ |= bit;
    else pressedButtons_ &= static_cast<std::uint8_t>(~bit);
    Application::instance().eventQueue().post(InputEvent{.kind = state == WL_POINTER_BUTTON_STATE_PRESSED
                                                                     ? InputEvent::Kind::PointerDown
                                                                     : InputEvent::Kind::PointerUp,
                                                         .handle = handle_,
                                                         .position = pointerPos_,
                                                         .button = mouseButtonFromLinux(button),
                                                         .pressedButtons = pressedButtons_});
  }

  void handlePointerAxis(std::uint32_t axis, wl_fixed_t value) {
    float dx = 0.f, dy = 0.f;
    float const v = static_cast<float>(wl_fixed_to_double(value));
    if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) dx = v;
    else dy = v;
    Application::instance().eventQueue().post(InputEvent{.kind = InputEvent::Kind::Scroll,
                                                         .handle = handle_,
                                                         .position = pointerPos_,
                                                         .scrollDelta = {dx, dy},
                                                         .preciseScrollDelta = true,
                                                         .pressedButtons = pressedButtons_});
  }

  void handleKeyboardKey(xkb_state* stateForKeyboard, std::uint32_t key, std::uint32_t state) {
    xkb_keysym_t sym = stateForKeyboard ? xkb_state_key_get_one_sym(stateForKeyboard, key + 8) : XKB_KEY_NoSymbol;
    bool const pressed = state == WL_KEYBOARD_KEY_STATE_PRESSED;
    Application::instance().eventQueue().post(InputEvent{.kind = pressed ? InputEvent::Kind::KeyDown
                                                                          : InputEvent::Kind::KeyUp,
                                                         .handle = handle_,
                                                         .key = keyFromXkb(sym),
                                                         .modifiers = currentModifiers_});
    if (pressed) {
      std::string text = utf8FromKeysym(sym);
      if (!text.empty()) {
        Application::instance().eventQueue().post(InputEvent{.kind = InputEvent::Kind::TextInput,
                                                             .handle = handle_,
                                                             .text = std::move(text)});
      }
    }
  }

  void handleKeyboardModifiers(xkb_state* stateForKeyboard, std::uint32_t depressed,
                               std::uint32_t latched, std::uint32_t locked, std::uint32_t group) {
    if (!stateForKeyboard) return;
    xkb_state_update_mask(stateForKeyboard, depressed, latched, locked, 0, 0, group);
    Modifiers mods = Modifiers::None;
    if (xkb_state_mod_name_is_active(stateForKeyboard, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE) > 0) {
      mods = mods | Modifiers::Shift;
    }
    if (xkb_state_mod_name_is_active(stateForKeyboard, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE) > 0) {
      mods = mods | Modifiers::Ctrl;
    }
    if (xkb_state_mod_name_is_active(stateForKeyboard, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE) > 0) {
      mods = mods | Modifiers::Alt;
    }
    if (xkb_state_mod_name_is_active(stateForKeyboard, XKB_MOD_NAME_LOGO, XKB_STATE_MODS_EFFECTIVE) > 0) {
      mods = mods | Modifiers::Meta;
    }
    currentModifiers_ = mods;
  }

  void handleOutputRemoved(wl_output* output) {
    enteredOutputs_.erase(std::remove(enteredOutputs_.begin(), enteredOutputs_.end(), output),
                          enteredOutputs_.end());
    updateEnteredScale();
  }

  void handleOutputScaleChanged(wl_output* output) {
    if (std::find(enteredOutputs_.begin(), enteredOutputs_.end(), output) != enteredOutputs_.end()) {
      updateEnteredScale();
    }
  }

private:
  static void frameDone(void* data, wl_callback* callback, std::uint32_t) {
    auto* self = static_cast<WaylandWindow*>(data);
    if (callback == self->frameCallback_) {
      wl_callback_destroy(self->frameCallback_);
      self->frameCallback_ = nullptr;
    } else {
      wl_callback_destroy(callback);
    }
    if (!self->framePending_) return;
    self->framePending_ = false;
    auto& queue = Application::instance().eventQueue();
    queue.post(FrameEvent{nowNanos(), self->handle_});
    queue.dispatch();
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

  static void surfaceEnter(void* data, wl_surface*, wl_output* output) {
    auto* self = static_cast<WaylandWindow*>(data);
    if (std::find(self->enteredOutputs_.begin(), self->enteredOutputs_.end(), output) == self->enteredOutputs_.end()) {
      self->enteredOutputs_.push_back(output);
    }
    self->updateEnteredScale();
  }
  static void surfaceLeave(void* data, wl_surface*, wl_output* output) {
    auto* self = static_cast<WaylandWindow*>(data);
    self->enteredOutputs_.erase(std::remove(self->enteredOutputs_.begin(), self->enteredOutputs_.end(), output),
                                self->enteredOutputs_.end());
    self->updateEnteredScale();
  }
  static void surfacePreferredBufferScale(void* data, wl_surface*, std::int32_t factor) {
    auto* self = static_cast<WaylandWindow*>(data);
    factor = std::max(1, factor);
    self->dpiScaleX_ = safeScale(static_cast<float>(factor));
    self->dpiScaleY_ = self->dpiScaleX_;
    wl_surface_set_buffer_scale(self->surface_, factor);
    if (self->fluxWindow_) self->fluxWindow_->updateCanvasDpiScale(self->dpiScaleX_, self->dpiScaleY_);
    Application::instance().eventQueue().post(WindowEvent{.kind = WindowEvent::Kind::DpiChanged,
                                                          .handle = self->handle_,
                                                          .dpi = self->dpiScaleX_,
                                                          .dpiX = self->dpiScaleX_,
                                                          .dpiY = self->dpiScaleY_});
    Application::instance().eventQueue().post(WindowEvent{WindowEvent::Kind::Resize, self->handle_, self->size_});
    self->requestResizeRedraw();
  }
  static void surfacePreferredBufferTransform(void*, wl_surface*, std::uint32_t) {}

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
    if (!shared_ || !shared_->decorationManager) {
      if (!warnedDecorationFallback_) {
        warnedDecorationFallback_ = true;
        std::fprintf(stderr, "Flux Wayland: compositor does not expose xdg-decoration; server-side decorations are unavailable.\n");
      }
      return;
    }
    decoration_ = zxdg_decoration_manager_v1_get_toplevel_decoration(shared_->decorationManager, toplevel_);
    zxdg_toplevel_decoration_v1_add_listener(decoration_, &decorationListener_, this);
    zxdg_toplevel_decoration_v1_set_mode(decoration_, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
  }

  float outputScale(wl_output* output) const {
    if (!shared_) return 1.f;
    for (auto const& candidate : shared_->outputs) {
      if (candidate->output == output) return candidate->scale;
    }
    return 1.f;
  }

  void updateEnteredScale() {
    float scale = 1.f;
    for (wl_output* output : enteredOutputs_) {
      scale = std::max(scale, outputScale(output));
    }
    if (std::abs(scale - dpiScaleX_) < 0.001f && std::abs(scale - dpiScaleY_) < 0.001f) return;
    dpiScaleX_ = scale;
    dpiScaleY_ = scale;
    wl_surface_set_buffer_scale(surface_, static_cast<std::int32_t>(std::max(1.f, std::round(scale))));
    if (fluxWindow_) fluxWindow_->updateCanvasDpiScale(dpiScaleX_, dpiScaleY_);
    Application::instance().eventQueue().post(WindowEvent{.kind = WindowEvent::Kind::DpiChanged,
                                                          .handle = handle_,
                                                          .dpi = dpiScaleX_,
                                                          .dpiX = dpiScaleX_,
                                                          .dpiY = dpiScaleY_});
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

  static inline wl_callback_listener frameCallbackListener_{frameDone};
  static inline wl_surface_listener surfaceListener_{surfaceEnter, surfaceLeave, surfacePreferredBufferScale,
                                                    surfacePreferredBufferTransform};
  static inline xdg_surface_listener xdgSurfaceListener_{xdgConfigure};
  static inline xdg_toplevel_listener toplevelListener_{topConfigure, topClose, topConfigureBounds,
                                                       topCapabilities};
  static inline zxdg_toplevel_decoration_v1_listener decorationListener_{decorationConfigure};

  wl_display* display_ = nullptr;
  std::vector<wl_output*> enteredOutputs_;
  wl_surface* surface_ = nullptr;
  wl_callback* frameCallback_ = nullptr;
  xdg_surface* xdgSurface_ = nullptr;
  xdg_toplevel* toplevel_ = nullptr;
  zxdg_toplevel_decoration_v1* decoration_ = nullptr;
  Canvas* canvas_ = nullptr;
  SharedWaylandConnection* shared_ = nullptr;

  Window* fluxWindow_ = nullptr;
  unsigned int handle_ = 0;
  Size size_{};
  std::string title_;
  std::string appId_;
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

namespace {

WaylandWindow* windowForSurface(SharedWaylandConnection* shared, wl_surface* surface) {
  if (!shared || !surface) return nullptr;
  for (WaylandWindow* window : shared->windows) {
    if (window && window->waylandSurface() == surface) return window;
  }
  return nullptr;
}

void refreshWindowsForOutput(SharedWaylandConnection* shared, wl_output* output) {
  if (!shared) return;
  for (WaylandWindow* window : shared->windows) {
    if (window) window->handleOutputScaleChanged(output);
  }
}

void sharedRegistryGlobal(void* data, wl_registry* registry, std::uint32_t name,
                          char const* interface, std::uint32_t version) {
  auto* shared = static_cast<SharedWaylandConnection*>(data);
  if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
    shared->compositor = static_cast<wl_compositor*>(
        wl_registry_bind(registry, name, &wl_compositor_interface, std::min(version, 4u)));
  } else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0) {
    shared->wmBase = static_cast<xdg_wm_base*>(
        wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));
    xdg_wm_base_add_listener(shared->wmBase, &sharedWmBaseListener, shared);
  } else if (std::strcmp(interface, wl_seat_interface.name) == 0) {
    shared->seat = static_cast<wl_seat*>(
        wl_registry_bind(registry, name, &wl_seat_interface, std::min(version, 7u)));
    wl_seat_add_listener(shared->seat, &sharedSeatListener, shared);
  } else if (std::strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0) {
    shared->decorationManager = static_cast<zxdg_decoration_manager_v1*>(
        wl_registry_bind(registry, name, &zxdg_decoration_manager_v1_interface, 1));
  } else if (std::strcmp(interface, wl_output_interface.name) == 0) {
    auto output = std::make_unique<SharedWaylandConnection::Output>();
    output->name = name;
    output->output = static_cast<wl_output*>(
        wl_registry_bind(registry, name, &wl_output_interface, std::min(version, 2u)));
    wl_output_add_listener(output->output, &sharedOutputListener, output.get());
    shared->outputs.push_back(std::move(output));
  }
}

void sharedRegistryRemove(void* data, wl_registry*, std::uint32_t name) {
  auto* shared = static_cast<SharedWaylandConnection*>(data);
  auto it = std::find_if(shared->outputs.begin(), shared->outputs.end(),
                         [&](auto const& output) { return output->name == name; });
  if (it == shared->outputs.end()) return;
  wl_output* removed = (*it)->output;
  for (WaylandWindow* window : shared->windows) {
    if (window) window->handleOutputRemoved(removed);
  }
  if ((*it)->output) wl_output_destroy((*it)->output);
  shared->outputs.erase(it);
}

void sharedWmPing(void*, xdg_wm_base* base, std::uint32_t serial) {
  xdg_wm_base_pong(base, serial);
}

void sharedSeatCapabilities(void* data, wl_seat* seat, std::uint32_t caps) {
  auto* shared = static_cast<SharedWaylandConnection*>(data);
  if ((caps & WL_SEAT_CAPABILITY_POINTER) && !shared->pointer) {
    shared->pointer = wl_seat_get_pointer(seat);
    wl_pointer_add_listener(shared->pointer, &sharedPointerListener, shared);
  }
  if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !shared->keyboard) {
    shared->keyboard = wl_seat_get_keyboard(seat);
    wl_keyboard_add_listener(shared->keyboard, &sharedKeyboardListener, shared);
  }
}

void sharedSeatName(void*, wl_seat*, char const*) {}
void sharedOutputGeometry(void*, wl_output*, std::int32_t, std::int32_t, std::int32_t, std::int32_t,
                          std::int32_t, char const*, char const*, std::int32_t) {}
void sharedOutputMode(void*, wl_output*, std::uint32_t, std::int32_t, std::int32_t, std::int32_t) {}
void sharedOutputDone(void*, wl_output*) {}
void sharedOutputScale(void* data, wl_output* output, std::int32_t scale) {
  auto* sharedOutput = static_cast<SharedWaylandConnection::Output*>(data);
  sharedOutput->scale = safeScale(static_cast<float>(std::max(1, scale)));
  refreshWindowsForOutput(&gWaylandConnection, output);
}
void sharedOutputName(void*, wl_output*, char const*) {}
void sharedOutputDescription(void*, wl_output*, char const*) {}

void sharedPointerEnter(void* data, wl_pointer*, std::uint32_t, wl_surface* surface, wl_fixed_t x, wl_fixed_t y) {
  auto* shared = static_cast<SharedWaylandConnection*>(data);
  shared->pointerFocus = windowForSurface(shared, surface);
  if (shared->pointerFocus) shared->pointerFocus->handlePointerEnter(x, y);
}
void sharedPointerLeave(void* data, wl_pointer*, std::uint32_t, wl_surface* surface) {
  auto* shared = static_cast<SharedWaylandConnection*>(data);
  WaylandWindow* window = windowForSurface(shared, surface);
  if (window) window->handlePointerLeave();
  if (!surface || shared->pointerFocus == window) shared->pointerFocus = nullptr;
}
void sharedPointerMotion(void* data, wl_pointer*, std::uint32_t, wl_fixed_t x, wl_fixed_t y) {
  auto* shared = static_cast<SharedWaylandConnection*>(data);
  if (shared->pointerFocus) shared->pointerFocus->handlePointerMotion(x, y);
}
void sharedPointerButton(void* data, wl_pointer*, std::uint32_t, std::uint32_t, std::uint32_t button,
                         std::uint32_t state) {
  auto* shared = static_cast<SharedWaylandConnection*>(data);
  if (shared->pointerFocus) shared->pointerFocus->handlePointerButton(button, state);
}
void sharedPointerAxis(void* data, wl_pointer*, std::uint32_t, std::uint32_t axis, wl_fixed_t value) {
  auto* shared = static_cast<SharedWaylandConnection*>(data);
  if (shared->pointerFocus) shared->pointerFocus->handlePointerAxis(axis, value);
}
void sharedPointerFrame(void*, wl_pointer*) {}
void sharedPointerAxisSource(void*, wl_pointer*, std::uint32_t) {}
void sharedPointerAxisStop(void*, wl_pointer*, std::uint32_t, std::uint32_t) {}
void sharedPointerAxisDiscrete(void*, wl_pointer*, std::uint32_t, std::int32_t) {}
void sharedPointerAxisValue120(void*, wl_pointer*, std::uint32_t, std::int32_t) {}
void sharedPointerAxisRelativeDirection(void*, wl_pointer*, std::uint32_t, std::uint32_t) {}

void sharedKeymap(void* data, wl_keyboard*, std::uint32_t format, int fd, std::uint32_t size) {
  auto* shared = static_cast<SharedWaylandConnection*>(data);
  if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
    close(fd);
    return;
  }
  char* map = static_cast<char*>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
  if (map == MAP_FAILED) {
    close(fd);
    return;
  }
  xkb_keymap* keymap = xkb_keymap_new_from_string(shared->xkbContext, map, XKB_KEYMAP_FORMAT_TEXT_V1,
                                                  XKB_KEYMAP_COMPILE_NO_FLAGS);
  munmap(map, size);
  close(fd);
  if (!keymap) return;
  xkb_state* state = xkb_state_new(keymap);
  if (!state) {
    xkb_keymap_unref(keymap);
    return;
  }
  if (shared->xkbState) xkb_state_unref(shared->xkbState);
  if (shared->xkbKeymap) xkb_keymap_unref(shared->xkbKeymap);
  shared->xkbKeymap = keymap;
  shared->xkbState = state;
}

void sharedKeyboardEnter(void* data, wl_keyboard*, std::uint32_t, wl_surface* surface, wl_array*) {
  auto* shared = static_cast<SharedWaylandConnection*>(data);
  shared->keyboardFocus = windowForSurface(shared, surface);
}
void sharedKeyboardLeave(void* data, wl_keyboard*, std::uint32_t, wl_surface* surface) {
  auto* shared = static_cast<SharedWaylandConnection*>(data);
  WaylandWindow* window = windowForSurface(shared, surface);
  if (!surface || shared->keyboardFocus == window) shared->keyboardFocus = nullptr;
}
void sharedKeyboardKey(void* data, wl_keyboard*, std::uint32_t, std::uint32_t, std::uint32_t key,
                       std::uint32_t state) {
  auto* shared = static_cast<SharedWaylandConnection*>(data);
  if (shared->keyboardFocus) shared->keyboardFocus->handleKeyboardKey(shared->xkbState, key, state);
}
void sharedKeyboardModifiers(void* data, wl_keyboard*, std::uint32_t, std::uint32_t depressed,
                             std::uint32_t latched, std::uint32_t locked, std::uint32_t group) {
  auto* shared = static_cast<SharedWaylandConnection*>(data);
  if (shared->keyboardFocus) {
    shared->keyboardFocus->handleKeyboardModifiers(shared->xkbState, depressed, latched, locked, group);
  }
}
void sharedKeyboardRepeatInfo(void*, wl_keyboard*, std::int32_t, std::int32_t) {}

} // namespace

namespace detail {

std::unique_ptr<PlatformWindow> createPlatformWindow(WindowConfig const& config) {
  return std::make_unique<WaylandWindow>(config);
}

} // namespace detail
} // namespace flux
