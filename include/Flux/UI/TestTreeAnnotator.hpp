#pragma once

/// \file Flux/UI/TestTreeAnnotator.hpp
/// Build-time UI tree for `--test-mode` GetUi snapshots (component hierarchy + layout rects).

#include <Flux/Core/Types.hpp>
#include <Flux/UI/ComponentKey.hpp>

#include <memory>
#include <string>
#include <vector>

namespace flux {

class LayoutRectCache;

/// Records component nodes during `BuildContext::build`; bounds filled from `LayoutRectCache` after layout.
class TestTreeAnnotator {
public:
  struct Node {
    ComponentKey key;
    std::string typeName;
    std::string text;
    std::string value;
    std::string focusKey;
    bool interactive = false;
    bool focusable = false;
    Rect bounds{};
    std::vector<std::unique_ptr<Node>> children;
  };

  TestTreeAnnotator();

  Node* root() noexcept { return root_.get(); }
  Node const* root() const noexcept { return root_.get(); }

  void clear();

  /// Push a composite node (call before building children). Pairs with `popComposite`.
  void pushComposite(std::string typeName, ComponentKey const& key, std::string text, std::string value,
                     std::string focusKey, bool interactive, bool focusable);

  void popComposite();

  /// Leaf or non-nested render slot (single node in tree).
  void addLeaf(std::string typeName, ComponentKey const& key, Rect const& bounds, std::string text,
               std::string value, std::string focusKey, bool interactive, bool focusable);

  /// After `LayoutRectCache::fill`, assign `bounds` from cached rects.
  void applyBounds(LayoutRectCache const& cache);

  /// Serialize to JSON (v1-compatible shape: id, type, bounds, text, value, interactive, focusable, focusKey, children).
  std::string toJson() const;

private:
  std::unique_ptr<Node> root_;
  std::vector<Node*> stack_{};

  static std::string keyToId(ComponentKey const& key);
  static void appendFloat(std::string& out, float v);
  static std::string escapeJson(std::string const& s);
  static void serializeNode(Node const& node, std::string const& idPath, std::string& out);
};

} // namespace flux
