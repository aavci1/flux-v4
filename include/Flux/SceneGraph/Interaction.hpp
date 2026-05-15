#pragma once

/// \file Flux/SceneGraph/Interaction.hpp
///
/// Abstract interaction payload carried by scene-graph nodes.

#include <Flux/Core/Geometry.hpp>
#include <Flux/Core/Identity.hpp>

namespace flux::scenegraph {

class SceneNode;

class Interaction {
public:
  virtual ~Interaction() = default;

  [[nodiscard]] virtual ComponentKey const& stableTargetKey() const noexcept = 0;
  [[nodiscard]] virtual bool focusable() const = 0;
  [[nodiscard]] virtual bool isEmpty() const noexcept = 0;
};

struct InteractionHitResult {
  SceneNode const* node = nullptr;
  Point localPoint{};
  Interaction const* interaction = nullptr;
};

} // namespace flux::scenegraph
