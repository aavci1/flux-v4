#pragma once

/// \file Flux/UI/Views/Svg.hpp
///
/// Declarative SVG-like vector document view.

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/Reactive/Bindable.hpp>
#include <Flux/UI/Detail/PrimitiveForwards.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/ViewModifiers.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace flux {

class MountContext;
namespace scenegraph {
class SceneNode;
}

struct SvgPath {
  Reactive::Bindable<std::string> d{std::string{}};
  Reactive::Bindable<FillStyle> fill{FillStyle::none()};
  Reactive::Bindable<StrokeStyle> stroke{StrokeStyle::none()};
  Reactive::Bindable<float> opacity{1.f};

  bool operator==(SvgPath const&) const = default;
};

struct SvgRect {
  Reactive::Bindable<float> x{0.f};
  Reactive::Bindable<float> y{0.f};
  Reactive::Bindable<float> width{0.f};
  Reactive::Bindable<float> height{0.f};
  Reactive::Bindable<CornerRadius> cornerRadius{CornerRadius{}};
  Reactive::Bindable<FillStyle> fill{FillStyle::none()};
  Reactive::Bindable<StrokeStyle> stroke{StrokeStyle::none()};
  Reactive::Bindable<float> opacity{1.f};

  bool operator==(SvgRect const&) const = default;
};

struct SvgCircle {
  Reactive::Bindable<float> cx{0.f};
  Reactive::Bindable<float> cy{0.f};
  Reactive::Bindable<float> r{0.f};
  Reactive::Bindable<FillStyle> fill{FillStyle::none()};
  Reactive::Bindable<StrokeStyle> stroke{StrokeStyle::none()};
  Reactive::Bindable<float> opacity{1.f};

  bool operator==(SvgCircle const&) const = default;
};

struct SvgEllipse {
  Reactive::Bindable<float> cx{0.f};
  Reactive::Bindable<float> cy{0.f};
  Reactive::Bindable<float> rx{0.f};
  Reactive::Bindable<float> ry{0.f};
  Reactive::Bindable<FillStyle> fill{FillStyle::none()};
  Reactive::Bindable<StrokeStyle> stroke{StrokeStyle::none()};
  Reactive::Bindable<float> opacity{1.f};

  bool operator==(SvgEllipse const&) const = default;
};

struct SvgLine {
  Reactive::Bindable<float> x1{0.f};
  Reactive::Bindable<float> y1{0.f};
  Reactive::Bindable<float> x2{0.f};
  Reactive::Bindable<float> y2{0.f};
  Reactive::Bindable<StrokeStyle> stroke{StrokeStyle::none()};
  Reactive::Bindable<float> opacity{1.f};

  bool operator==(SvgLine const&) const = default;
};

struct SvgPolygon {
  std::vector<Point> points;
  Reactive::Bindable<FillStyle> fill{FillStyle::none()};
  Reactive::Bindable<StrokeStyle> stroke{StrokeStyle::none()};
  Reactive::Bindable<float> opacity{1.f};

  bool operator==(SvgPolygon const&) const = default;
};

struct SvgPolyline {
  std::vector<Point> points;
  Reactive::Bindable<FillStyle> fill{FillStyle::none()};
  Reactive::Bindable<StrokeStyle> stroke{StrokeStyle::none()};
  Reactive::Bindable<float> opacity{1.f};

  bool operator==(SvgPolyline const&) const = default;
};

struct SvgGroup;
struct SvgConditional;

struct SvgNode {
  using GroupPtr = std::shared_ptr<SvgGroup>;
  using ConditionalPtr = std::shared_ptr<SvgConditional>;
  using Storage = std::variant<SvgPath, SvgRect, SvgCircle, SvgEllipse, SvgLine,
                               SvgPolygon, SvgPolyline, GroupPtr, ConditionalPtr>;

  Storage storage{GroupPtr{}};

  SvgNode();
  SvgNode(SvgPath value);
  SvgNode(SvgRect value);
  SvgNode(SvgCircle value);
  SvgNode(SvgEllipse value);
  SvgNode(SvgLine value);
  SvgNode(SvgPolygon value);
  SvgNode(SvgPolyline value);
  SvgNode(SvgGroup value);
  SvgNode(SvgConditional value);

  bool operator==(SvgNode const& other) const;
};

struct SvgGroup {
  Reactive::Bindable<Mat3> transform{Mat3::identity()};
  Reactive::Bindable<float> opacity{1.f};
  std::vector<SvgNode> children;

  bool operator==(SvgGroup const&) const = default;
};

struct SvgConditional {
  Reactive::Bindable<bool> when{false};
  std::vector<SvgNode> children;

  bool operator==(SvgConditional const&) const = default;
};

enum class SvgPreserveAspectRatio : std::uint8_t {
  Meet,
  Slice,
  Stretch,
};

struct Svg : ViewModifiers<Svg> {
  Rect viewBox{0.f, 0.f, 0.f, 0.f};
  SvgPreserveAspectRatio preserveAspectRatio = SvgPreserveAspectRatio::Meet;
  Size intrinsicSize{0.f, 0.f};
  SvgGroup root;

  Size measure(MeasureContext&, LayoutConstraints const&, LayoutHints const&, TextSystem&) const;
  std::unique_ptr<scenegraph::SceneNode> mount(MountContext&) const;

  bool operator==(Svg const&) const = default;
};

SvgGroup translated(float x, float y, std::vector<SvgNode> children);
SvgGroup rotated(float radians, Point pivot, std::vector<SvgNode> children);
SvgGroup scaled(float sx, float sy, std::vector<SvgNode> children);

} // namespace flux
