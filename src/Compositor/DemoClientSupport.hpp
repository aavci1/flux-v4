#pragma once

#include <poll.h>
#include <unistd.h>
#include <wayland-client.h>

#include <chrono>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>

namespace flux::compositor::demo {

inline std::string displayNameFromRuntimeFile() {
  char const* runtimeDir = std::getenv("XDG_RUNTIME_DIR");
  if (!runtimeDir || !*runtimeDir) return {};
  std::ifstream file(std::string(runtimeDir) + "/flux-compositor-display");
  std::string name;
  file >> name;
  return name;
}

inline std::string resolveDisplayName() {
  char const* envDisplay = std::getenv("WAYLAND_DISPLAY");
  if (envDisplay && *envDisplay) return envDisplay;

  std::string fileDisplay = displayNameFromRuntimeFile();
  if (!fileDisplay.empty()) return fileDisplay;

  throw std::runtime_error(
      "WAYLAND_DISPLAY is not set and $XDG_RUNTIME_DIR/flux-compositor-display was not found");
}

inline wl_display* connectDisplay(char const* clientName) {
  std::string displayName = resolveDisplayName();
  std::fprintf(stderr, "%s: connecting to Wayland display %s\n", clientName, displayName.c_str());
  return wl_display_connect(displayName.c_str());
}

inline int dispatchWithTimeout(wl_display* display, int timeoutMs) {
  while (wl_display_prepare_read(display) != 0) {
    int const pending = wl_display_dispatch_pending(display);
    if (pending < 0) return -1;
  }

  wl_display_flush(display);
  pollfd fd{
      .fd = wl_display_get_fd(display),
      .events = POLLIN,
      .revents = 0,
  };
  int const ready = poll(&fd, 1, timeoutMs);
  if (ready < 0) {
    wl_display_cancel_read(display);
    return -1;
  }
  if (ready == 0) {
    wl_display_cancel_read(display);
    return 0;
  }
  if (wl_display_read_events(display) < 0) return -1;
  return wl_display_dispatch_pending(display);
}

template <typename Predicate>
inline bool waitUntil(wl_display* display, Predicate predicate, int timeoutMs) {
  auto const deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
  while (!predicate()) {
    auto const now = std::chrono::steady_clock::now();
    if (now >= deadline) return false;
    auto const remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
    int const sliceMs =
        static_cast<int>(std::min<std::chrono::milliseconds>(remaining, std::chrono::milliseconds(250)).count());
    if (dispatchWithTimeout(display, sliceMs) < 0) return false;
  }
  return true;
}

struct RoundtripState {
  bool done = false;
};

inline void roundtripDone(void* data, wl_callback* callback, std::uint32_t) {
  static_cast<RoundtripState*>(data)->done = true;
  wl_callback_destroy(callback);
}

inline wl_callback_listener const kRoundtripListener{roundtripDone};

inline bool roundtripWithTimeout(wl_display* display, int timeoutMs) {
  RoundtripState state;
  wl_callback* callback = wl_display_sync(display);
  if (!callback) return false;
  wl_callback_add_listener(callback, &kRoundtripListener, &state);
  return waitUntil(display, [&] { return state.done; }, timeoutMs);
}

} // namespace flux::compositor::demo
