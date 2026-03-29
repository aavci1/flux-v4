#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Scene/SceneGraph.hpp>
#include <Flux/UI/ComponentKey.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/EventMap.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/StateStore.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <tuple>
#include <vector>

namespace flux {

class Runtime;

struct OverlayId {
  std::uint64_t value = 0;
  bool isValid() const noexcept { return value != 0; }
  bool operator==(OverlayId const&) const = default;
};

inline constexpr OverlayId kInvalidOverlayId{};

struct OverlayConfig {

  std::optional<Rect> anchor;

  enum class Placement { Below, Above, End, Start };
  Placement placement = Placement::Below;

  Vec2 offset{};

  std::optional<Size> maxSize;

  bool modal = false;

  Color backdropColor = Color{0.f, 0.f, 0.f, 0.4f};

  bool dismissOnOutsideTap = true;

  bool dismissOnEscape = true;

  std::function<void()> onDismiss;
};

std::tuple<std::function<void(Element, OverlayConfig)>, std::function<void()>, bool> useOverlay();

struct OverlayEntry {
  OverlayId id{};
  std::optional<Element> content;
  OverlayConfig config;

  SceneGraph graph;
  EventMap eventMap;
  std::unique_ptr<StateStore> stateStore = std::make_unique<StateStore>();
  Rect resolvedFrame{};

  ComponentKey preFocusKey{};
};

class OverlayManager {
public:
  OverlayManager() = default;

  void rebuild(Size windowSize, Runtime& runtime);

  OverlayId push(Element content, OverlayConfig config, Runtime* runtime);
  void remove(OverlayId id, Runtime* runtime);
  void clear(Runtime* runtime);

  bool hasOverlays() const noexcept { return !overlays_.empty(); }

  OverlayEntry const* top() const;

  std::vector<std::unique_ptr<OverlayEntry>> const& entries() const { return overlays_; }

private:
  Rect resolveFrame(Size windowSize, OverlayConfig const& config, Size contentSize) const;
  LayoutConstraints resolveConstraints(Size windowSize, OverlayConfig const& config) const;

  void insertModalChrome(OverlayEntry& entry, Size windowSize);

  std::vector<std::unique_ptr<OverlayEntry>> overlays_;
  std::uint64_t nextId_ = 1;
  LayoutEngine layoutEngine_{};
};

} // namespace flux
