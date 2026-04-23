#include <Flux/SceneGraph/GroupNode.hpp>

namespace flux::scenegraph {

GroupNode::GroupNode(Rect bounds) : SceneNode(SceneNodeKind::Group, bounds) {}

GroupNode::~GroupNode() = default;

} // namespace flux::scenegraph
