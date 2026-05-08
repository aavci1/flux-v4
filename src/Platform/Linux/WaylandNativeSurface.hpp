#pragma once

struct wl_display;
struct wl_surface;

namespace flux {

struct WaylandNativeSurface {
  wl_display* display = nullptr;
  wl_surface* surface = nullptr;
};

} // namespace flux
