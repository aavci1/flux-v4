#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Styles.hpp>

namespace flux::scene {

bool rectEmpty(Rect const& rect) noexcept;
Rect unionRect(Rect lhs, Rect rhs) noexcept;
Rect offsetRect(Rect rect, Point delta) noexcept;
Rect intersectRects(Rect lhs, Rect rhs) noexcept;
Rect expandForStrokeAndShadow(Rect rect, StrokeStyle const& stroke, ShadowStyle const& shadow) noexcept;
Rect transformBounds(Mat3 const& transform, Rect const& rect) noexcept;

} // namespace flux::scene
