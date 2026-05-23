#include "DemoClientSupport.hpp"
#include "xdg-shell-client-protocol.h"

#include <sys/mman.h>
#include <unistd.h>
#include <linux/input-event-codes.h>
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

constexpr int kParentWidth = 480;
constexpr int kParentHeight = 280;
constexpr int kMenuWidth = 200;
constexpr int kMenuHeight = 120;
constexpr int kSubmenuWidth = 160;
constexpr int kSubmenuHeight = 96;

std::atomic<bool> gRunning{true};

struct Buffer {
  wl_buffer* buffer = nullptr;
  void* pixels = nullptr;
  int fd = -1;
  int width = 0;
  int height = 0;
  int stride = 0;
  int size = 0;
};

struct DemoClient {
  wl_display* display = nullptr;
  wl_registry* registry = nullptr;
  wl_compositor* compositor = nullptr;
  wl_shm* shm = nullptr;
  wl_seat* seat = nullptr;
  wl_pointer* pointer = nullptr;
  xdg_wm_base* wmBase = nullptr;
  wl_surface* surface = nullptr;
  xdg_surface* xdgSurface = nullptr;
  xdg_toplevel* toplevel = nullptr;
  Buffer parentBuffer;
  bool configured = false;
  wl_surface* menuSurface = nullptr;
  xdg_surface* menuXdgSurface = nullptr;
  xdg_popup* menuPopup = nullptr;
  Buffer menuBuffer;
  bool menuCommitted = false;
  bool menuGrabbed = false;
  int menuHoverRow = -1;
  wl_surface* submenuSurface = nullptr;
  xdg_surface* submenuXdgSurface = nullptr;
  xdg_popup* submenuPopup = nullptr;
  Buffer submenuBuffer;
  bool submenuCommitted = false;
  bool submenuGrabbed = false;
  int submenuHoverRow = -1;
  std::uint32_t lastPointerSerial = 0;
};

int createSharedMemoryFile(char const* name, std::size_t size) {
  int fd = memfd_create(name, MFD_CLOEXEC | MFD_ALLOW_SEALING);
  if (fd < 0) throw std::runtime_error(std::string("memfd_create failed: ") + std::strerror(errno));
  if (ftruncate(fd, static_cast<off_t>(size)) != 0) {
    close(fd);
    throw std::runtime_error(std::string("ftruncate failed: ") + std::strerror(errno));
  }
  return fd;
}

void fillSolid(Buffer& buffer, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
  auto* dst = static_cast<std::uint8_t*>(buffer.pixels);
  for (int y = 0; y < buffer.height; ++y) {
    for (int x = 0; x < buffer.width; ++x) {
      std::size_t const offset = static_cast<std::size_t>(y) * buffer.stride + static_cast<std::size_t>(x) * 4u;
      dst[offset + 0u] = r;
      dst[offset + 1u] = g;
      dst[offset + 2u] = b;
      dst[offset + 3u] = 0xff;
    }
  }
}

void fillMenuBuffer(Buffer& buffer, int hoverRow, bool submenuOpen) {
  auto* dst = static_cast<std::uint8_t*>(buffer.pixels);
  int const rowHeight = std::max(1, buffer.height / 3);
  char const* labels[3] = {"Open", "More >", "Quit"};
  for (int y = 0; y < buffer.height; ++y) {
    int const row = std::min(2, y / rowHeight);
    bool const hover = row == hoverRow;
    bool const active = row == 1 && submenuOpen;
    for (int x = 0; x < buffer.width; ++x) {
      std::size_t const offset = static_cast<std::size_t>(y) * buffer.stride + static_cast<std::size_t>(x) * 4u;
      if (y == rowHeight || y == rowHeight * 2) {
        dst[offset + 0u] = 40;
        dst[offset + 1u] = 40;
        dst[offset + 2u] = 40;
      } else if (hover || active) {
        dst[offset + 0u] = 70;
        dst[offset + 1u] = 130;
        dst[offset + 2u] = 220;
      } else {
        dst[offset + 0u] = 230;
        dst[offset + 1u] = 230;
        dst[offset + 2u] = 235;
      }
      dst[offset + 3u] = 0xff;
      (void)labels;
    }
  }
}

void fillSubmenuBuffer(Buffer& buffer, int hoverRow) {
  auto* dst = static_cast<std::uint8_t*>(buffer.pixels);
  int const rowHeight = std::max(1, buffer.height / 3);
  for (int y = 0; y < buffer.height; ++y) {
    int const row = std::min(2, y / rowHeight);
    bool const hover = row == hoverRow;
    for (int x = 0; x < buffer.width; ++x) {
      std::size_t const offset = static_cast<std::size_t>(y) * buffer.stride + static_cast<std::size_t>(x) * 4u;
      if (y == rowHeight || y == rowHeight * 2) {
        dst[offset + 0u] = 30;
        dst[offset + 1u] = 30;
        dst[offset + 2u] = 30;
      } else if (hover) {
        dst[offset + 0u] = 90;
        dst[offset + 1u] = 190;
        dst[offset + 2u] = 120;
      } else {
        dst[offset + 0u] = 210;
        dst[offset + 1u] = 245;
        dst[offset + 2u] = 220;
      }
      dst[offset + 3u] = 0xff;
    }
  }
}

Buffer createBuffer(wl_shm* shm, char const* name, int width, int height) {
  Buffer buffer;
  buffer.width = width;
  buffer.height = height;
  buffer.stride = width * 4;
  buffer.size = buffer.stride * height;
  buffer.fd = createSharedMemoryFile(name, static_cast<std::size_t>(buffer.size));
  buffer.pixels = mmap(nullptr, buffer.size, PROT_READ | PROT_WRITE, MAP_SHARED, buffer.fd, 0);
  if (buffer.pixels == MAP_FAILED) {
    buffer.pixels = nullptr;
    throw std::runtime_error(std::string("mmap failed: ") + std::strerror(errno));
  }
  wl_shm_pool* pool = wl_shm_create_pool(shm, buffer.fd, buffer.size);
  buffer.buffer = wl_shm_pool_create_buffer(pool, 0, width, height, buffer.stride, WL_SHM_FORMAT_XRGB8888);
  wl_shm_pool_destroy(pool);
  return buffer;
}

void destroyBuffer(Buffer& buffer) {
  if (buffer.buffer) wl_buffer_destroy(buffer.buffer);
  if (buffer.pixels) munmap(buffer.pixels, buffer.size);
  if (buffer.fd >= 0) close(buffer.fd);
  buffer = {};
  buffer.fd = -1;
}

void commitBuffer(wl_surface* surface, Buffer const& buffer) {
  wl_surface_attach(surface, buffer.buffer, 0, 0);
  wl_surface_damage_buffer(surface, 0, 0, buffer.width, buffer.height);
  wl_surface_commit(surface);
}

void repaintMenu(DemoClient& client) {
  if (!client.menuSurface || !client.menuBuffer.buffer) return;
  fillMenuBuffer(client.menuBuffer, client.menuHoverRow, client.submenuPopup != nullptr);
  commitBuffer(client.menuSurface, client.menuBuffer);
  wl_display_flush(client.display);
}

void repaintSubmenu(DemoClient& client) {
  if (!client.submenuSurface || !client.submenuBuffer.buffer) return;
  fillSubmenuBuffer(client.submenuBuffer, client.submenuHoverRow);
  commitBuffer(client.submenuSurface, client.submenuBuffer);
  wl_display_flush(client.display);
}

void wmBasePing(void*, xdg_wm_base* wmBase, std::uint32_t serial) {
  xdg_wm_base_pong(wmBase, serial);
}

xdg_wm_base_listener const kWmBaseListener{wmBasePing};

void parentConfigure(void* data, xdg_surface* surface, std::uint32_t serial) {
  auto* client = static_cast<DemoClient*>(data);
  xdg_surface_ack_configure(surface, serial);
  client->configured = true;
}

void menuSurfaceConfigure(void* data, xdg_surface* surface, std::uint32_t serial) {
  auto* client = static_cast<DemoClient*>(data);
  xdg_surface_ack_configure(surface, serial);
  if (!client->menuCommitted) {
    fillMenuBuffer(client->menuBuffer, -1, false);
    commitBuffer(client->menuSurface, client->menuBuffer);
    client->menuCommitted = true;
    if (!client->menuGrabbed && client->seat && client->lastPointerSerial != 0) {
      xdg_popup_grab(client->menuPopup, client->seat, client->lastPointerSerial);
      client->menuGrabbed = true;
      std::fprintf(stderr, "lambda-window-manager-popup-grab-demo: menu popup grab serial=%u\n",
                   client->lastPointerSerial);
    }
  }
}

void submenuSurfaceConfigure(void* data, xdg_surface* surface, std::uint32_t serial) {
  auto* client = static_cast<DemoClient*>(data);
  xdg_surface_ack_configure(surface, serial);
  if (!client->submenuCommitted) {
    fillSubmenuBuffer(client->submenuBuffer, -1);
    commitBuffer(client->submenuSurface, client->submenuBuffer);
    client->submenuCommitted = true;
    if (!client->submenuGrabbed && client->seat && client->lastPointerSerial != 0) {
      xdg_popup_grab(client->submenuPopup, client->seat, client->lastPointerSerial);
      client->submenuGrabbed = true;
      std::fprintf(stderr, "lambda-window-manager-popup-grab-demo: submenu popup grab serial=%u\n",
                   client->lastPointerSerial);
    }
  }
}

xdg_surface_listener const kParentSurfaceListener{parentConfigure};
xdg_surface_listener const kMenuSurfaceListener{menuSurfaceConfigure};
xdg_surface_listener const kSubmenuSurfaceListener{submenuSurfaceConfigure};

void toplevelConfigure(void*, xdg_toplevel*, std::int32_t, std::int32_t, wl_array*) {}
void toplevelClose(void*, xdg_toplevel*) { gRunning.store(false, std::memory_order_relaxed); }
void toplevelConfigureBounds(void*, xdg_toplevel*, std::int32_t, std::int32_t) {}
void toplevelWmCapabilities(void*, xdg_toplevel*, wl_array*) {}

xdg_toplevel_listener const kToplevelListener{
    .configure = toplevelConfigure,
    .close = toplevelClose,
    .configure_bounds = toplevelConfigureBounds,
    .wm_capabilities = toplevelWmCapabilities,
};

void popupConfigure(void*, xdg_popup*, std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height) {
  std::fprintf(stderr,
               "lambda-window-manager-popup-grab-demo: popup configured at %d,%d size %dx%d\n",
               x,
               y,
               width,
               height);
}

void menuPopupDone(void* data, xdg_popup*) {
  auto* client = static_cast<DemoClient*>(data);
  std::fprintf(stderr, "lambda-window-manager-popup-grab-demo: menu popup dismissed\n");
  if (client->submenuPopup) xdg_popup_destroy(client->submenuPopup);
  client->submenuPopup = nullptr;
  client->submenuXdgSurface = nullptr;
  if (client->submenuSurface) wl_surface_destroy(client->submenuSurface);
  client->submenuSurface = nullptr;
  destroyBuffer(client->submenuBuffer);
  client->submenuCommitted = false;
  client->submenuGrabbed = false;
  client->menuPopup = nullptr;
  client->menuXdgSurface = nullptr;
  if (client->menuSurface) wl_surface_destroy(client->menuSurface);
  client->menuSurface = nullptr;
  destroyBuffer(client->menuBuffer);
  client->menuCommitted = false;
  client->menuGrabbed = false;
  client->menuHoverRow = -1;
}

void submenuPopupDone(void* data, xdg_popup*) {
  auto* client = static_cast<DemoClient*>(data);
  std::fprintf(stderr, "lambda-window-manager-popup-grab-demo: submenu popup dismissed\n");
  client->submenuPopup = nullptr;
  client->submenuXdgSurface = nullptr;
  if (client->submenuSurface) wl_surface_destroy(client->submenuSurface);
  client->submenuSurface = nullptr;
  destroyBuffer(client->submenuBuffer);
  client->submenuCommitted = false;
  client->submenuGrabbed = false;
  client->submenuHoverRow = -1;
  repaintMenu(*client);
}

void popupRepositioned(void*, xdg_popup*, std::uint32_t token) {
  std::fprintf(stderr, "lambda-window-manager-popup-grab-demo: popup repositioned token %u\n", token);
}

xdg_popup_listener const kMenuPopupListener{
    .configure = popupConfigure,
    .popup_done = menuPopupDone,
    .repositioned = popupRepositioned,
};

xdg_popup_listener const kSubmenuPopupListener{
    .configure = popupConfigure,
    .popup_done = submenuPopupDone,
    .repositioned = popupRepositioned,
};

int rowForSurfaceY(int height, wl_fixed_t surfaceY) {
  int const rowHeight = std::max(1, height / 3);
  return std::clamp(static_cast<int>(wl_fixed_to_double(surfaceY)) / rowHeight, 0, 2);
}

void createSubmenu(DemoClient& client) {
  if (client.submenuPopup || !client.menuPopup || !client.menuXdgSurface) return;
  client.submenuSurface = wl_compositor_create_surface(client.compositor);
  client.submenuXdgSurface = xdg_wm_base_get_xdg_surface(client.wmBase, client.submenuSurface);
  xdg_surface_add_listener(client.submenuXdgSurface, &kSubmenuSurfaceListener, &client);

  xdg_positioner* positioner = xdg_wm_base_create_positioner(client.wmBase);
  xdg_positioner_set_size(positioner, kSubmenuWidth, kSubmenuHeight);
  xdg_positioner_set_anchor_rect(positioner, kMenuWidth - 4, kMenuHeight / 3, 4, kMenuHeight / 3);
  xdg_positioner_set_anchor(positioner, XDG_POSITIONER_ANCHOR_RIGHT);
  xdg_positioner_set_gravity(positioner, XDG_POSITIONER_GRAVITY_RIGHT);
  xdg_positioner_set_offset(positioner, 4, 0);
  client.submenuPopup =
      xdg_surface_get_popup(client.submenuXdgSurface, client.menuXdgSurface, positioner);
  xdg_popup_add_listener(client.submenuPopup, &kSubmenuPopupListener, &client);
  xdg_positioner_destroy(positioner);

  client.submenuBuffer =
      createBuffer(client.shm, "lambda-window-manager-popup-grab-demo-submenu", kSubmenuWidth, kSubmenuHeight);
  wl_surface_commit(client.submenuSurface);
  wl_display_flush(client.display);
  std::fprintf(stderr, "lambda-window-manager-popup-grab-demo: requested submenu popup\n");
}

void createMenu(DemoClient& client) {
  if (client.menuPopup) return;
  client.menuSurface = wl_compositor_create_surface(client.compositor);
  client.menuXdgSurface = xdg_wm_base_get_xdg_surface(client.wmBase, client.menuSurface);
  xdg_surface_add_listener(client.menuXdgSurface, &kMenuSurfaceListener, &client);

  xdg_positioner* positioner = xdg_wm_base_create_positioner(client.wmBase);
  xdg_positioner_set_size(positioner, kMenuWidth, kMenuHeight);
  xdg_positioner_set_anchor_rect(positioner, 120, 90, 1, 1);
  xdg_positioner_set_anchor(positioner, XDG_POSITIONER_ANCHOR_BOTTOM_LEFT);
  xdg_positioner_set_gravity(positioner, XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT);
  xdg_positioner_set_offset(positioner, 8, 8);
  client.menuPopup = xdg_surface_get_popup(client.menuXdgSurface, client.xdgSurface, positioner);
  xdg_popup_add_listener(client.menuPopup, &kMenuPopupListener, &client);
  xdg_positioner_destroy(positioner);

  client.menuBuffer =
      createBuffer(client.shm, "lambda-window-manager-popup-grab-demo-menu", kMenuWidth, kMenuHeight);
  wl_surface_commit(client.menuSurface);
  wl_display_flush(client.display);
  std::fprintf(stderr, "lambda-window-manager-popup-grab-demo: requested menu popup\n");
}

void pointerEnter(void* data, wl_pointer*, std::uint32_t serial, wl_surface* surface, wl_fixed_t,
                  wl_fixed_t surfaceY) {
  auto* client = static_cast<DemoClient*>(data);
  client->lastPointerSerial = serial;
  if (surface == client->menuSurface) {
    client->menuHoverRow = rowForSurfaceY(kMenuHeight, surfaceY);
    if (client->menuHoverRow == 1) createSubmenu(*client);
    repaintMenu(*client);
    return;
  }
  if (surface == client->submenuSurface) {
    client->submenuHoverRow = rowForSurfaceY(kSubmenuHeight, surfaceY);
    repaintSubmenu(*client);
  }
}

void pointerLeave(void* data, wl_pointer*, std::uint32_t, wl_surface* surface) {
  auto* client = static_cast<DemoClient*>(data);
  if (surface == client->menuSurface) {
    client->menuHoverRow = -1;
    repaintMenu(*client);
  } else if (surface == client->submenuSurface) {
    client->submenuHoverRow = -1;
    repaintSubmenu(*client);
  }
}

void pointerMotion(void* data, wl_pointer*, std::uint32_t, wl_fixed_t, wl_fixed_t surfaceY) {
  auto* client = static_cast<DemoClient*>(data);
  if (client->menuHoverRow >= 0) {
    int const row = rowForSurfaceY(kMenuHeight, surfaceY);
    if (row != client->menuHoverRow) {
      client->menuHoverRow = row;
      if (row == 1) createSubmenu(*client);
      repaintMenu(*client);
    }
    return;
  }
  if (client->submenuHoverRow >= 0) {
    int const row = rowForSurfaceY(kSubmenuHeight, surfaceY);
    if (row != client->submenuHoverRow) {
      client->submenuHoverRow = row;
      repaintSubmenu(*client);
    }
  }
}

void pointerButton(void* data, wl_pointer*, std::uint32_t serial, std::uint32_t, std::uint32_t button,
                   std::uint32_t state) {
  auto* client = static_cast<DemoClient*>(data);
  if (state == WL_POINTER_BUTTON_STATE_PRESSED) client->lastPointerSerial = serial;
  if (state != WL_POINTER_BUTTON_STATE_PRESSED || button != BTN_LEFT) return;
  if (!client->menuPopup) {
    createMenu(*client);
    return;
  }
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
  } else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0) {
    client->wmBase = static_cast<xdg_wm_base*>(
        wl_registry_bind(registry, name, &xdg_wm_base_interface, std::min(version, 6u)));
    xdg_wm_base_add_listener(client->wmBase, &kWmBaseListener, client);
  }
}

void registryGlobalRemove(void*, wl_registry*, std::uint32_t) {}

wl_registry_listener const kRegistryListener{registryGlobal, registryGlobalRemove};

void destroyClient(DemoClient& client) {
  destroyBuffer(client.submenuBuffer);
  destroyBuffer(client.menuBuffer);
  destroyBuffer(client.parentBuffer);
  if (client.submenuPopup) xdg_popup_destroy(client.submenuPopup);
  if (client.submenuXdgSurface) xdg_surface_destroy(client.submenuXdgSurface);
  if (client.submenuSurface) wl_surface_destroy(client.submenuSurface);
  if (client.menuPopup) xdg_popup_destroy(client.menuPopup);
  if (client.menuXdgSurface) xdg_surface_destroy(client.menuXdgSurface);
  if (client.menuSurface) wl_surface_destroy(client.menuSurface);
  if (client.toplevel) xdg_toplevel_destroy(client.toplevel);
  if (client.xdgSurface) xdg_surface_destroy(client.xdgSurface);
  if (client.surface) wl_surface_destroy(client.surface);
  if (client.pointer) wl_pointer_destroy(client.pointer);
  if (client.seat) wl_seat_destroy(client.seat);
  if (client.wmBase) xdg_wm_base_destroy(client.wmBase);
  if (client.shm) wl_shm_destroy(client.shm);
  if (client.compositor) wl_compositor_destroy(client.compositor);
  if (client.registry) wl_registry_destroy(client.registry);
  if (client.display) wl_display_disconnect(client.display);
}

} // namespace

int main() {
  DemoClient client;
  try {
    client.display = flux::compositor::demo::connectDisplay("lambda-window-manager-popup-grab-demo");
    client.registry = wl_display_get_registry(client.display);
    wl_registry_add_listener(client.registry, &kRegistryListener, &client);
    if (wl_display_roundtrip(client.display) < 0) throw std::runtime_error("initial roundtrip failed");
    if (!client.compositor || !client.shm || !client.seat || !client.wmBase) {
      throw std::runtime_error("compositor is missing wl_compositor, wl_shm, wl_seat, or xdg_wm_base");
    }

    client.surface = wl_compositor_create_surface(client.compositor);
    client.xdgSurface = xdg_wm_base_get_xdg_surface(client.wmBase, client.surface);
    xdg_surface_add_listener(client.xdgSurface, &kParentSurfaceListener, &client);
    client.toplevel = xdg_surface_get_toplevel(client.xdgSurface);
    xdg_toplevel_add_listener(client.toplevel, &kToplevelListener, &client);
    xdg_toplevel_set_title(client.toplevel, "Flux popup grab demo");
    xdg_toplevel_set_app_id(client.toplevel, "lambda-window-manager-popup-grab-demo");

    client.parentBuffer =
        createBuffer(client.shm, "lambda-window-manager-popup-grab-demo-parent", kParentWidth, kParentHeight);
    fillSolid(client.parentBuffer, 70, 110, 180);
    commitBuffer(client.surface, client.parentBuffer);
    wl_display_flush(client.display);

    std::fprintf(stderr,
                 "lambda-window-manager-popup-grab-demo: click the parent window to open a menu; hover "
                 "'More >' for a grabbed submenu; click outside or press Escape to dismiss\n");
    std::fprintf(stderr,
                 "lambda-window-manager-popup-grab-demo: enable compositor grabs with [input] popup_grabs = "
                 "true in the compositor config\n");

    while (gRunning.load(std::memory_order_relaxed)) {
      if (flux::compositor::demo::dispatchWithTimeout(client.display, 16) < 0) {
        throw std::runtime_error("Wayland dispatch failed");
      }
    }
    destroyClient(client);
    return 0;
  } catch (std::exception const& e) {
    destroyClient(client);
    std::fprintf(stderr, "lambda-window-manager-popup-grab-demo: %s\n", e.what());
    return 1;
  }
}
