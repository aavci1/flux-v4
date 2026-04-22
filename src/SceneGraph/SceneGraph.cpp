#include <Flux/SceneGraph/SceneGraph.hpp>

#include <stdexcept>

namespace flux::scenegraph {

SceneGraph::SceneGraph() : root_(std::make_unique<GroupNode>()) {}

SceneGraph::SceneGraph(std::unique_ptr<SceneNode> root) {
    setRoot(std::move(root));
}

void SceneGraph::setRoot(std::unique_ptr<SceneNode> root) {
    if (!root) {
        throw std::invalid_argument("SceneGraph root must not be null");
    }
    root_ = std::move(root);
}

} // namespace flux::scenegraph
