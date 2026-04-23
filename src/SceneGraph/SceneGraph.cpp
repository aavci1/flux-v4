#include <Flux/SceneGraph/SceneGraph.hpp>

#include <Flux/SceneGraph/GroupNode.hpp>

#include <stdexcept>

namespace flux::scenegraph {

struct SceneGraph::Impl {
    std::unique_ptr<SceneNode> root;
};

SceneGraph::SceneGraph() : impl_(std::make_unique<Impl>()) {
    impl_->root = std::make_unique<GroupNode>();
}

SceneGraph::~SceneGraph() = default;

SceneGraph::SceneGraph(std::unique_ptr<SceneNode> root) : impl_(std::make_unique<Impl>()) {
    setRoot(std::move(root));
}

SceneNode &SceneGraph::root() noexcept {
    return *impl_->root;
}

SceneNode const &SceneGraph::root() const noexcept {
    return *impl_->root;
}

void SceneGraph::setRoot(std::unique_ptr<SceneNode> root) {
    if (!root) {
        throw std::invalid_argument("SceneGraph root must not be null");
    }
    impl_->root = std::move(root);
}

} // namespace flux::scenegraph
