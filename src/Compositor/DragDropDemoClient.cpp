#include "DemoClientSupport.hpp"
#include "xdg-shell-client-protocol.h"

#include <linux/input-event-codes.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <unistd.h>
#include <wayland-client.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

constexpr int kWidth = 260;
constexpr int kHeight = 170;
constexpr int kStride = kWidth * 4;
constexpr int kBufferSize = kStride * kHeight;
constexpr char const* kMimeText = "text/plain;charset=utf-8";
constexpr char const* kPayload = "Flux drag-and-drop payload";

std::atomic<bool> gRunning{true};

struct DemoClient;

struct DemoWindow {
  DemoClient* client = nullptr;
  wl_surface* surface = nullptr;
  wl_buffer* buffer = nullptr;
  xdg_surface* xdgSurface = nullptr;
  xdg_toplevel* toplevel = nullptr;
  void* pixels = nullptr;
  int fd = -1;
  bool configured = false;
  bool source = false;
  bool blockVisible = false;
  bool hover = false;
};

struct DemoClient {
  wl_display* display = nullptr;
  wl_registry* registry = nullptr;
  wl_compositor* compositor = nullptr;
  wl_shm* shm = nullptr;
  wl_seat* seat = nullptr;
  wl_pointer* pointer = nullptr;
  wl_data_device_manager* dataManager = nullptr;
  wl_data_device* dataDevice = nullptr;
  wl_data_source* dataSource = nullptr;
  wl_data_offer* currentOffer = nullptr;
  wl_surface* pointerSurface = nullptr;
  xdg_wm_base* wmBase = nullptr;
  DemoWindow sourceWindow;
  DemoWindow targetWindow;
  int receiveFd = -1;
};

int createSharedMemoryFile(std::size_t size) {
  int fd = memfd_create("lambda-window-manager-dnd-demo", MFD_CLOEXEC | MFD_ALLOW_SEALING);
  if (fd < 0) throw std::runtime_error(std::string("memfd_create failed: ") + std::strerror(errno));
  if (ftruncate(fd, static_cast<off_t>(size)) != 0) {
    close(fd);
    throw std::runtime_error(std::string("ftruncate failed: ") + std::strerror(errno));
  }
  return fd;
}

void setPixel(std::uint8_t* dst, int x, int y, std::uint8_t red, std::uint8_t green, std::uint8_t blue) {
  std::size_t const offset = static_cast<std::size_t>(y) * kStride + static_cast<std::size_t>(x) * 4u;
  dst[offset + 0u] = blue;
  dst[offset + 1u] = green;
  dst[offset + 2u] = red;
  dst[offset + 3u] = 0xff;
}

bool inRect(int x, int y, int left, int top, int width, int height) {
  return x >= left && x < left + width && y >= top && y < top + height;
}

void fillPattern(DemoWindow& window) {
  auto* dst = static_cast<std::uint8_t*>(window.pixels);
  for (int y = 0; y < kHeight; ++y) {
    for (int x = 0; x < kWidth; ++x) {
      bool const grid = (x % 28) < 3 || (y % 28) < 3;
      if (window.source) {
        setPixel(dst, x, y, grid ? 250 : 150, grid ? 245 : 55, grid ? 210 : 70);
      } else {
        std::uint8_t const red = window.hover ? 65 : 42;
        std::uint8_t const green = window.hover ? 170 : 135;
        std::uint8_t const blue = window.hover ? 245 : 215;
        setPixel(dst, x, y, grid ? 210 : red, grid ? 245 : green, grid ? 250 : blue);
      }
      if (window.blockVisible && inRect(x, y, 78, 46, 104, 78)) {
        bool const border = x < 84 || x >= 176 || y < 52 || y >= 118;
        setPixel(dst, x, y, border ? 255 : 236, border ? 255 : 74, border ? 255 : 64);
      }
    }
  }
}

void repaintWindow(DemoWindow& window) {
  if (!window.surface || !window.buffer || !window.pixels) return;
  fillPattern(window);
  wl_surface_attach(window.surface, window.buffer, 0, 0);
  wl_surface_damage_buffer(window.surface, 0, 0, kWidth, kHeight);
  wl_surface_commit(window.surface);
  if (window.client && window.client->display) wl_display_flush(window.client->display);
}

void createBuffer(DemoClient& client, DemoWindow& window) {
  window.fd = createSharedMemoryFile(kBufferSize);
  window.pixels = mmap(nullptr, kBufferSize, PROT_READ | PROT_WRITE, MAP_SHARED, window.fd, 0);
  if (window.pixels == MAP_FAILED) {
    window.pixels = nullptr;
    throw std::runtime_error(std::string("mmap failed: ") + std::strerror(errno));
  }
  fillPattern(window);
  wl_shm_pool* pool = wl_shm_create_pool(client.shm, window.fd, kBufferSize);
  window.buffer = wl_shm_pool_create_buffer(pool, 0, kWidth, kHeight, kStride, WL_SHM_FORMAT_XRGB8888);
  wl_shm_pool_destroy(pool);
}

void wmBasePing(void*, xdg_wm_base* wmBase, std::uint32_t serial) {
  xdg_wm_base_pong(wmBase, serial);
}

xdg_wm_base_listener const kWmBaseListener{wmBasePing};

void xdgSurfaceConfigure(void* data, xdg_surface* surface, std::uint32_t serial) {
  auto* window = static_cast<DemoWindow*>(data);
  xdg_surface_ack_configure(surface, serial);
  window->configured = true;
}

xdg_surface_listener const kXdgSurfaceListener{xdgSurfaceConfigure};

void toplevelConfigure(void*, xdg_toplevel*, std::int32_t, std::int32_t, wl_array*) {}

void toplevelClose(void*, xdg_toplevel*) {
  gRunning.store(false, std::memory_order_relaxed);
}

void toplevelConfigureBounds(void*, xdg_toplevel*, std::int32_t, std::int32_t) {}

void toplevelWmCapabilities(void*, xdg_toplevel*, wl_array*) {}

xdg_toplevel_listener const kToplevelListener{
    .configure = toplevelConfigure,
    .close = toplevelClose,
    .configure_bounds = toplevelConfigureBounds,
    .wm_capabilities = toplevelWmCapabilities,
};

void sourceTarget(void*, wl_data_source*, char const* mimeType) {
  std::fprintf(stderr, "lambda-window-manager-dnd-demo: target accepted %s\n", mimeType ? mimeType : "(none)");
}

void sourceSend(void*, wl_data_source*, char const* mimeType, int fd) {
  if (!mimeType || std::strcmp(mimeType, kMimeText) != 0) {
    close(fd);
    return;
  }
  write(fd, kPayload, std::strlen(kPayload));
  close(fd);
  std::fprintf(stderr, "lambda-window-manager-dnd-demo: source sent payload\n");
}

void sourceCancelled(void* data, wl_data_source*) {
  auto* client = static_cast<DemoClient*>(data);
  client->sourceWindow.blockVisible = true;
  client->targetWindow.hover = false;
  repaintWindow(client->sourceWindow);
  repaintWindow(client->targetWindow);
  std::fprintf(stderr, "lambda-window-manager-dnd-demo: drag cancelled\n");
}

void sourceDropPerformed(void*, wl_data_source*) {
  std::fprintf(stderr, "lambda-window-manager-dnd-demo: drop performed\n");
}

void sourceFinished(void*, wl_data_source*) {
  std::fprintf(stderr, "lambda-window-manager-dnd-demo: drag finished\n");
}

char const* actionName(std::uint32_t action) {
  switch (action) {
  case WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY: return "copy";
  case WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE: return "move";
  case WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK: return "ask";
  default: return "none";
  }
}

void sourceAction(void*, wl_data_source*, std::uint32_t action) {
  std::fprintf(stderr, "lambda-window-manager-dnd-demo: source action %s\n", actionName(action));
}

wl_data_source_listener const kDataSourceListener{
    .target = sourceTarget,
    .send = sourceSend,
    .cancelled = sourceCancelled,
    .dnd_drop_performed = sourceDropPerformed,
    .dnd_finished = sourceFinished,
    .action = sourceAction,
};

void offerOffer(void*, wl_data_offer*, char const* mimeType) {
  std::fprintf(stderr, "lambda-window-manager-dnd-demo: target saw offered mime %s\n", mimeType ? mimeType : "(null)");
}

void offerSourceActions(void*, wl_data_offer*, std::uint32_t actions) {
  std::fprintf(stderr, "lambda-window-manager-dnd-demo: target saw source actions 0x%x\n", actions);
}
void offerAction(void*, wl_data_offer*, std::uint32_t action) {
  std::fprintf(stderr, "lambda-window-manager-dnd-demo: target action %s\n", actionName(action));
}

wl_data_offer_listener const kDataOfferListener{
    .offer = offerOffer,
    .source_actions = offerSourceActions,
    .action = offerAction,
};

void pointerEnter(void* data, wl_pointer*, std::uint32_t, wl_surface* surface, wl_fixed_t, wl_fixed_t) {
  static_cast<DemoClient*>(data)->pointerSurface = surface;
}

void pointerLeave(void* data, wl_pointer*, std::uint32_t, wl_surface* surface) {
  auto* client = static_cast<DemoClient*>(data);
  if (client->pointerSurface == surface) client->pointerSurface = nullptr;
}
void pointerMotion(void*, wl_pointer*, std::uint32_t, wl_fixed_t, wl_fixed_t) {}

void pointerButton(void* data, wl_pointer*, std::uint32_t serial, std::uint32_t, std::uint32_t button, std::uint32_t state) {
  auto* client = static_cast<DemoClient*>(data);
  if (button != BTN_LEFT || state != WL_POINTER_BUTTON_STATE_PRESSED) return;
  if (client->pointerSurface != client->sourceWindow.surface) return;
  if (!client->dataSource) {
    client->dataSource = wl_data_device_manager_create_data_source(client->dataManager);
    wl_data_source_add_listener(client->dataSource, &kDataSourceListener, client);
    wl_data_source_offer(client->dataSource, kMimeText);
    wl_data_source_set_actions(client->dataSource, WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);
  }
  wl_data_device_start_drag(client->dataDevice,
                            client->dataSource,
                            client->sourceWindow.surface,
                            nullptr,
                            serial);
  client->sourceWindow.blockVisible = false;
  repaintWindow(client->sourceWindow);
  std::fprintf(stderr, "lambda-window-manager-dnd-demo: drag started; release over the blue target window\n");
}

void pointerAxis(void*, wl_pointer*, std::uint32_t, std::uint32_t, wl_fixed_t) {}
void pointerFrame(void*, wl_pointer*) {}
void pointerAxisSource(void*, wl_pointer*, std::uint32_t) {}
void pointerAxisStop(void*, wl_pointer*, std::uint32_t, std::uint32_t) {}
void pointerAxisDiscrete(void*, wl_pointer*, std::uint32_t, std::int32_t) {}
void pointerAxisValue120(void*, wl_pointer*, std::uint32_t, std::int32_t) {}
void pointerAxisRelativeDirection(void*, wl_pointer*, std::uint32_t, std::uint32_t) {}

wl_pointer_listener const kPointerListener{
    .enter = pointerEnter,
    .leave = pointerLeave,
    .motion = pointerMotion,
    .button = pointerButton,
    .axis = pointerAxis,
    .frame = pointerFrame,
    .axis_source = pointerAxisSource,
    .axis_stop = pointerAxisStop,
    .axis_discrete = pointerAxisDiscrete,
    .axis_value120 = pointerAxisValue120,
    .axis_relative_direction = pointerAxisRelativeDirection,
};

void readDroppedText(DemoClient* client) {
  if (client->receiveFd < 0) return;
  pollfd fd{.fd = client->receiveFd, .events = POLLIN | POLLHUP, .revents = 0};
  if (poll(&fd, 1, 0) <= 0) return;
  if ((fd.revents & (POLLIN | POLLHUP)) == 0) return;
  char buffer[256]{};
  ssize_t const bytes = read(client->receiveFd, buffer, sizeof(buffer) - 1u);
  if (bytes > 0) {
    buffer[bytes] = '\0';
    std::fprintf(stderr, "lambda-window-manager-dnd-demo: target received \"%s\"\n", buffer);
    close(client->receiveFd);
    client->receiveFd = -1;
  } else if (bytes == 0 || (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
    close(client->receiveFd);
    client->receiveFd = -1;
  }
}

void dataDeviceDataOffer(void* data, wl_data_device*, wl_data_offer* offer) {
  auto* client = static_cast<DemoClient*>(data);
  client->currentOffer = offer;
  wl_data_offer_add_listener(offer, &kDataOfferListener, client);
}

void dataDeviceEnter(void* data,
                     wl_data_device*,
                     std::uint32_t serial,
                     wl_surface*,
                     wl_fixed_t,
                     wl_fixed_t,
                     wl_data_offer* offer) {
  auto* client = static_cast<DemoClient*>(data);
  client->currentOffer = offer;
  if (offer) {
    wl_data_offer_accept(offer, serial, kMimeText);
    wl_data_offer_set_actions(offer,
                              WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY,
                              WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);
  }
  client->targetWindow.hover = true;
  repaintWindow(client->targetWindow);
  std::fprintf(stderr, "lambda-window-manager-dnd-demo: entered target window\n");
}

void dataDeviceLeave(void* data, wl_data_device*) {
  auto* client = static_cast<DemoClient*>(data);
  client->targetWindow.hover = false;
  repaintWindow(client->targetWindow);
}
void dataDeviceMotion(void*, wl_data_device*, std::uint32_t, wl_fixed_t, wl_fixed_t) {}

void dataDeviceDrop(void* data, wl_data_device*) {
  auto* client = static_cast<DemoClient*>(data);
  if (!client->currentOffer) return;
  int fds[2];
  if (pipe(fds) != 0) throw std::runtime_error(std::string("pipe failed: ") + std::strerror(errno));
  fcntl(fds[0], F_SETFL, fcntl(fds[0], F_GETFL, 0) | O_NONBLOCK);
  client->receiveFd = fds[0];
  wl_data_offer_receive(client->currentOffer, kMimeText, fds[1]);
  close(fds[1]);
  wl_data_offer_finish(client->currentOffer);
  client->targetWindow.hover = false;
  client->targetWindow.blockVisible = true;
  repaintWindow(client->targetWindow);
  wl_display_flush(client->display);
  std::fprintf(stderr, "lambda-window-manager-dnd-demo: drop requested payload\n");
}

void dataDeviceSelection(void*, wl_data_device*, wl_data_offer*) {}

wl_data_device_listener const kDataDeviceListener{
    .data_offer = dataDeviceDataOffer,
    .enter = dataDeviceEnter,
    .leave = dataDeviceLeave,
    .motion = dataDeviceMotion,
    .drop = dataDeviceDrop,
    .selection = dataDeviceSelection,
};

void seatCapabilities(void* data, wl_seat* seat, std::uint32_t capabilities) {
  auto* client = static_cast<DemoClient*>(data);
  if ((capabilities & WL_SEAT_CAPABILITY_POINTER) && !client->pointer) {
    client->pointer = wl_seat_get_pointer(seat);
    wl_pointer_add_listener(client->pointer, &kPointerListener, client);
  }
}

void seatName(void*, wl_seat*, char const*) {}

wl_seat_listener const kSeatListener{seatCapabilities, seatName};

void registryGlobal(void* data, wl_registry* registry, std::uint32_t name, char const* interface,
                    std::uint32_t version) {
  auto* client = static_cast<DemoClient*>(data);
  if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
    client->compositor = static_cast<wl_compositor*>(
        wl_registry_bind(registry, name, &wl_compositor_interface, std::min(version, 4u)));
  } else if (std::strcmp(interface, wl_shm_interface.name) == 0) {
    client->shm = static_cast<wl_shm*>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
  } else if (std::strcmp(interface, wl_seat_interface.name) == 0) {
    client->seat = static_cast<wl_seat*>(wl_registry_bind(registry, name, &wl_seat_interface, std::min(version, 7u)));
    wl_seat_add_listener(client->seat, &kSeatListener, client);
  } else if (std::strcmp(interface, wl_data_device_manager_interface.name) == 0) {
    client->dataManager =
        static_cast<wl_data_device_manager*>(wl_registry_bind(registry, name, &wl_data_device_manager_interface, 3));
  } else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0) {
    client->wmBase = static_cast<xdg_wm_base*>(
        wl_registry_bind(registry, name, &xdg_wm_base_interface, std::min(version, 6u)));
    xdg_wm_base_add_listener(client->wmBase, &kWmBaseListener, client);
  }
}

void registryRemove(void*, wl_registry*, std::uint32_t) {}

wl_registry_listener const kRegistryListener{registryGlobal, registryRemove};

void createWindow(DemoClient& client, DemoWindow& window, char const* title) {
  window.client = &client;
  window.surface = wl_compositor_create_surface(client.compositor);
  window.xdgSurface = xdg_wm_base_get_xdg_surface(client.wmBase, window.surface);
  xdg_surface_add_listener(window.xdgSurface, &kXdgSurfaceListener, &window);
  window.toplevel = xdg_surface_get_toplevel(window.xdgSurface);
  xdg_toplevel_add_listener(window.toplevel, &kToplevelListener, &window);
  xdg_toplevel_set_title(window.toplevel, title);
  xdg_toplevel_set_app_id(window.toplevel, "lambda-window-manager-dnd-demo");
  wl_surface_commit(window.surface);
}

void commitWindow(DemoClient& client, DemoWindow& window) {
  createBuffer(client, window);
  wl_surface_attach(window.surface, window.buffer, 0, 0);
  wl_surface_damage_buffer(window.surface, 0, 0, kWidth, kHeight);
  wl_surface_commit(window.surface);
}

void destroyWindow(DemoWindow& window) {
  if (window.buffer) wl_buffer_destroy(window.buffer);
  if (window.toplevel) xdg_toplevel_destroy(window.toplevel);
  if (window.xdgSurface) xdg_surface_destroy(window.xdgSurface);
  if (window.surface) wl_surface_destroy(window.surface);
  if (window.pixels) munmap(window.pixels, kBufferSize);
  if (window.fd >= 0) close(window.fd);
}

void destroyClient(DemoClient& client) {
  if (client.receiveFd >= 0) close(client.receiveFd);
  if (client.dataSource) wl_data_source_destroy(client.dataSource);
  if (client.dataDevice) wl_data_device_release(client.dataDevice);
  if (client.pointer) wl_pointer_release(client.pointer);
  if (client.seat) wl_seat_release(client.seat);
  destroyWindow(client.targetWindow);
  destroyWindow(client.sourceWindow);
  if (client.dataManager) wl_data_device_manager_destroy(client.dataManager);
  if (client.wmBase) xdg_wm_base_destroy(client.wmBase);
  if (client.shm) wl_shm_destroy(client.shm);
  if (client.compositor) wl_compositor_destroy(client.compositor);
  if (client.registry) wl_registry_destroy(client.registry);
  if (client.display) wl_display_disconnect(client.display);
}

} // namespace

int main() {
  DemoClient client;
  client.sourceWindow.source = true;
  client.sourceWindow.blockVisible = true;
  try {
    client.display = flux::compositor::demo::connectDisplay("lambda-window-manager-dnd-demo");
    if (!client.display) throw std::runtime_error("wl_display_connect failed");

    client.registry = wl_display_get_registry(client.display);
    wl_registry_add_listener(client.registry, &kRegistryListener, &client);
    if (!flux::compositor::demo::roundtripWithTimeout(client.display, 3000)) {
      throw std::runtime_error("registry roundtrip timed out");
    }
    if (!client.compositor || !client.shm || !client.seat || !client.dataManager || !client.wmBase) {
      throw std::runtime_error("compositor is missing wl_compositor, wl_shm, wl_seat, wl_pointer, wl_data_device_manager, or xdg_wm_base");
    }
    if (!flux::compositor::demo::waitUntil(client.display, [&] { return client.pointer != nullptr; }, 3000)) {
      throw std::runtime_error("wl_pointer capability timed out");
    }
    client.dataDevice = wl_data_device_manager_get_data_device(client.dataManager, client.seat);
    wl_data_device_add_listener(client.dataDevice, &kDataDeviceListener, &client);

    createWindow(client, client.targetWindow, "Flux DnD target");
    createWindow(client, client.sourceWindow, "Flux DnD source");
    if (!flux::compositor::demo::waitUntil(client.display,
                                           [&] { return client.sourceWindow.configured && client.targetWindow.configured; },
                                           3000)) {
      throw std::runtime_error("initial configure timed out");
    }
    commitWindow(client, client.targetWindow);
    commitWindow(client, client.sourceWindow);
    wl_display_flush(client.display);
    std::fprintf(stderr, "lambda-window-manager-dnd-demo: orange source and blue target committed\n");

    while (gRunning.load(std::memory_order_relaxed)) {
      if (flux::compositor::demo::dispatchWithTimeout(client.display, 250) < 0) break;
      readDroppedText(&client);
    }

    destroyClient(client);
    return 0;
  } catch (std::exception const& e) {
    std::fprintf(stderr, "lambda-window-manager-dnd-demo: %s\n", e.what());
    destroyClient(client);
    return 1;
  }
}
