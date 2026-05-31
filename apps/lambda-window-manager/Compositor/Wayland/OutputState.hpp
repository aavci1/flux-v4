#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace lambda::compositor {

inline constexpr std::uint32_t kOutputVersion = 4;

[[nodiscard]] inline std::uint32_t outputResourceVersion(std::uint32_t boundVersion) {
  return std::min(boundVersion, kOutputVersion);
}

[[nodiscard]] inline std::int32_t outputIntegerScale(float scale) {
  if (!std::isfinite(scale)) return 1;
  return std::max(1, static_cast<std::int32_t>(std::ceil(scale)));
}

} // namespace lambda::compositor
