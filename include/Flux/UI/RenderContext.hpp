#pragma once

/// \file Flux/UI/RenderContext.hpp
///
/// Second phase after layout: emit SceneGraph nodes and EventMap entries.

#include <Flux/Scene/NodeId.hpp>
#include <Flux/Scene/Nodes.hpp>
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
  explicit RenderContext(SceneGraph& g, EventMap& em, TextSystem& ts, bool incrementalSceneReuse = false);

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

  bool incrementalSceneReuseEnabled() const noexcept;

  void beginCapture(std::vector<NodeId>* out);
  void endCapture();
  NodeId addLayer(NodeId parent, LayerNode node);
  NodeId addRect(NodeId parent, RectNode node);
  NodeId addText(NodeId parent, TextNode node);
  NodeId addImage(NodeId parent, ImageNode node);
  NodeId addPath(NodeId parent, PathNode node);
  NodeId addLine(NodeId parent, LineNode node);
  NodeId addCustomRender(NodeId parent, CustomRenderNode node);

private:
  void recordCaptured(NodeId id);

  SceneGraph& graph_;
  EventMap& eventMap_;
  TextSystem& textSystem_;
  bool incrementalSceneReuse_ = false;
  std::vector<NodeId> layerStack_;

  struct LayoutFrame {
    LayoutConstraints constraints{};
    LayoutHints hints{};
  };
  std::vector<LayoutFrame> layoutStack_;

  std::vector<ElementModifiers const*> activeElementModifiers_{};
  std::vector<bool> suppressLeafModifierEvents_{};
  std::vector<std::vector<NodeId>*> captureStack_{};
};

} // namespace flux
