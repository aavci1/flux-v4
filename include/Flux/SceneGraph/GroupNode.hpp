#pragma once

/// \file Flux/SceneGraph/GroupNode.hpp
///
/// Scene-graph group node. Groups carry bounds and child hierarchy but no direct draw payload.

#include <Flux/SceneGraph/SceneNode.hpp>

namespace flux::scenegraph {

class GroupNode final : public SceneNode {
  public:
    explicit GroupNode(Rect bounds = {});
    ~GroupNode() override;
};

} // namespace flux::scenegraph
