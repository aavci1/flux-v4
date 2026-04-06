#pragma once

/// \file Flux/UI/LayoutTree.hpp
///
/// Intermediate layout result: geometry and structure only (no SceneGraph).

#include <Flux/Core/Types.hpp>
#include <Flux/UI/ComponentKey.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include <cassert>
#include <cstdint>
#include <span>
#include <vector>

namespace flux {

struct ElementModifiers;

class Element;

/// Unique identifier for a node in the LayoutTree (1-based index; 0 = invalid).
struct LayoutNodeId {
  std::uint32_t value = 0;

  constexpr bool isValid() const noexcept { return value != 0; }

  std::size_t index() const noexcept {
#ifndef NDEBUG
    assert(value > 0);
#endif
    return static_cast<std::size_t>(value - 1U);
  }

  static LayoutNodeId fromIndex(std::size_t i) noexcept {
    return LayoutNodeId{static_cast<std::uint32_t>(i + 1U)};
  }
};

constexpr bool operator==(LayoutNodeId a, LayoutNodeId b) noexcept {
  return a.value == b.value;
}
constexpr bool operator!=(LayoutNodeId a, LayoutNodeId b) noexcept { return !(a == b); }

/// How a container layer is emitted in the render phase (matches ContainerBuildScope behavior).
struct ContainerLayerSpec {
  enum class Kind : std::uint8_t {
    Standard,         ///< translate(parentFrame.x, parentFrame.y)
    OffsetScroll,     ///< translate(parentFrame.x - ox, parentFrame.y - oy)
    ScaleAroundCenter ///< full Mat3 from ScaleAroundCenter
  };

  Kind kind = Kind::Standard;
  Point scrollOffset{};
  float scale = 1.f;
  Point scaleCenter{};
  Mat3 customTransform = Mat3::identity(); ///< used when kind == ScaleAroundCenter (full layer matrix)
  bool clip = false;
  float clipW = 0.f;
  float clipH = 0.f;
};

/// One node in the layout result tree.
struct LayoutNode {
  LayoutNodeId id{};
  LayoutNodeId parent{};

  /// Bounds in the current layer's local coordinate system (same convention as \ref LayoutEngine frames).
  Rect frame{};

  /// Axis-aligned bounds in window / root space (for \ref LayoutRectCache and debugging).
  Rect worldBounds{};

  enum class Kind : std::uint8_t {
    Container,
    Leaf,
    Modifier,
    Composite,
    Tombstone,
  };
  Kind kind = Kind::Leaf;

  /// For Container nodes: child node IDs in layout / render order.
  std::vector<LayoutNodeId> children;

  LayoutConstraints constraints{};
  LayoutHints hints{};

  ComponentKey componentKey{};

  /// For Modifier nodes: resolved modifier state (owned by the Element tree).
  ElementModifiers const* modifiers = nullptr;

  /// Source element for the render phase (valid for the duration of layout + render).
  Element const* element = nullptr;

  ContainerLayerSpec containerSpec{};

  /// When set, this leaf is a \ref RenderComponent custom draw (see \ref Element::Model render path).
  bool isCustomRenderLeaf = false;

  /// Optional tag for render-phase special cases (e.g. popover chrome ordering).
  enum class ContainerTag : std::uint8_t { None, PopoverCalloutShape };
  ContainerTag containerTag = ContainerTag::None;

  /// Modifier pass: scene layer transform relative to parent (when effect layer is used).
  bool modifierHasEffectLayer = false;
  Mat3 modifierLayerTransform = Mat3::identity();
};

/// Complete layout result for one tree (main or overlay).
class LayoutTree {
public:
  void clear() {
    nodes_.clear();
    rootId_ = {};
  }

  [[nodiscard]] LayoutNodeId root() const noexcept { return rootId_; }

  [[nodiscard]] LayoutNode const* get(LayoutNodeId id) const noexcept {
    if (!id.isValid()) {
      return nullptr;
    }
    std::size_t const i = id.index();
    return i < nodes_.size() ? &nodes_[i] : nullptr;
  }

  [[nodiscard]] LayoutNode* get(LayoutNodeId id) noexcept {
    if (!id.isValid()) {
      return nullptr;
    }
    std::size_t const i = id.index();
    return i < nodes_.size() ? &nodes_[i] : nullptr;
  }

  [[nodiscard]] std::span<LayoutNode const> nodes() const noexcept { return nodes_; }

  /// Union of \p nodeId's subtree \ref LayoutNode::worldBounds (including the root node).
  [[nodiscard]] Rect unionSubtreeWorldBounds(LayoutNodeId nodeId) const;

  [[nodiscard]] std::optional<Rect> rectForKey(ComponentKey const& key) const;

  /// Internal: append node; returns assigned id. If \p parent is invalid, this becomes the root.
  LayoutNodeId pushNode(LayoutNode&& node, LayoutNodeId parent);

  void setRoot(LayoutNodeId id) noexcept { rootId_ = id; }

private:
  std::vector<LayoutNode> nodes_{};
  LayoutNodeId rootId_{};
};

/// Axis-aligned bounding rect of \p r after transforming its four corners by \p t.
[[nodiscard]] Rect transformWorldBounds(Mat3 const& t, Rect const& r);

} // namespace flux
