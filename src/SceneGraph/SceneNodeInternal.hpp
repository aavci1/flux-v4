#pragma once

namespace flux::scenegraph {

class SceneNode;

namespace detail {

struct SceneNodeAccess {
    static void clearDirty(SceneNode const &node) noexcept;
};

} // namespace detail
} // namespace flux::scenegraph
