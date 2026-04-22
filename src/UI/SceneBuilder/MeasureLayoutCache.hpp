#pragma once

#include <Flux/UI/ComponentKey.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include "UI/Layout/Algorithms/GridLayout.hpp"
#include "UI/Layout/Algorithms/StackLayout.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>

namespace flux::detail {

struct MeasureLayoutKey {
  std::uint64_t measureId = 0;
  ComponentKey componentKey{};
  LayoutConstraints constraints{};
  LayoutHints hints{};

  bool operator==(MeasureLayoutKey const& other) const noexcept {
    return measureId == other.measureId && componentKey == other.componentKey &&
           constraints.maxWidth == other.constraints.maxWidth &&
           constraints.maxHeight == other.constraints.maxHeight &&
           constraints.minWidth == other.constraints.minWidth &&
           constraints.minHeight == other.constraints.minHeight &&
           hints.hStackCrossAlign == other.hints.hStackCrossAlign &&
           hints.vStackCrossAlign == other.hints.vStackCrossAlign &&
           hints.zStackHorizontalAlign == other.hints.zStackHorizontalAlign &&
           hints.zStackVerticalAlign == other.hints.zStackVerticalAlign;
  }
};

struct MeasureLayoutKeyHash {
  std::size_t operator()(MeasureLayoutKey const& key) const noexcept {
    std::size_t seed = std::hash<std::uint64_t>{}(key.measureId);
    auto mix = [&](std::size_t value) {
      seed ^= value + 0x9e3779b9u + (seed << 6) + (seed >> 2);
    };
    mix(ComponentKeyHash{}(key.componentKey));
    mix(std::hash<float>{}(key.constraints.maxWidth));
    mix(std::hash<float>{}(key.constraints.maxHeight));
    mix(std::hash<float>{}(key.constraints.minWidth));
    mix(std::hash<float>{}(key.constraints.minHeight));
    auto hashAlignment = [](std::optional<Alignment> alignment) -> std::size_t {
      return alignment.has_value() ? std::hash<int>{}(static_cast<int>(*alignment) + 1) : 0u;
    };
    mix(hashAlignment(key.hints.hStackCrossAlign));
    mix(hashAlignment(key.hints.vStackCrossAlign));
    mix(hashAlignment(key.hints.zStackHorizontalAlign));
    mix(hashAlignment(key.hints.zStackVerticalAlign));
    return seed;
  }
};

class MeasureLayoutCache {
public:
  void clear() noexcept {
    elementSizes_.clear();
    stackLayouts_.clear();
    zStackSizes_.clear();
    gridLayouts_.clear();
  }

  void recordElementSize(MeasureLayoutKey key, Size size) {
    elementSizes_[std::move(key)] = size;
  }

  [[nodiscard]] Size const* findElementSize(MeasureLayoutKey const& key) const {
    auto const it = elementSizes_.find(key);
    return it == elementSizes_.end() ? nullptr : &it->second;
  }

  void recordStackLayout(MeasureLayoutKey key, layout::StackLayoutResult result) {
    stackLayouts_[std::move(key)] = std::move(result);
  }

  [[nodiscard]] layout::StackLayoutResult const* findStackLayout(MeasureLayoutKey const& key) const {
    auto const it = stackLayouts_.find(key);
    return it == stackLayouts_.end() ? nullptr : &it->second;
  }

  void recordZStackSize(MeasureLayoutKey key, Size size) {
    zStackSizes_[std::move(key)] = size;
  }

  [[nodiscard]] Size const* findZStackSize(MeasureLayoutKey const& key) const {
    auto const it = zStackSizes_.find(key);
    return it == zStackSizes_.end() ? nullptr : &it->second;
  }

  void recordGridLayout(MeasureLayoutKey key, layout::GridLayoutResult result) {
    gridLayouts_[std::move(key)] = std::move(result);
  }

  [[nodiscard]] layout::GridLayoutResult const* findGridLayout(MeasureLayoutKey const& key) const {
    auto const it = gridLayouts_.find(key);
    return it == gridLayouts_.end() ? nullptr : &it->second;
  }

private:
  std::unordered_map<MeasureLayoutKey, Size, MeasureLayoutKeyHash> elementSizes_{};
  std::unordered_map<MeasureLayoutKey, layout::StackLayoutResult, MeasureLayoutKeyHash> stackLayouts_{};
  std::unordered_map<MeasureLayoutKey, Size, MeasureLayoutKeyHash> zStackSizes_{};
  std::unordered_map<MeasureLayoutKey, layout::GridLayoutResult, MeasureLayoutKeyHash> gridLayouts_{};
};

} // namespace flux::detail
