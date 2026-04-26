#pragma once

#include <Flux/Core/ComponentKey.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/UI/Element.hpp>

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
  bool isValid() const noexcept { return value != 0; }
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

  enum class Placement {
    Below,
    Above,
    End,
    Start,
  };

  std::optional<Rect> anchor;
  std::optional<ComponentKey> anchorTrackLeafKey;
  std::optional<ComponentKey> anchorTrackComponentKey;
  std::optional<float> anchorMaxHeight;
  EdgeInsets anchorOutsets{};
  Placement placement = Placement::Below;
  CrossAlignment crossAlignment = CrossAlignment::Center;
  Vec2 offset{};
  std::optional<Size> maxSize;
  bool modal = false;
  Color backdropColor = Colors::transparent;
  bool dismissOnOutsideTap = true;
  bool dismissOnEscape = true;
  std::function<void()> onDismiss;
  std::string debugName;
};

std::tuple<std::function<void(Element, OverlayConfig)>, std::function<void()>, bool> useOverlay();

struct OverlayEntry {
  OverlayId id{};
  OverlayConfig config;
  scenegraph::SceneGraph sceneGraph;
  Rect resolvedFrame{};
};

class OverlayManager {
public:
  OverlayManager() = default;

  void rebuild(Size windowSize, Runtime& runtime);
  OverlayId push(Element content, OverlayConfig config, Runtime* runtime);
  void remove(OverlayId id, Runtime* runtime);
  void clear(Runtime* runtime, bool invokeDismissCallbacks = true);

  bool hasOverlays() const noexcept { return !overlays_.empty(); }
  OverlayEntry const* top() const;
  OverlayEntry* find(OverlayId id);
  OverlayEntry const* find(OverlayId id) const;
  std::vector<std::unique_ptr<OverlayEntry>> const& entries() const { return overlays_; }

private:
  std::vector<std::unique_ptr<OverlayEntry>> overlays_{};
  std::uint64_t nextId_ = 1;
};

} // namespace flux
