#include <Flux/SceneGraph/SceneGraph.hpp>

#include <Flux/SceneGraph/GroupNode.hpp>

#include <stdexcept>
#include <unordered_map>

namespace flux::scenegraph {

struct SceneGraph::Impl {
    std::unique_ptr<SceneNode> root;
    std::unordered_map<ComponentKey, Rect, ComponentKeyHash> currentGeometry;
    std::unordered_map<ComponentKey, Rect, ComponentKeyHash> previousGeometry;
    std::unordered_map<ComponentKey, Rect, ComponentKeyHash> buildingGeometry;
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

void SceneGraph::beginGeometryBuild() {
    impl_->buildingGeometry.clear();
}

void SceneGraph::finishGeometryBuild() {
    impl_->previousGeometry.swap(impl_->currentGeometry);
    impl_->currentGeometry = std::move(impl_->buildingGeometry);
    impl_->buildingGeometry.clear();
}

void SceneGraph::clearGeometry() {
    impl_->currentGeometry.clear();
    impl_->previousGeometry.clear();
    impl_->buildingGeometry.clear();
}

void SceneGraph::recordGeometry(ComponentKey const& key, Rect rect) {
    if (key.empty()) {
        return;
    }
    impl_->buildingGeometry[key] = rect;
}

std::optional<Rect> SceneGraph::rectForKey(ComponentKey const& key) const {
    if (auto it = impl_->currentGeometry.find(key); it != impl_->currentGeometry.end()) {
        return it->second;
    }
    if (auto it = impl_->previousGeometry.find(key); it != impl_->previousGeometry.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<Rect> SceneGraph::rectForLeafKeyPrefix(ComponentKey const& key) const {
    for (std::size_t len = key.size(); len > 0; --len) {
        ComponentKey prefix(key.begin(), key.begin() + static_cast<std::ptrdiff_t>(len));
        if (std::optional<Rect> rect = rectForKey(prefix)) {
            return rect;
        }
    }
    return std::nullopt;
}

std::optional<Rect> SceneGraph::rectForTapAnchor(ComponentKey const& key) const {
    if (key.empty()) {
        return std::nullopt;
    }
    return rectForLeafKeyPrefix(key);
}

std::vector<std::pair<ComponentKey, Rect>> SceneGraph::snapshotGeometry() const {
    std::vector<std::pair<ComponentKey, Rect>> out;
    out.reserve(impl_->currentGeometry.size());
    for (auto const& [key, rect] : impl_->currentGeometry) {
        out.emplace_back(key, rect);
    }
    return out;
}

} // namespace flux::scenegraph
