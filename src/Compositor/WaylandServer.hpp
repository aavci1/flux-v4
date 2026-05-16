#pragma once

#include <cstdint>
#include <memory>
#include <optional>
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
  struct DmabufPlane {
    std::uint32_t offset = 0;
    std::uint32_t stride = 0;
    std::uint64_t modifier = 0;
  };

  std::uint64_t id = 0;
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t width = 0;
  std::int32_t height = 0;
  std::int32_t titleBarHeight = 0;
  bool focused = false;
  std::uint64_t serial = 0;
  std::vector<std::uint8_t> rgbaPixels;
  std::uint32_t dmabufFormat = 0;
  std::vector<DmabufPlane> dmabufPlanes;
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
  [[nodiscard]] std::optional<CommittedSurfaceSnapshot> cursorSurface() const;
  [[nodiscard]] std::vector<int> duplicateDmabufFds(std::uint64_t surfaceId) const;
  [[nodiscard]] bool copyDmabufToRgba(std::uint64_t surfaceId, std::vector<std::uint8_t>& out) const;

  void dispatch();
  void flushClients();
  void sendFrameCallbacks(std::uint32_t timeMs);
  void handlePointerMotion(double dx, double dy, std::uint32_t timeMs);
  void handlePointerPosition(double x, double y, std::uint32_t timeMs);
  void handlePointerButton(std::uint32_t button, bool pressed, std::uint32_t timeMs);
  void handlePointerAxis(double dx, double dy, std::uint32_t timeMs);
  void handleKeyboardKey(std::uint32_t key, bool pressed, std::uint32_t timeMs);
  [[nodiscard]] float pointerX() const noexcept { return pointerX_; }
  [[nodiscard]] float pointerY() const noexcept { return pointerY_; }

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
  std::vector<wl_resource*> seatResources_;
  std::vector<wl_resource*> pointerResources_;
  std::vector<wl_resource*> keyboardResources_;
  Surface* pointerFocus_ = nullptr;
  Surface* keyboardFocus_ = nullptr;
  Surface* dragSurface_ = nullptr;
  Surface* cursorSurface_ = nullptr;
  std::int32_t cursorHotspotX_ = 0;
  std::int32_t cursorHotspotY_ = 0;
  float dragOffsetX_ = 0.f;
  float dragOffsetY_ = 0.f;
  float pointerX_ = 32.f;
  float pointerY_ = 32.f;
  std::uint64_t nextSurfaceId_ = 1;
  std::uint32_t nextConfigureSerial_ = 1;
  std::uint32_t nextInputSerial_ = 1;
};

} // namespace flux::compositor
