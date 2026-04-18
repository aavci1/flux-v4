#include <Flux/UI/Detail/LayoutDebugDump.hpp>

#include "UI/Layout/LayoutHelpers.hpp"

#include <cstdio>
#include <unordered_map>

namespace flux {

namespace {

thread_local std::unordered_map<std::uint64_t, Size> gMeasureSize;

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

} // namespace flux
