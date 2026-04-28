#pragma once

#include <algorithm>
#include <memory>
#include <vector>

namespace fluxv5 {

struct ToyNode : std::enable_shared_from_this<ToyNode> {
  float width = 0.0f;
  float height = 0.0f;
  float opacity = 1.0f;
  ToyNode* parent = nullptr;
  std::vector<std::shared_ptr<ToyNode>> children;

  void setSize(float nextWidth, float nextHeight) {
    width = nextWidth;
    height = nextHeight;
  }

  void setOpacity(float nextOpacity) {
    opacity = nextOpacity;
  }

  void addChild(std::shared_ptr<ToyNode> child) {
    child->parent = this;
    children.push_back(std::move(child));
  }

  void insertChild(std::size_t index, std::shared_ptr<ToyNode> child) {
    child->parent = this;
    children.insert(children.begin() + static_cast<std::ptrdiff_t>(index),
      std::move(child));
  }

  void removeChild(ToyNode const* child) {
    auto it = std::remove_if(children.begin(), children.end(),
      [&](std::shared_ptr<ToyNode> const& current) {
        return current.get() == child;
      });
    for (auto cleanup = it; cleanup != children.end(); ++cleanup) {
      (*cleanup)->parent = nullptr;
    }
    children.erase(it, children.end());
  }
};

inline std::shared_ptr<ToyNode> makeToyNode() {
  return std::make_shared<ToyNode>();
}

} // namespace fluxv5
