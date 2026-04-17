#include <Flux/Scene/SceneTreeDump.hpp>

#include <Flux/Scene/SceneTree.hpp>

#include <ostream>

namespace flux {

void dumpSceneNode(SceneNode const& node, std::ostream& os, int depth) {
  for (int i = 0; i < depth; ++i) {
    os << "  ";
  }
  os << sceneNodeKindName(node.kind()) << " id=" << node.id().value << " pos=(" << node.position.x << ", "
     << node.position.y << ") bounds=(" << node.bounds.x << ", " << node.bounds.y << ", " << node.bounds.width
     << ", " << node.bounds.height << ")";
  if (node.interaction()) {
    os << " interactive";
  }
  os << '\n';
  for (std::unique_ptr<SceneNode> const& child : node.children()) {
    dumpSceneNode(*child, os, depth + 1);
  }
}

void dumpSceneTree(SceneTree const& tree, std::ostream& os) {
  os << "---- SceneTree dump ----\n";
  dumpSceneNode(tree.root(), os, 0);
  os << "---- end SceneTree dump ----\n";
}

} // namespace flux
