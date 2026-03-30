#pragma once

#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/UI/ComponentKey.hpp>
#include <Flux/UI/EventMap.hpp>
#include <Flux/UI/Overlay.hpp>
#include <Flux/Scene/SceneGraph.hpp>

#include <vector>

namespace flux {

class Window;
class GestureTracker;

/// Owns cursor resolution for one window.
class CursorController {
public:
  explicit CursorController(Window& window) : window_(window) {}

  void updateForPoint(Point windowPoint, GestureTracker const& gesture,
                      std::vector<OverlayEntry const*> const& overlayEntries, SceneGraph const& mainGraph,
                      EventMap const& mainEventMap);

  void reset() { apply(Cursor::Arrow); }

private:
  void apply(Cursor kind);

  Window& window_;
  Cursor current_{ Cursor::Arrow };
};

} // namespace flux
