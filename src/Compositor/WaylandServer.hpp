#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct wl_client;
struct wl_display;
struct wl_global;
struct wl_resource;

namespace flux::compositor {

struct WaylandOutputInfo {
  std::string name;
  std::int32_t width = 0;
  std::int32_t height = 0;
  std::int32_t refreshMilliHz = 60'000;
  std::int32_t physicalWidthMm = 0;
  std::int32_t physicalHeightMm = 0;
};

struct CommittedSurfaceSnapshot {
  std::uint64_t id = 0;
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t width = 0;
  std::int32_t height = 0;
  std::uint64_t serial = 0;
  std::vector<std::uint8_t> rgbaPixels;
};

class WaylandServer {
public:
  explicit WaylandServer(WaylandOutputInfo output);
  ~WaylandServer();

  WaylandServer(WaylandServer const&) = delete;
  WaylandServer& operator=(WaylandServer const&) = delete;

  [[nodiscard]] char const* socketName() const noexcept;
  [[nodiscard]] int eventFd() const noexcept;
  [[nodiscard]] std::size_t toplevelCount() const noexcept;
  [[nodiscard]] std::vector<CommittedSurfaceSnapshot> committedSurfaces() const;

  void dispatch();
  void flushClients();
  void sendFrameCallbacks(std::uint32_t timeMs);

  // Protocol callbacks are plain C function pointers, so this implementation
  // state is public to the translation unit callbacks. It remains unexposed to
  // consumers because this header is private to the compositor executable.
  struct Surface;
  struct XdgSurface;
  struct XdgToplevel;
  struct ShmPool;
  struct ShmBuffer;
  struct DmabufParams;
  struct DmabufBuffer;
  struct ToplevelDecoration;

  wl_resource* createSurface(wl_client* client, std::uint32_t version, std::uint32_t id);
  void destroySurface(Surface* surface);
  void destroyXdgSurface(XdgSurface* surface);
  void destroyXdgToplevel(XdgToplevel* toplevel);
  void destroyShmPool(ShmPool* pool);
  void destroyShmBuffer(ShmBuffer* buffer);
  void destroyDmabufParams(DmabufParams* params);
  void destroyDmabufBuffer(DmabufBuffer* buffer);
  void destroyToplevelDecoration(ToplevelDecoration* decoration);

  wl_display* display_ = nullptr;
  wl_global* compositorGlobal_ = nullptr;
  wl_global* shmGlobal_ = nullptr;
  wl_global* outputGlobal_ = nullptr;
  wl_global* seatGlobal_ = nullptr;
  wl_global* xdgWmBaseGlobal_ = nullptr;
  wl_global* linuxDmabufGlobal_ = nullptr;
  wl_global* xdgDecorationManagerGlobal_ = nullptr;
  std::string socketName_;
  WaylandOutputInfo output_;
  std::vector<std::unique_ptr<Surface>> surfaces_;
  std::vector<std::unique_ptr<XdgSurface>> xdgSurfaces_;
  std::vector<std::unique_ptr<XdgToplevel>> toplevels_;
  std::vector<std::unique_ptr<ShmPool>> shmPools_;
  std::vector<std::unique_ptr<ShmBuffer>> shmBuffers_;
  std::vector<std::unique_ptr<DmabufParams>> dmabufParams_;
  std::vector<std::unique_ptr<DmabufBuffer>> dmabufBuffers_;
  std::vector<std::unique_ptr<ToplevelDecoration>> toplevelDecorations_;
  std::uint64_t nextSurfaceId_ = 1;
  std::uint32_t nextConfigureSerial_ = 1;
};

} // namespace flux::compositor
