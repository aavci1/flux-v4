#pragma once

/// \file Flux/UI/SceneGeometryIndex.hpp
///
/// Part of the Flux public API.

#include <Flux/Core/Types.hpp>
#include <Flux/UI/ComponentKey.hpp>

#include <optional>
#include <unordered_map>

namespace flux {

/// Assigned-frame geometry index for retained scene builds.
class SceneGeometryIndex {
public:
  void clear() { rects_.clear(); }

  void record(ComponentKey const& key, Rect rect) {
    if (key.empty()) {
      return;
    }
    rects_[key] = rect;
  }

  [[nodiscard]] std::optional<Rect> rectForKey(ComponentKey const& key) const {
    if (auto it = rects_.find(key); it != rects_.end()) {
      return it->second;
    }
    return std::nullopt;
  }

private:
  std::unordered_map<ComponentKey, Rect, ComponentKeyHash> rects_{};
};

} // namespace flux
