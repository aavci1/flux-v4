#include "UI/Layout/Algorithms/StackLayout.hpp"

#include "UI/Layout/LayoutHelpers.hpp"

#include <algorithm>

namespace flux::layout {

StackMainAxisLayout layoutStackMainAxis(std::span<StackMainAxisChild const> children, float spacing,
                                        float assignedMainSize, bool hasAssignedMainSize) {
  StackMainAxisLayout result{};
  result.mainSizes.reserve(children.size());
  result.constrained = hasAssignedMainSize;

  for (StackMainAxisChild const& child : children) {
    result.mainSizes.push_back(std::max(child.naturalMainSize, child.minMainSize));
  }

  if (result.constrained && !result.mainSizes.empty()) {
    float const gaps = children.size() > 1 ? static_cast<float>(children.size() - 1) * spacing : 0.f;
    float const targetSum = std::max(0.f, assignedMainSize - gaps);
    float sumNatural = 0.f;
    for (float size : result.mainSizes) {
      sumNatural += size;
    }

    float const extra = targetSum - sumNatural;
    if (extra > kFlexEpsilon) {
      float totalGrow = 0.f;
      for (StackMainAxisChild const& child : children) {
        totalGrow += child.flexGrow;
      }
      if (totalGrow > kFlexEpsilon) {
        for (std::size_t i = 0; i < children.size(); ++i) {
          if (children[i].flexGrow > 0.f) {
            result.mainSizes[i] += extra * (children[i].flexGrow / totalGrow);
          }
        }
      }
    } else if (extra < -kFlexEpsilon) {
      std::vector<float> const naturalSizes = result.mainSizes;
      for (;;) {
        float allocatedSum = 0.f;
        for (float size : result.mainSizes) {
          allocatedSum += size;
        }
        float const need = allocatedSum - targetSum;
        if (need <= kFlexEpsilon) {
          break;
        }

        float shrinkBasis = 0.f;
        for (std::size_t i = 0; i < children.size(); ++i) {
          if (children[i].flexShrink > 0.f &&
              result.mainSizes[i] > children[i].minMainSize + kFlexEpsilon) {
            shrinkBasis += children[i].flexShrink * naturalSizes[i];
          }
        }
        if (shrinkBasis <= 1e-6f) {
          break;
        }

        float removedThisPass = 0.f;
        for (std::size_t i = 0; i < children.size(); ++i) {
          if (children[i].flexShrink <= 0.f ||
              result.mainSizes[i] <= children[i].minMainSize + kFlexEpsilon) {
            continue;
          }
          float const remove =
              need * ((children[i].flexShrink * naturalSizes[i]) / shrinkBasis);
          float const nextSize = result.mainSizes[i] - remove;
          if (nextSize < children[i].minMainSize) {
            removedThisPass += result.mainSizes[i] - children[i].minMainSize;
            result.mainSizes[i] = children[i].minMainSize;
          } else {
            removedThisPass += remove;
            result.mainSizes[i] = nextSize;
          }
        }
        if (removedThisPass < kFlexEpsilon) {
          break;
        }
      }
    }
  }

  result.usedMainSize = children.size() > 1 ? static_cast<float>(children.size() - 1) * spacing : 0.f;
  for (float size : result.mainSizes) {
    result.usedMainSize += size;
  }

  result.containerMainSize = result.constrained ? std::max(0.f, assignedMainSize) : std::max(0.f, result.usedMainSize);
  result.startOffset = result.constrained ? (result.containerMainSize - result.usedMainSize) * 0.5f : 0.f;
  return result;
}

StackLayoutResult layoutStack(StackAxis axis, Alignment crossAlignment,
                              std::span<Size const> measuredSizes,
                              std::span<float const> mainSizes,
                              float spacing,
                              float containerMainSize,
                              float startOffset,
                              float assignedCrossSize,
                              bool hasAssignedCrossSize) {
  StackLayoutResult result{};
  result.slots.reserve(measuredSizes.size());

  float maxCrossSize = 0.f;
  for (Size const size : measuredSizes) {
    float const crossSize = axis == StackAxis::Vertical ? size.width : size.height;
    maxCrossSize = std::max(maxCrossSize, crossSize);
  }

  float containerCrossSize = maxCrossSize;
  if (axis == StackAxis::Vertical) {
    if (hasAssignedCrossSize) {
      containerCrossSize = std::max(0.f, assignedCrossSize);
    }
  } else if (crossAlignment == Alignment::Stretch && hasAssignedCrossSize) {
    containerCrossSize = std::max(0.f, assignedCrossSize);
  }

  float mainOffset = startOffset;
  for (std::size_t i = 0; i < measuredSizes.size(); ++i) {
    Size const measured = measuredSizes[i];
    float const mainSize = i < mainSizes.size() ? mainSizes[i] : 0.f;
    StackSlot slot{};
    if (axis == StackAxis::Vertical) {
      float const crossSlotSize =
          crossAlignment == Alignment::Stretch && containerCrossSize > 0.f ? containerCrossSize : measured.width;
      slot.origin = Point{
          hAlignOffset(measured.width, containerCrossSize > 0.f ? containerCrossSize : measured.width, crossAlignment),
          mainOffset,
      };
      slot.assignedSize = Size{crossSlotSize, mainSize};
    } else {
      float const crossSlotSize =
          crossAlignment == Alignment::Stretch && containerCrossSize > 0.f ? containerCrossSize : measured.height;
      float const crossSpace = containerCrossSize > 0.f ? containerCrossSize : measured.height;
      slot.origin = Point{
          mainOffset,
          vAlignOffset(measured.height, crossSpace, crossAlignment),
      };
      slot.assignedSize = Size{mainSize, crossSlotSize};
    }
    result.slots.push_back(slot);
    mainOffset += mainSize;
    if (i + 1 < measuredSizes.size()) {
      mainOffset += spacing;
    }
  }

  if (axis == StackAxis::Vertical) {
    result.containerSize = Size{containerCrossSize, containerMainSize};
  } else {
    result.containerSize = Size{containerMainSize, containerCrossSize};
  }
  return result;
}

} // namespace flux::layout
