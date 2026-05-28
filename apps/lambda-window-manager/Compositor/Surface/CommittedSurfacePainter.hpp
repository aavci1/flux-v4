#pragma once

#include "Compositor/Chrome/ChromeConfig.hpp"
#include "Compositor/Wayland/WaylandTypes.hpp"

#include <Lambda/Graphics/Canvas.hpp>
#include <Lambda/Graphics/Image.hpp>
#include <Lambda/Graphics/TextSystem.hpp>

#include <chrono>

namespace lambda::compositor {

inline constexpr std::chrono::milliseconds kSurfaceOpenAnimationDuration{140};

struct SurfaceVisualState {
  std::chrono::steady_clock::time_point firstSeen{};
  CommittedSurfaceSnapshot lastSnapshot{};
  bool hasLastSnapshot = false;
};

[[nodiscard]] bool shouldTraceRenderSnapshot(CommittedSurfaceSnapshot const& current,
                                             SurfaceVisualState const& visual);

void drawCommittedSurfaceSnapshot(Canvas& canvas,
                                  TextSystem& textSystem,
                                  CommittedSurfaceSnapshot const& surface,
                                  SurfaceVisualState& visual,
                                  Image& clientImage,
                                  std::chrono::steady_clock::time_point frameTime,
                                  ChromeConfig const& chrome,
                                  bool animationsEnabled);

} // namespace lambda::compositor
