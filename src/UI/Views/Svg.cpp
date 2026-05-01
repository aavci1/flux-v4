#include <Flux/UI/Views/Svg.hpp>

#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Graphics/SvgPath.hpp>
#include <Flux/Reactive/Effect.hpp>
#include <Flux/SceneGraph/RenderNode.hpp>
#include <Flux/UI/MeasureContext.hpp>
#include <Flux/UI/MountContext.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace flux {

SvgNode::SvgNode() = default;
SvgNode::SvgNode(SvgPath value) : storage(std::move(value)) {}
SvgNode::SvgNode(SvgRect value) : storage(std::move(value)) {}
SvgNode::SvgNode(SvgCircle value) : storage(std::move(value)) {}
SvgNode::SvgNode(SvgEllipse value) : storage(std::move(value)) {}
SvgNode::SvgNode(SvgLine value) : storage(std::move(value)) {}
SvgNode::SvgNode(SvgPolygon value) : storage(std::move(value)) {}
SvgNode::SvgNode(SvgPolyline value) : storage(std::move(value)) {}
SvgNode::SvgNode(SvgGroup value) : storage(std::make_shared<SvgGroup>(std::move(value))) {}
SvgNode::SvgNode(SvgConditional value) : storage(std::make_shared<SvgConditional>(std::move(value))) {}

bool SvgNode::operator==(SvgNode const& other) const {
  if (storage.index() != other.storage.index()) {
    return false;
  }
  return std::visit([](auto const& lhs, auto const& rhs) -> bool {
    using L = std::decay_t<decltype(lhs)>;
    using R = std::decay_t<decltype(rhs)>;
    if constexpr (!std::is_same_v<L, R>) {
      return false;
    } else if constexpr (std::is_same_v<L, GroupPtr>) {
      if (!lhs || !rhs) {
        return lhs == rhs;
      }
      return *lhs == *rhs;
    } else if constexpr (std::is_same_v<L, ConditionalPtr>) {
      if (!lhs || !rhs) {
        return lhs == rhs;
      }
      return *lhs == *rhs;
    } else {
      return lhs == rhs;
    }
  }, storage, other.storage);
}

namespace {

Size clampSize(Size size, LayoutConstraints const& constraints) {
  if (std::isfinite(constraints.maxWidth) && constraints.maxWidth >= 0.f) {
    size.width = std::min(size.width, constraints.maxWidth);
  }
  if (std::isfinite(constraints.maxHeight) && constraints.maxHeight >= 0.f) {
    size.height = std::min(size.height, constraints.maxHeight);
  }
  size.width = std::max(size.width, constraints.minWidth);
  size.height = std::max(size.height, constraints.minHeight);
  return Size{std::max(0.f, size.width), std::max(0.f, size.height)};
}

Size assignedSize(LayoutConstraints const& constraints, Size measured) {
  Size size = measured;
  if (std::isfinite(constraints.maxWidth) && constraints.maxWidth > 0.f) {
    size.width = constraints.maxWidth;
  }
  if (std::isfinite(constraints.maxHeight) && constraints.maxHeight > 0.f) {
    size.height = constraints.maxHeight;
  }
  size.width = std::max(size.width, constraints.minWidth);
  size.height = std::max(size.height, constraints.minHeight);
  return Size{std::max(0.f, size.width), std::max(0.f, size.height)};
}

Size preferredSize(Svg const& svg) {
  if (svg.intrinsicSize.width > 0.f && svg.intrinsicSize.height > 0.f) {
    return svg.intrinsicSize;
  }
  if (svg.viewBox.width <= 0.f || svg.viewBox.height <= 0.f) {
    return Size{0.f, 0.f};
  }
  return Size{svg.viewBox.width, svg.viewBox.height};
}

Size measureSvg(Svg const& svg, LayoutConstraints const& constraints) {
  return clampSize(preferredSize(svg), constraints);
}

void withOpacity(Canvas& canvas, float opacity, auto&& draw) {
  float const clamped = std::clamp(opacity, 0.f, 1.f);
  if (clamped <= 0.f) {
    return;
  }
  if (std::abs(clamped - 1.f) <= 0.0001f) {
    draw();
    return;
  }
  canvas.save();
  canvas.setOpacity(canvas.opacity() * clamped);
  draw();
  canvas.restore();
}

Path pathFromPoints(std::vector<Point> const& points, bool close) {
  Path path;
  if (points.empty()) {
    return path;
  }
  path.moveTo(points.front());
  for (std::size_t i = 1; i < points.size(); ++i) {
    path.lineTo(points[i]);
  }
  if (close) {
    path.close();
  }
  return path;
}

Path const& cachedPathFor(SvgPath const& element, std::unordered_map<std::string, Path>& cache,
                          Path& transient) {
  std::string d = element.d.evaluate();
  if (element.d.isReactive()) {
    transient = parseSvgPath(d);
    return transient;
  }
  auto it = cache.find(d);
  if (it == cache.end()) {
    it = cache.emplace(d, parseSvgPath(d)).first;
  }
  return it->second;
}

void drawNode(Canvas& canvas, SvgNode const& node, std::unordered_map<std::string, Path>& pathCache);

void drawGroup(Canvas& canvas, SvgGroup const& group, std::unordered_map<std::string, Path>& pathCache) {
  withOpacity(canvas, group.opacity.evaluate(), [&] {
    canvas.save();
    canvas.transform(group.transform.evaluate());
    for (SvgNode const& child : group.children) {
      drawNode(canvas, child, pathCache);
    }
    canvas.restore();
  });
}

void drawConditional(Canvas& canvas, SvgConditional const& conditional,
                     std::unordered_map<std::string, Path>& pathCache) {
  if (!conditional.when.evaluate()) {
    return;
  }
  for (SvgNode const& child : conditional.children) {
    drawNode(canvas, child, pathCache);
  }
}

void drawNode(Canvas& canvas, SvgNode const& node, std::unordered_map<std::string, Path>& pathCache) {
  std::visit([&](auto const& value) {
    using T = std::decay_t<decltype(value)>;
    if constexpr (std::is_same_v<T, SvgPath>) {
      Path transient;
      Path const& path = cachedPathFor(value, pathCache, transient);
      withOpacity(canvas, value.opacity.evaluate(), [&] {
        canvas.drawPath(path, value.fill.evaluate(), value.stroke.evaluate());
      });
    } else if constexpr (std::is_same_v<T, SvgRect>) {
      Rect const rect{value.x.evaluate(), value.y.evaluate(), value.width.evaluate(), value.height.evaluate()};
      withOpacity(canvas, value.opacity.evaluate(), [&] {
        canvas.drawRect(rect, value.cornerRadius.evaluate(), value.fill.evaluate(), value.stroke.evaluate());
      });
    } else if constexpr (std::is_same_v<T, SvgCircle>) {
      withOpacity(canvas, value.opacity.evaluate(), [&] {
        canvas.drawCircle(Point{value.cx.evaluate(), value.cy.evaluate()}, value.r.evaluate(),
                          value.fill.evaluate(), value.stroke.evaluate());
      });
    } else if constexpr (std::is_same_v<T, SvgEllipse>) {
      Path path;
      path.ellipse(Point{value.cx.evaluate(), value.cy.evaluate()}, value.rx.evaluate(), value.ry.evaluate());
      withOpacity(canvas, value.opacity.evaluate(), [&] {
        canvas.drawPath(path, value.fill.evaluate(), value.stroke.evaluate());
      });
    } else if constexpr (std::is_same_v<T, SvgLine>) {
      withOpacity(canvas, value.opacity.evaluate(), [&] {
        canvas.drawLine(Point{value.x1.evaluate(), value.y1.evaluate()},
                        Point{value.x2.evaluate(), value.y2.evaluate()},
                        value.stroke.evaluate());
      });
    } else if constexpr (std::is_same_v<T, SvgPolygon>) {
      Path path = pathFromPoints(value.points, true);
      withOpacity(canvas, value.opacity.evaluate(), [&] {
        canvas.drawPath(path, value.fill.evaluate(), value.stroke.evaluate());
      });
    } else if constexpr (std::is_same_v<T, SvgPolyline>) {
      Path path = pathFromPoints(value.points, false);
      withOpacity(canvas, value.opacity.evaluate(), [&] {
        canvas.drawPath(path, value.fill.evaluate(), value.stroke.evaluate());
      });
    } else if constexpr (std::is_same_v<T, SvgNode::GroupPtr>) {
      if (value) {
        drawGroup(canvas, *value, pathCache);
      }
    } else if constexpr (std::is_same_v<T, SvgNode::ConditionalPtr>) {
      if (value) {
        drawConditional(canvas, *value, pathCache);
      }
    }
  }, node.storage);
}

Mat3 viewBoxTransform(Rect viewBox, Rect frame, SvgPreserveAspectRatio preserveAspectRatio) {
  if (viewBox.width <= 0.f || viewBox.height <= 0.f || frame.width <= 0.f || frame.height <= 0.f) {
    return Mat3::identity();
  }

  if (preserveAspectRatio == SvgPreserveAspectRatio::Stretch) {
    float const sx = frame.width / viewBox.width;
    float const sy = frame.height / viewBox.height;
    return Mat3::translate(frame.x - viewBox.x * sx, frame.y - viewBox.y * sy) * Mat3::scale(sx, sy);
  }

  float const sx = frame.width / viewBox.width;
  float const sy = frame.height / viewBox.height;
  float const scale = preserveAspectRatio == SvgPreserveAspectRatio::Slice ? std::max(sx, sy) : std::min(sx, sy);
  float const tx = frame.x + (frame.width - viewBox.width * scale) * 0.5f - viewBox.x * scale;
  float const ty = frame.y + (frame.height - viewBox.height * scale) * 0.5f - viewBox.y * scale;
  return Mat3::translate(tx, ty) * Mat3::scale(scale);
}

template<typename T>
void observeBinding(Reactive::Bindable<T> const& binding) {
  if (binding.isReactive()) {
    (void)binding.evaluate();
  }
}

void observeNode(SvgNode const& node);

void observeGroup(SvgGroup const& group) {
  observeBinding(group.transform);
  observeBinding(group.opacity);
  for (SvgNode const& child : group.children) {
    observeNode(child);
  }
}

void observeNode(SvgNode const& node) {
  std::visit([](auto const& value) {
    using T = std::decay_t<decltype(value)>;
    if constexpr (std::is_same_v<T, SvgPath>) {
      observeBinding(value.d);
      observeBinding(value.fill);
      observeBinding(value.stroke);
      observeBinding(value.opacity);
    } else if constexpr (std::is_same_v<T, SvgRect>) {
      observeBinding(value.x); observeBinding(value.y); observeBinding(value.width); observeBinding(value.height);
      observeBinding(value.cornerRadius); observeBinding(value.fill); observeBinding(value.stroke); observeBinding(value.opacity);
    } else if constexpr (std::is_same_v<T, SvgCircle>) {
      observeBinding(value.cx); observeBinding(value.cy); observeBinding(value.r);
      observeBinding(value.fill); observeBinding(value.stroke); observeBinding(value.opacity);
    } else if constexpr (std::is_same_v<T, SvgEllipse>) {
      observeBinding(value.cx); observeBinding(value.cy); observeBinding(value.rx); observeBinding(value.ry);
      observeBinding(value.fill); observeBinding(value.stroke); observeBinding(value.opacity);
    } else if constexpr (std::is_same_v<T, SvgLine>) {
      observeBinding(value.x1); observeBinding(value.y1); observeBinding(value.x2); observeBinding(value.y2);
      observeBinding(value.stroke); observeBinding(value.opacity);
    } else if constexpr (std::is_same_v<T, SvgPolygon> || std::is_same_v<T, SvgPolyline>) {
      observeBinding(value.fill); observeBinding(value.stroke); observeBinding(value.opacity);
    } else if constexpr (std::is_same_v<T, SvgNode::GroupPtr>) {
      if (value) {
        observeGroup(*value);
      }
    } else if constexpr (std::is_same_v<T, SvgNode::ConditionalPtr>) {
      if (value) {
        observeBinding(value->when);
        for (SvgNode const& child : value->children) {
          observeNode(child);
        }
      }
    }
  }, node.storage);
}

} // namespace

Size Svg::measure(MeasureContext& ctx, LayoutConstraints const& constraints,
                  LayoutHints const&, TextSystem&) const {
  ctx.advanceChildSlot();
  return measureSvg(*this, constraints);
}

std::unique_ptr<scenegraph::SceneNode> Svg::mount(MountContext& ctx) const {
  Size const measured = measureSvg(*this, ctx.constraints());
  Size const size = assignedSize(ctx.constraints(), measured);
  Svg document = *this;
  auto pathCache = std::make_shared<std::unordered_map<std::string, Path>>();
  auto draw = [document, pathCache](Canvas& canvas, Rect frame) mutable {
    if (document.viewBox.width <= 0.f || document.viewBox.height <= 0.f ||
        frame.width <= 0.f || frame.height <= 0.f) {
      return;
    }
    canvas.save();
    canvas.clipRect(frame, CornerRadius{}, true);
    canvas.transform(viewBoxTransform(document.viewBox, frame, document.preserveAspectRatio));
    drawGroup(canvas, document.root, *pathCache);
    canvas.restore();
  };

  auto node = std::make_unique<scenegraph::RenderNode>(
      Rect{0.f, 0.f, size.width, size.height}, draw);
  auto* rawNode = node.get();
  rawNode->setPurity(scenegraph::RenderNode::Purity::Live);
  rawNode->setLayoutConstraints(ctx.constraints());
  rawNode->setRelayout([rawNode, document](LayoutConstraints const& constraints) mutable {
    rawNode->setSize(assignedSize(constraints, measureSvg(document, constraints)));
  });

  Reactive::SmallFn<void()> requestRedraw = ctx.redrawCallback();
  Reactive::withOwner(ctx.owner(), [rawNode, document, requestRedraw = std::move(requestRedraw)]() mutable {
    Reactive::Effect([rawNode, document, requestRedraw, first = true]() mutable {
      observeGroup(document.root);
      if (first) {
        first = false;
        return;
      }
      auto draw = rawNode->draw();
      rawNode->setDraw(std::move(draw));
      if (requestRedraw) {
        requestRedraw();
      }
    });
  });

  return node;
}

SvgGroup translated(float x, float y, std::vector<SvgNode> children) {
  return SvgGroup{.transform = Mat3::translate(x, y), .children = std::move(children)};
}

SvgGroup rotated(float radians, Point pivot, std::vector<SvgNode> children) {
  return SvgGroup{.transform = Mat3::rotate(radians, pivot), .children = std::move(children)};
}

SvgGroup scaled(float sx, float sy, std::vector<SvgNode> children) {
  return SvgGroup{.transform = Mat3::scale(sx, sy), .children = std::move(children)};
}

} // namespace flux
