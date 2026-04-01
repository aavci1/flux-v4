#include <Flux/UI/Detail/LayoutDebugDump.hpp>

#include "UI/Layout/LayoutHelpers.hpp"

#include <algorithm>
#include <cstdio>
#include <unordered_map>
#include <vector>

namespace flux {

namespace {

thread_local std::vector<std::uint64_t> gBuildMeasureStack;
thread_local std::unordered_map<std::uint64_t, Size> gMeasureSize;

void fmtDim(FILE* f, float x) {
  if (!std::isfinite(x)) {
    std::fprintf(f, "inf");
  } else {
    std::fprintf(f, "%.1f", static_cast<double>(x));
  }
}

void fmtConstraints(FILE* f, LayoutConstraints const& cs) {
  fmtDim(f, cs.maxWidth);
  std::fprintf(f, "x");
  fmtDim(f, cs.maxHeight);
}

void fmtSize(FILE* f, Size s) {
  fmtDim(f, s.width);
  std::fprintf(f, "x");
  fmtDim(f, s.height);
}

void fmtRect(FILE* f, Rect r) {
  std::fprintf(f, "{%.1f,%.1f,%.1f,%.1f}", static_cast<double>(r.x), static_cast<double>(r.y),
               static_cast<double>(r.width), static_cast<double>(r.height));
}

int indentSpaces() {
  int const n = static_cast<int>(gBuildMeasureStack.size());
  return std::max(0, n - 1) * 2;
}

} // namespace

void layoutDebugBeginPass() {
  if (!flux::layout::layoutDebugLayoutEnabled()) {
    return;
  }
  gMeasureSize.clear();
  gBuildMeasureStack.clear();
  std::fprintf(stderr, "[flux:layout] --- rebuild ---\n");
}

void layoutDebugEndPass() {
  if (!flux::layout::layoutDebugLayoutEnabled()) {
    return;
  }
  std::fprintf(stderr, "[flux:layout] --- end ---\n");
}

void layoutDebugPushElementBuild(std::uint64_t elementMeasureId) {
  if (!flux::layout::layoutDebugLayoutEnabled()) {
    return;
  }
  gBuildMeasureStack.push_back(elementMeasureId);
}

void layoutDebugPopElementBuild() {
  if (!flux::layout::layoutDebugLayoutEnabled()) {
    return;
  }
  if (!gBuildMeasureStack.empty()) {
    gBuildMeasureStack.pop_back();
  }
}

void layoutDebugRecordMeasure(std::uint64_t measureId, LayoutConstraints const&, Size sz) {
  if (!flux::layout::layoutDebugLayoutEnabled()) {
    return;
  }
  gMeasureSize[measureId] = sz;
}

void layoutDebugLogContainer(char const* tag, LayoutConstraints const& outer, Rect parentFrame) {
  if (!flux::layout::layoutDebugLayoutEnabled()) {
    return;
  }
  Size measured{0.f, 0.f};
  std::uint64_t const id = gBuildMeasureStack.empty() ? 0 : gBuildMeasureStack.back();
  if (id != 0) {
    auto it = gMeasureSize.find(id);
    if (it != gMeasureSize.end()) {
      measured = it->second;
    }
  }
  std::fprintf(stderr, "%*s[%s] constraints: ", indentSpaces(), "", tag);
  fmtConstraints(stderr, outer);
  std::fprintf(stderr, "  measured: ");
  fmtSize(stderr, measured);
  std::fprintf(stderr, "  frame: ");
  fmtRect(stderr, parentFrame);
  std::fprintf(stderr, "\n");
}

void layoutDebugLogLeaf(char const* tag, LayoutConstraints const& cs, Rect frame, float flexGrow,
                        float flexShrink, float minMain) {
  if (!flux::layout::layoutDebugLayoutEnabled()) {
    return;
  }
  Size measured{0.f, 0.f};
  std::uint64_t const id = gBuildMeasureStack.empty() ? 0 : gBuildMeasureStack.back();
  if (id != 0) {
    auto it = gMeasureSize.find(id);
    if (it != gMeasureSize.end()) {
      measured = it->second;
    }
  }
  std::fprintf(stderr, "%*s[%s] constraints: ", indentSpaces(), "", tag);
  fmtConstraints(stderr, cs);
  std::fprintf(stderr, "  measured: ");
  fmtSize(stderr, measured);
  std::fprintf(stderr, "  frame: ");
  fmtRect(stderr, frame);
  if (flexGrow > flux::layout::kFlexEpsilon || flexShrink > flux::layout::kFlexEpsilon ||
      minMain > flux::layout::kFlexEpsilon) {
    std::fprintf(stderr, "  flex: grow=%.2f shrink=%.2f minMain=%.2f", static_cast<double>(flexGrow),
                 static_cast<double>(flexShrink), static_cast<double>(minMain));
  }
  std::fprintf(stderr, "\n");
}

} // namespace flux
