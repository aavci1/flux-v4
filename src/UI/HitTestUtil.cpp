#include <Flux/UI/HitTestUtil.hpp>

namespace flux {

std::optional<HitResult> hitTestPointerTarget(EventMap const& em, SceneGraph const& graph,
                                              Point windowPoint) {
  auto const accept = [&em](NodeId id) { return em.find(id) != nullptr; };
  return HitTester{}.hitTest(graph, windowPoint, accept);
}

} // namespace flux
