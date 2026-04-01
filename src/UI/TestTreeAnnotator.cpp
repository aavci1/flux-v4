#include <Flux/UI/TestTreeAnnotator.hpp>

#include <Flux/UI/LayoutRectCache.hpp>

#include <cstdio>
#include <sstream>

namespace flux {

TestTreeAnnotator::TestTreeAnnotator() = default;

void TestTreeAnnotator::clear() {
  root_.reset();
  stack_.clear();
}

void TestTreeAnnotator::pushComposite(std::string typeName, ComponentKey const& key, std::string text,
                                      std::string value, std::string focusKey, bool interactive, bool focusable) {
  auto node = std::make_unique<Node>();
  node->key = key;
  node->typeName = std::move(typeName);
  node->text = std::move(text);
  node->value = std::move(value);
  node->focusKey = std::move(focusKey);
  node->interactive = interactive;
  node->focusable = focusable;
  Node* raw = node.get();
  if (!root_) {
    root_ = std::move(node);
    stack_.push_back(raw);
    return;
  }
  if (stack_.empty()) {
    return;
  }
  stack_.back()->children.push_back(std::move(node));
  stack_.push_back(raw);
}

void TestTreeAnnotator::popComposite() {
  if (!stack_.empty()) {
    stack_.pop_back();
  }
}

void TestTreeAnnotator::addLeaf(std::string typeName, ComponentKey const& key, Rect const& bounds, std::string text,
                                std::string value, std::string focusKey, bool interactive, bool focusable) {
  auto node = std::make_unique<Node>();
  node->key = key;
  node->typeName = std::move(typeName);
  node->bounds = bounds;
  node->text = std::move(text);
  node->value = std::move(value);
  node->focusKey = std::move(focusKey);
  node->interactive = interactive;
  node->focusable = focusable;
  Node* raw = node.get();
  if (!root_) {
    root_ = std::move(node);
    return;
  }
  if (!stack_.empty()) {
    stack_.back()->children.push_back(std::move(node));
  } else {
    root_->children.push_back(std::move(node));
  }
  (void)raw;
}

void TestTreeAnnotator::applyBounds(LayoutRectCache const& cache) {
  struct W {
    static void walk(Node* n, LayoutRectCache const& c) {
      if (!n) {
        return;
      }
      if (auto r = c.forKey(n->key)) {
        n->bounds = *r;
      }
      for (auto& ch : n->children) {
        walk(ch.get(), c);
      }
    }
  };
  W::walk(root_.get(), cache);
}

std::string TestTreeAnnotator::keyToId(ComponentKey const& key) {
  if (key.empty()) {
    return "0";
  }
  std::ostringstream oss;
  for (std::size_t i = 0; i < key.size(); ++i) {
    if (i > 0) {
      oss << '/';
    }
    oss << key[i];
  }
  return oss.str();
}

void TestTreeAnnotator::appendFloat(std::string& out, float v) {
  char buf[32];
  int n = std::snprintf(buf, sizeof(buf), "%.1f", static_cast<double>(v));
  if (n > 0) {
    out.append(buf, static_cast<std::size_t>(n));
  }
}

std::string TestTreeAnnotator::escapeJson(std::string const& s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out += c;
        break;
    }
  }
  return out;
}

void TestTreeAnnotator::serializeNode(Node const& node, std::string const& idPath, std::string& out) {
  out += '{';
  out += "\"id\":\"";
  out += escapeJson(idPath);
  out += "\",\"type\":\"";
  out += escapeJson(node.typeName);
  out += "\",\"bounds\":{\"x\":";
  appendFloat(out, node.bounds.x);
  out += ",\"y\":";
  appendFloat(out, node.bounds.y);
  out += ",\"w\":";
  appendFloat(out, node.bounds.width);
  out += ",\"h\":";
  appendFloat(out, node.bounds.height);
  out += '}';
  if (!node.text.empty()) {
    out += ",\"text\":\"";
    out += escapeJson(node.text);
    out += '"';
  }
  if (!node.value.empty()) {
    out += ",\"value\":\"";
    out += escapeJson(node.value);
    out += '"';
  }
  if (node.interactive) {
    out += ",\"interactive\":true";
  }
  if (node.focusable) {
    out += ",\"focusable\":true";
  }
  if (!node.focusKey.empty()) {
    out += ",\"focusKey\":\"";
    out += escapeJson(node.focusKey);
    out += '"';
  }
  if (!node.children.empty()) {
    out += ",\"children\":[";
    for (std::size_t i = 0; i < node.children.size(); ++i) {
      if (i > 0) {
        out += ',';
      }
      std::string childId = idPath + "/" + std::to_string(i);
      serializeNode(*node.children[i], childId, out);
    }
    out += ']';
  }
  out += '}';
}

std::string TestTreeAnnotator::toJson() const {
  if (!root_) {
    return "{}";
  }
  std::string out;
  out.reserve(4096);
  serializeNode(*root_, "0", out);
  return out;
}

} // namespace flux
