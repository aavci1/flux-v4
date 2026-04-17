#pragma once

/// \file Flux/UI/SceneGeometryIndex.hpp
///
/// Part of the Flux public API.

#include <Flux/Core/Types.hpp>
#include <Flux/UI/ComponentKey.hpp>

#include <optional>
#include <unordered_map>

namespace flux {

class StateStore;

/// Assigned-frame geometry index for retained scene builds.
class SceneGeometryIndex {
public:
  void beginBuild() { building_.clear(); }
  void finishBuild();
  void clear();

  void record(ComponentKey const& key, Rect rect) {
    if (key.empty()) {
      return;
    }
    building_[key] = rect;
  }

  [[nodiscard]] std::optional<Rect> forCurrentComponent(StateStore const& store) const;
  [[nodiscard]] std::optional<Rect> forKey(ComponentKey const& key) const;
  [[nodiscard]] std::optional<Rect> forLeafKeyPrefix(ComponentKey const& stableTargetKey) const;
  [[nodiscard]] std::optional<Rect> forTapAnchor(ComponentKey const& tapLeafKey) const;

  [[nodiscard]] std::optional<Rect> rectForKey(ComponentKey const& key) const { return forKey(key); }

private:
  std::unordered_map<ComponentKey, Rect, ComponentKeyHash> current_{};
  std::unordered_map<ComponentKey, Rect, ComponentKeyHash> prev_{};
  std::unordered_map<ComponentKey, Rect, ComponentKeyHash> building_{};
};

} // namespace flux
