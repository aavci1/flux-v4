#include <Flux/UI/Detail/LayoutDebugDump.hpp>

#include <Flux/Scene/SceneTree.hpp>
#include <Flux/UI/SceneGeometryIndex.hpp>

#include "UI/Layout/LayoutHelpers.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

namespace flux {

namespace {

thread_local std::unordered_map<std::uint64_t, Size> gMeasureSize;

void printIndent(int depth) {
  for (int i = 0; i < depth; ++i) {
    std::fputs("  ", stderr);
  }
}

std::string formatLocalId(LocalId const& local) {
  char buffer[64];
  if (local.kind == LocalId::Kind::Positional) {
    std::snprintf(buffer, sizeof(buffer), "i:%llu",
                  static_cast<unsigned long long>(local.value == 0 ? 0ull : local.value - 1ull));
  } else {
    std::snprintf(buffer, sizeof(buffer), "k:%016llx", static_cast<unsigned long long>(local.value));
  }
  return std::string(buffer);
}

std::string formatComponentKey(ComponentKey const& key) {
  if (key.empty()) {
    return "<root>";
  }
  std::string out;
  for (std::size_t i = 0; i < key.size(); ++i) {
    if (i != 0) {
      out += '/';
    }
    out += formatLocalId(key[i]);
  }
  return out;
}

void dumpSceneNode(SceneNode const& node, int depth) {
  printIndent(depth);
  std::fprintf(stderr,
               "[flux:layout] node kind=%.*s id=0x%016llx pos=(%.1f, %.1f) bounds=(%.1f, %.1f, %.1f, %.1f)%s\n",
               static_cast<int>(sceneNodeKindName(node.kind()).size()), sceneNodeKindName(node.kind()).data(),
               static_cast<unsigned long long>(node.id().value), static_cast<double>(node.position.x),
               static_cast<double>(node.position.y), static_cast<double>(node.bounds.x),
               static_cast<double>(node.bounds.y), static_cast<double>(node.bounds.width),
               static_cast<double>(node.bounds.height), node.interaction() ? " interactive" : "");
  for (std::unique_ptr<SceneNode> const& child : node.children()) {
    dumpSceneNode(*child, depth + 1);
  }
}

} // namespace

void layoutDebugBeginPass() {
  if (!flux::layout::layoutDebugLayoutEnabled()) {
    return;
  }
  gMeasureSize.clear();
  std::fprintf(stderr, "[flux:layout] --- rebuild ---\n");
}

void layoutDebugEndPass() {
  if (!flux::layout::layoutDebugLayoutEnabled()) {
    return;
  }
  std::fprintf(stderr, "[flux:layout] --- end ---\n");
}

void layoutDebugRecordMeasure(std::uint64_t measureId, LayoutConstraints const&, Size sz) {
  if (!flux::layout::layoutDebugLayoutEnabled()) {
    return;
  }
  gMeasureSize[measureId] = sz;
}

void layoutDebugDumpRetained(SceneTree const& tree, SceneGeometryIndex const& geometry) {
  if (!flux::layout::layoutDebugLayoutEnabled()) {
    return;
  }

  std::fprintf(stderr, "[flux:layout] scene tree:\n");
  dumpSceneNode(tree.root(), 0);

  std::vector<std::pair<ComponentKey, Rect>> entries = geometry.snapshotCurrent();
  std::sort(entries.begin(), entries.end(), [](auto const& lhs, auto const& rhs) {
    ComponentKey const& a = lhs.first;
    ComponentKey const& b = rhs.first;
    std::size_t const common = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < common; ++i) {
      if (a[i].kind != b[i].kind) {
        return static_cast<int>(a[i].kind) < static_cast<int>(b[i].kind);
      }
      if (a[i].value != b[i].value) {
        return a[i].value < b[i].value;
      }
    }
    return a.size() < b.size();
  });

  std::fprintf(stderr, "[flux:layout] geometry entries=%zu\n", entries.size());
  for (auto const& [key, rect] : entries) {
    std::string const keyText = formatComponentKey(key);
    std::fprintf(stderr, "[flux:layout]   key=%s frame=(%.1f, %.1f, %.1f, %.1f)\n", keyText.c_str(),
                 static_cast<double>(rect.x), static_cast<double>(rect.y), static_cast<double>(rect.width),
                 static_cast<double>(rect.height));
  }

  std::fprintf(stderr, "[flux:layout] measures=%zu\n", gMeasureSize.size());
}

} // namespace flux
