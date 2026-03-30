#include <Flux/UI/HitTestUtil.hpp>

namespace flux {

std::optional<HitResult> hitTestPointerTarget(EventMap const& em, SceneGraph const& graph,
                                              Point windowPoint) {
  auto const acceptAll = [&em](NodeId id) { return em.find(id) != nullptr; };
  auto const acceptPrimary = [&em](NodeId id) {
    EventHandlers const* h = em.find(id);
    if (!h) {
      return false;
    }
    if (h->cursorPassthrough) {
      return false;
    }
    return true;
  };
  HitTester tester{};
  if (auto r = tester.hitTest(graph, windowPoint, acceptPrimary)) {
    return r;
  }
  return tester.hitTest(graph, windowPoint, acceptAll);
}

} // namespace flux
