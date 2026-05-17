#include "DemoClientSupport.hpp"
#include "xdg-shell-client-protocol.h"

#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <sys/mman.h>
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

constexpr int kWidth = 420;
constexpr int kHeight = 180;
constexpr int kStride = kWidth * 4;
constexpr int kBufferSize = kStride * kHeight;
constexpr char const* kMimeText = "text/plain;charset=utf-8";
constexpr char const* kClipboardText = "Flux clipboard smoke text";

std::atomic<bool> gRunning{true};

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
  wl_surface* surface = nullptr;
  wl_buffer* buffer = nullptr;
  xdg_wm_base* wmBase = nullptr;
  xdg_surface* xdgSurface = nullptr;
  xdg_toplevel* toplevel = nullptr;
  void* pixels = nullptr;
  int fd = -1;
  int receiveFd = -1;
  bool configured = false;
  bool selectionSet = false;
};

int createSharedMemoryFile(std::size_t size) {
  int fd = memfd_create("flux-compositor-clipboard-demo", MFD_CLOEXEC | MFD_ALLOW_SEALING);
  if (fd < 0) throw std::runtime_error(std::string("memfd_create failed: ") + std::strerror(errno));
  if (ftruncate(fd, static_cast<off_t>(size)) != 0) {
    close(fd);
    throw std::runtime_error(std::string("ftruncate failed: ") + std::strerror(errno));
  }
  return fd;
}

void fillPattern(void* pixels) {
  auto* dst = static_cast<std::uint8_t*>(pixels);
  for (int y = 0; y < kHeight; ++y) {
    for (int x = 0; x < kWidth; ++x) {
      bool const stripe = ((x / 24) + (y / 18)) % 2 == 0;
      std::uint8_t const red = stripe ? 190 : 55;
      std::uint8_t const green = stripe ? 75 : 180;
      std::uint8_t const blue = stripe ? 230 : 115;
      std::size_t const offset = static_cast<std::size_t>(y) * kStride + static_cast<std::size_t>(x) * 4u;
      dst[offset + 0u] = blue;
      dst[offset + 1u] = green;
      dst[offset + 2u] = red;
      dst[offset + 3u] = 0xff;
    }
  }
}

void wmBasePing(void*, xdg_wm_base* wmBase, std::uint32_t serial) { xdg_wm_base_pong(wmBase, serial); }
xdg_wm_base_listener const kWmBaseListener{wmBasePing};

void xdgSurfaceConfigure(void* data, xdg_surface* surface, std::uint32_t serial) {
  auto* client = static_cast<DemoClient*>(data);
  xdg_surface_ack_configure(surface, serial);
  client->configured = true;
}
xdg_surface_listener const kXdgSurfaceListener{xdgSurfaceConfigure};

void toplevelConfigure(void*, xdg_toplevel*, std::int32_t, std::int32_t, wl_array*) {}
void toplevelClose(void*, xdg_toplevel*) { gRunning.store(false, std::memory_order_relaxed); }
void toplevelConfigureBounds(void*, xdg_toplevel*, std::int32_t, std::int32_t) {}
void toplevelWmCapabilities(void*, xdg_toplevel*, wl_array*) {}
xdg_toplevel_listener const kToplevelListener{toplevelConfigure, toplevelClose, toplevelConfigureBounds, toplevelWmCapabilities};

void dataSourceTarget(void*, wl_data_source*, char const*) {}
void dataSourceSend(void*, wl_data_source*, char const* mimeType, int fd) {
  if (std::strcmp(mimeType, kMimeText) == 0) {
    char const* text = kClipboardText;
    std::size_t remaining = std::strlen(text);
    while (remaining > 0) {
      ssize_t written = write(fd, text, remaining);
      if (written <= 0) break;
      text += written;
      remaining -= static_cast<std::size_t>(written);
    }
  }
  close(fd);
}
void dataSourceCancelled(void*, wl_data_source*) { std::fprintf(stderr, "flux-compositor-clipboard-demo: source cancelled\n"); }
void dataSourceDndDropPerformed(void*, wl_data_source*) {}
void dataSourceDndFinished(void*, wl_data_source*) {}
void dataSourceAction(void*, wl_data_source*, std::uint32_t) {}
wl_data_source_listener const kDataSourceListener{
    dataSourceTarget, dataSourceSend, dataSourceCancelled, dataSourceDndDropPerformed, dataSourceDndFinished, dataSourceAction};

void dataOfferOffer(void*, wl_data_offer*, char const* mimeType) {
  std::fprintf(stderr, "flux-compositor-clipboard-demo: offered %s\n", mimeType);
}
void dataOfferSourceActions(void*, wl_data_offer*, std::uint32_t) {}
void dataOfferAction(void*, wl_data_offer*, std::uint32_t) {}
wl_data_offer_listener const kDataOfferListener{dataOfferOffer, dataOfferSourceActions, dataOfferAction};

void requestOfferText(DemoClient* client, wl_data_offer* offer) {
  int fds[2]{-1, -1};
  if (pipe(fds) != 0) throw std::runtime_error(std::string("pipe failed: ") + std::strerror(errno));
  wl_data_offer_receive(offer, kMimeText, fds[1]);
  close(fds[1]);
  fcntl(fds[0], F_SETFL, fcntl(fds[0], F_GETFL, 0) | O_NONBLOCK);
  if (client->receiveFd >= 0) close(client->receiveFd);
  client->receiveFd = fds[0];
}

void dataDeviceDataOffer(void* data, wl_data_device*, wl_data_offer* offer) {
  auto* client = static_cast<DemoClient*>(data);
  client->currentOffer = offer;
  wl_data_offer_add_listener(offer, &kDataOfferListener, client);
}
void dataDeviceEnter(void*, wl_data_device*, std::uint32_t, wl_surface*, wl_fixed_t, wl_fixed_t, wl_data_offer*) {}
void dataDeviceLeave(void*, wl_data_device*) {}
void dataDeviceMotion(void*, wl_data_device*, std::uint32_t, wl_fixed_t, wl_fixed_t) {}
void dataDeviceDrop(void*, wl_data_device*) {}
void dataDeviceSelection(void* data, wl_data_device*, wl_data_offer* offer) {
  if (offer) requestOfferText(static_cast<DemoClient*>(data), offer);
}
wl_data_device_listener const kDataDeviceListener{
    dataDeviceDataOffer, dataDeviceEnter, dataDeviceLeave, dataDeviceMotion, dataDeviceDrop, dataDeviceSelection};

void setClipboardSelection(DemoClient* client, std::uint32_t serial) {
  if (client->selectionSet) return;
  wl_data_device_set_selection(client->dataDevice, client->dataSource, serial);
  wl_display_flush(client->display);
  client->selectionSet = true;
  std::fprintf(stderr, "flux-compositor-clipboard-demo: set clipboard selection\n");
}

void pointerEnter(void*, wl_pointer*, std::uint32_t, wl_surface*, wl_fixed_t, wl_fixed_t) {}
void pointerLeave(void*, wl_pointer*, std::uint32_t, wl_surface*) {}
void pointerMotion(void*, wl_pointer*, std::uint32_t, wl_fixed_t, wl_fixed_t) {}
void pointerButton(void* data, wl_pointer*, std::uint32_t serial, std::uint32_t, std::uint32_t button, std::uint32_t state) {
  if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_PRESSED) setClipboardSelection(static_cast<DemoClient*>(data), serial);
}
void pointerAxis(void*, wl_pointer*, std::uint32_t, std::uint32_t, wl_fixed_t) {}
void pointerFrame(void*, wl_pointer*) {}
void pointerAxisSource(void*, wl_pointer*, std::uint32_t) {}
void pointerAxisStop(void*, wl_pointer*, std::uint32_t, std::uint32_t) {}
void pointerAxisDiscrete(void*, wl_pointer*, std::uint32_t, std::int32_t) {}
void pointerAxisValue120(void*, wl_pointer*, std::uint32_t, std::int32_t) {}
void pointerAxisRelativeDirection(void*, wl_pointer*, std::uint32_t, std::uint32_t) {}
wl_pointer_listener const kPointerListener{pointerEnter, pointerLeave, pointerMotion, pointerButton, pointerAxis, pointerFrame,
                                           pointerAxisSource, pointerAxisStop, pointerAxisDiscrete, pointerAxisValue120,
                                           pointerAxisRelativeDirection};

void registryGlobal(void* data, wl_registry* registry, std::uint32_t name, char const* interface, std::uint32_t version) {
  auto* client = static_cast<DemoClient*>(data);
  if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
    client->compositor = static_cast<wl_compositor*>(wl_registry_bind(registry, name, &wl_compositor_interface, std::min(version, 4u)));
  } else if (std::strcmp(interface, wl_shm_interface.name) == 0) {
    client->shm = static_cast<wl_shm*>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
  } else if (std::strcmp(interface, wl_seat_interface.name) == 0) {
    client->seat = static_cast<wl_seat*>(wl_registry_bind(registry, name, &wl_seat_interface, std::min(version, 7u)));
  } else if (std::strcmp(interface, wl_data_device_manager_interface.name) == 0) {
    client->dataManager = static_cast<wl_data_device_manager*>(wl_registry_bind(registry, name, &wl_data_device_manager_interface, std::min(version, 3u)));
  } else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0) {
    client->wmBase = static_cast<xdg_wm_base*>(wl_registry_bind(registry, name, &xdg_wm_base_interface, std::min(version, 6u)));
    xdg_wm_base_add_listener(client->wmBase, &kWmBaseListener, client);
  }
}
void registryRemove(void*, wl_registry*, std::uint32_t) {}
wl_registry_listener const kRegistryListener{registryGlobal, registryRemove};

void createBuffer(DemoClient& client) {
  client.fd = createSharedMemoryFile(kBufferSize);
  client.pixels = mmap(nullptr, kBufferSize, PROT_READ | PROT_WRITE, MAP_SHARED, client.fd, 0);
  if (client.pixels == MAP_FAILED) throw std::runtime_error(std::string("mmap failed: ") + std::strerror(errno));
  fillPattern(client.pixels);
  wl_shm_pool* pool = wl_shm_create_pool(client.shm, client.fd, kBufferSize);
  client.buffer = wl_shm_pool_create_buffer(pool, 0, kWidth, kHeight, kStride, WL_SHM_FORMAT_XRGB8888);
  wl_shm_pool_destroy(pool);
}

void destroyClient(DemoClient& client) {
  if (client.dataSource) wl_data_source_destroy(client.dataSource);
  if (client.dataDevice) wl_data_device_release(client.dataDevice);
  if (client.dataManager) wl_data_device_manager_destroy(client.dataManager);
  if (client.pointer) wl_pointer_destroy(client.pointer);
  if (client.buffer) wl_buffer_destroy(client.buffer);
  if (client.toplevel) xdg_toplevel_destroy(client.toplevel);
  if (client.xdgSurface) xdg_surface_destroy(client.xdgSurface);
  if (client.surface) wl_surface_destroy(client.surface);
  if (client.wmBase) xdg_wm_base_destroy(client.wmBase);
  if (client.seat) wl_seat_destroy(client.seat);
  if (client.shm) wl_shm_destroy(client.shm);
  if (client.compositor) wl_compositor_destroy(client.compositor);
  if (client.registry) wl_registry_destroy(client.registry);
  if (client.pixels) munmap(client.pixels, kBufferSize);
  if (client.receiveFd >= 0) close(client.receiveFd);
  if (client.fd >= 0) close(client.fd);
  if (client.display) wl_display_disconnect(client.display);
}

} // namespace

int main() {
  DemoClient client;
  try {
    client.display = flux::compositor::demo::connectDisplay("flux-compositor-clipboard-demo");
    if (!client.display) throw std::runtime_error("wl_display_connect failed");
    client.registry = wl_display_get_registry(client.display);
    wl_registry_add_listener(client.registry, &kRegistryListener, &client);
    if (!flux::compositor::demo::roundtripWithTimeout(client.display, 3000)) throw std::runtime_error("registry roundtrip timed out");
    if (!client.compositor || !client.shm || !client.seat || !client.wmBase || !client.dataManager) {
      throw std::runtime_error("compositor is missing required clipboard globals");
    }
    client.pointer = wl_seat_get_pointer(client.seat);
    wl_pointer_add_listener(client.pointer, &kPointerListener, &client);
    client.dataDevice = wl_data_device_manager_get_data_device(client.dataManager, client.seat);
    wl_data_device_add_listener(client.dataDevice, &kDataDeviceListener, &client);
    client.dataSource = wl_data_device_manager_create_data_source(client.dataManager);
    wl_data_source_add_listener(client.dataSource, &kDataSourceListener, &client);
    wl_data_source_offer(client.dataSource, kMimeText);
    client.surface = wl_compositor_create_surface(client.compositor);
    client.xdgSurface = xdg_wm_base_get_xdg_surface(client.wmBase, client.surface);
    xdg_surface_add_listener(client.xdgSurface, &kXdgSurfaceListener, &client);
    client.toplevel = xdg_surface_get_toplevel(client.xdgSurface);
    xdg_toplevel_add_listener(client.toplevel, &kToplevelListener, &client);
    xdg_toplevel_set_title(client.toplevel, "Flux Clipboard demo");
    xdg_toplevel_set_app_id(client.toplevel, "flux-compositor-clipboard-demo");
    wl_surface_commit(client.surface);
    if (!flux::compositor::demo::waitUntil(client.display, [&] { return client.configured; }, 3000)) {
      throw std::runtime_error("initial configure timed out");
    }
    createBuffer(client);
    wl_surface_attach(client.surface, client.buffer, 0, 0);
    wl_surface_damage_buffer(client.surface, 0, 0, kWidth, kHeight);
    wl_surface_commit(client.surface);
    wl_display_flush(client.display);
    std::fprintf(stderr, "flux-compositor-clipboard-demo: click the window to set and receive clipboard selection\n");
    while (gRunning.load(std::memory_order_relaxed)) {
      if (flux::compositor::demo::dispatchWithTimeout(client.display, 250) < 0) break;
      if (client.receiveFd >= 0) {
        char buffer[256]{};
        ssize_t n = read(client.receiveFd, buffer, sizeof(buffer) - 1u);
        if (n > 0) {
          buffer[n] = '\0';
          std::fprintf(stderr, "flux-compositor-clipboard-demo: received \"%s\"\n", buffer);
          gRunning.store(false, std::memory_order_relaxed);
        }
      }
    }
    destroyClient(client);
    return 0;
  } catch (std::exception const& e) {
    std::fprintf(stderr, "flux-compositor-clipboard-demo: %s\n", e.what());
    destroyClient(client);
    return 1;
  }
}
