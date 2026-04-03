#pragma once

/// \file Flux/UI/RenderContext.hpp
///
/// Second phase after layout: emit SceneGraph nodes and EventMap entries.

#include <Flux/Scene/NodeId.hpp>
#include <Flux/UI/ComponentKey.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include <vector>

namespace flux {

class SceneGraph;
class TextSystem;
class EventMap;
struct ElementModifiers;

class RenderContext {
public:
  explicit RenderContext(SceneGraph& g, EventMap& em, TextSystem& ts);

  SceneGraph& graph();
  EventMap& eventMap();
  TextSystem& textSystem();

  [[nodiscard]] NodeId parentLayer() const;

  void pushLayer(NodeId layerId);
  void popLayer();

  LayoutConstraints const& constraints() const;
  LayoutHints const& hints() const;
  void pushConstraints(LayoutConstraints const& c, LayoutHints hints = {});
  void popConstraints();

  void pushActiveElementModifiers(ElementModifiers const* m);
  void popActiveElementModifiers();
  ElementModifiers const* activeElementModifiers() const noexcept;

  void pushSuppressLeafModifierEvents(bool suppress);
  void popSuppressLeafModifierEvents();
  bool suppressLeafModifierEvents() const noexcept;

private:
  SceneGraph& graph_;
  EventMap& eventMap_;
  TextSystem& textSystem_;
  std::vector<NodeId> layerStack_;

  struct LayoutFrame {
    LayoutConstraints constraints{};
    LayoutHints hints{};
  };
  std::vector<LayoutFrame> layoutStack_;

  std::vector<ElementModifiers const*> activeElementModifiers_{};
  std::vector<bool> suppressLeafModifierEvents_{};
};

} // namespace flux
