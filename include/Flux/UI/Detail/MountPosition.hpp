#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/SceneGraph/SceneNode.hpp>

namespace flux::detail {

inline void setLayoutPosition(scenegraph::SceneNode& node, Point origin) {
  Point const childOffset = node.position();
  node.setPosition(Point{origin.x + childOffset.x, origin.y + childOffset.y});
}

} // namespace flux::detail
