#pragma once

/// \file Flux/UI/Overlay.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>
#include <Flux/Scene/SceneGraph.hpp>
#include <Flux/UI/ComponentKey.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/EventMap.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/LayoutTree.hpp>
#include <Flux/UI/MeasureCache.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/Views/PopoverPlacement.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace flux {

class Runtime;

struct OverlayId {
  std::uint64_t value = 0;
  bool isValid() const noexcept;
  bool operator==(OverlayId const&) const = default;
};

inline constexpr OverlayId kInvalidOverlayId{};

struct OverlayConfig {

  enum class CrossAlignment {
    Center,
    Start,
    End,
    PreferStart,
    PreferEnd,
  };

  std::optional<Rect> anchor;

  /// When set, each overlay rebuild refreshes `anchor` from `Runtime::layoutRectForLeafKeyPrefix`
  /// so the popover follows the control (e.g. scroll) instead of staying at the initial tap rect.
  std::optional<ComponentKey> anchorTrackLeafKey;

  /// When set, each overlay rebuild refreshes `anchor` from `Runtime::layoutRectForKey`.
  /// Useful for overlays anchored to a composite trigger rather than the tapped leaf inside it.
  std::optional<ComponentKey> anchorTrackComponentKey;

  /// When set, clamps `anchor.height` after layout union (e.g. row height vs inflated text bounds).
  std::optional<float> anchorMaxHeight;

  /// Expands the resolved anchor rect before overlay placement.
  EdgeInsets anchorOutsets{};

  enum class Placement { Below, Above, End, Start };
  Placement placement = Placement::Below;
  CrossAlignment crossAlignment = CrossAlignment::Center;

  Vec2 offset{};

  std::optional<Size> maxSize;

  bool modal = false;

  /// Modal dialogs and non-modal overlays with non-zero alpha (e.g. dimmed popovers).
  Color backdropColor = Color{0.f, 0.f, 0.f, 0.f};

  /// When set, each rebuild re-resolves placement from \ref anchor (popover flip when scrolling).
  std::optional<PopoverPlacement> popoverPreferredPlacement;
  float popoverGapTotal = 0.f;
  float popoverGap = 0.f;

  bool dismissOnOutsideTap = true;

  bool dismissOnEscape = true;

  std::function<void()> onDismiss;

  /// Optional debug label for overlay placement instrumentation.
  std::string debugName;
};

std::tuple<std::function<void(Element, OverlayConfig)>, std::function<void()>, bool> useOverlay();

struct OverlayEntry {
  OverlayId id{};
  std::optional<Element> content;
  OverlayConfig config;

  SceneGraph graph;
  LayoutTree layoutTree;
  EventMap eventMap;
  std::unique_ptr<StateStore> stateStore = std::make_unique<StateStore>();
  Rect resolvedFrame{};

  /// When overlay content is a \ref Popover, set in \ref OverlayManager::push to update
  /// \ref Popover::resolvedPlacement each rebuild (no \ref Element API hook).
  std::function<void(PopoverPlacement)> onPlacementResolved;

  ComponentKey preFocusKey{};
};

class OverlayManager {
public:
  OverlayManager() = default;

  void rebuild(Size windowSize, Runtime& runtime);

  OverlayId push(Element content, OverlayConfig config, Runtime* runtime);
  void remove(OverlayId id, Runtime* runtime);
  /// If \p invokeDismissCallbacks is false, `config.onDismiss` is not called (used when tearing down
  /// the window so hooks must not re-enter `removeOverlay`).
  void clear(Runtime* runtime, bool invokeDismissCallbacks = true);

  bool hasOverlays() const noexcept;

  OverlayEntry const* top() const;

  std::vector<std::unique_ptr<OverlayEntry>> const& entries() const;

private:
  Rect resolveFrame(Size windowSize, OverlayConfig const& config, Rect contentBounds) const;
  LayoutConstraints resolveConstraints(Size windowSize, OverlayConfig const& config) const;

  void insertOverlayBackdropChrome(OverlayEntry& entry, Size windowSize, Runtime& runtime,
                                   bool dismissOnBackdropTap);

  std::vector<std::unique_ptr<OverlayEntry>> overlays_;
  std::uint64_t nextId_ = 1;
  LayoutEngine layoutEngine_{};
  MeasureCache overlayMeasureCache_{};
};

} // namespace flux
