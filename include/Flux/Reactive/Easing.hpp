#pragma once

#include <functional>

namespace flux {

using EasingFn = float (*)(float);

namespace Easing {

float linear(float t);
float easeIn(float t);
float easeOut(float t);
float easeInOut(float t);

/// Generates a spring easing function. The returned function is stateless
/// and maps [0,1] → [0,1] by numerically integrating a damped spring.
/// Note: spring output may overshoot 1.0 (that is the intended behaviour).
std::function<float(float)> spring(float stiffness = 300.f, float damping = 20.f);

} // namespace Easing
} // namespace flux
