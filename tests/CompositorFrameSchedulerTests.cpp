#include "Compositor/Wayland/WaylandServerImpl.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <vector>

TEST_CASE("frame callbacks are limited to surfaces in the presented frame") {
  using lambda::compositor::WaylandServer;
  using lambda::compositor::surfaceParticipatesInPresentedFrame;

  WaylandServer::Impl::Surface visible{};
  visible.id = 10;
  WaylandServer::Impl::Surface notPresented{};
  notPresented.id = 20;
  WaylandServer::Impl::Surface zeroId{};

  std::vector<std::uint64_t> const presentedSurfaceIds{10, 30};

  CHECK(surfaceParticipatesInPresentedFrame(&visible, presentedSurfaceIds));
  CHECK_FALSE(surfaceParticipatesInPresentedFrame(&notPresented, presentedSurfaceIds));
  CHECK_FALSE(surfaceParticipatesInPresentedFrame(&zeroId, presentedSurfaceIds));
  CHECK_FALSE(surfaceParticipatesInPresentedFrame(nullptr, presentedSurfaceIds));
}
