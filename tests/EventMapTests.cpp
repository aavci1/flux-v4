#include <Flux/UI/EventMap.hpp>

#include <doctest/doctest.h>

namespace flux {

TEST_CASE("EventMap: closest key fallback finds nearest descendant") {
    EventMap map;
    bool tapped = false;

    map.insert(NodeId {1, 1}, EventHandlers {
                                  .stableTargetKey = ComponentKey {2, 4, 1},
                                  .onTap = [&tapped] { tapped = true; },
                              });

    auto const [id, h] = map.findClosestWithIdByKey(ComponentKey {2, 4});
    REQUIRE(id == NodeId {1, 1});
    REQUIRE(h != nullptr);
    REQUIRE(h->onTap != nullptr);

    h->onTap();
    CHECK(tapped);
}

TEST_CASE("EventMap: closest key fallback finds nearest ancestor but not sibling") {
    EventMap map;

    map.insert(NodeId {1, 1}, EventHandlers {
                                  .stableTargetKey = ComponentKey {5, 1},
                                  .onTap = [] {},
                              });
    map.insert(NodeId {2, 1}, EventHandlers {
                                  .stableTargetKey = ComponentKey {5, 2},
                                  .onTap = [] {},
                              });

    auto const [ancestorId, ancestorHandlers] =
        map.findClosestWithIdByKey(ComponentKey {5, 1, 3});
    CHECK(ancestorId == NodeId {1, 1});
    CHECK(ancestorHandlers != nullptr);

    auto const [missId, missHandlers] = map.findClosestWithIdByKey(ComponentKey {5, 3});
    CHECK(missId == kInvalidNodeId);
    CHECK(missHandlers == nullptr);
}

} // namespace flux
