#include <Flux/UI/Views/Image.hpp>

#include <Flux/SceneGraph/ImageNode.hpp>
#include <Flux/UI/MeasureContext.hpp>
#include <Flux/UI/MountContext.hpp>

#include <algorithm>
#include <cmath>
#include <memory>

namespace flux::views {

namespace {

float finiteDimension(float value, float fallback) {
  return std::isfinite(value) ? std::max(0.f, value) : fallback;
}

Size naturalSize(std::shared_ptr<flux::Image> const& image) {
  if (!image) {
    return Size{};
  }
  Size size = image->size();
  size.width = std::max(0.f, size.width);
  size.height = std::max(0.f, size.height);
  return size;
}

Size resolveFrame(Size natural, LayoutConstraints const& constraints) {
  Size size = natural;
  if (size.width <= 0.f && std::isfinite(constraints.maxWidth)) {
    size.width = constraints.maxWidth;
  }
  if (size.height <= 0.f && std::isfinite(constraints.maxHeight)) {
    size.height = constraints.maxHeight;
  }
  if (std::isfinite(constraints.maxWidth) && constraints.maxWidth > 0.f &&
      std::isfinite(constraints.maxHeight) && constraints.maxHeight > 0.f) {
    size.width = constraints.maxWidth;
  }

  if (std::isfinite(constraints.maxWidth)) {
    size.width = std::min(size.width, constraints.maxWidth);
  }
  if (std::isfinite(constraints.maxHeight)) {
    size.height = std::min(size.height, constraints.maxHeight);
  }
  size.width = std::max(size.width, constraints.minWidth);
  size.height = std::max(size.height, constraints.minHeight);
  return Size{finiteDimension(size.width, constraints.minWidth),
              finiteDimension(size.height, constraints.minHeight)};
}

} // namespace

Size Image::measure(MeasureContext& ctx, LayoutConstraints const& constraints,
                    LayoutHints const&, TextSystem&) const {
  ctx.advanceChildSlot();
  return resolveFrame(naturalSize(source), constraints);
}

std::unique_ptr<scenegraph::SceneNode> Image::mount(MountContext& ctx) const {
  Size const frameSize = resolveFrame(naturalSize(source), ctx.constraints());
  return std::make_unique<scenegraph::ImageNode>(
      Rect{0.f, 0.f, frameSize.width, frameSize.height}, source, fillMode);
}

} // namespace flux::views
