#pragma once

#include <memory>

namespace flux::scenegraph {

class SceneNode;
class PreparedRenderOps;

namespace detail {

struct SceneNodeAccess {
    static void clearDirty(SceneNode const &node) noexcept;
    static bool ownPaintingDirty(SceneNode const &node) noexcept;
    static bool subtreeDirty(SceneNode const &node) noexcept;
    static void clearSubtreeDirty(SceneNode const &node) noexcept;
    static std::unique_ptr<PreparedRenderOps>& preparedRenderOps(SceneNode const &node) noexcept;
};

} // namespace detail
} // namespace flux::scenegraph
