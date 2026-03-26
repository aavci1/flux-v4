#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace flux {

struct Size {
  float width = 0;
  float height = 0;

  constexpr Size(float width = 0, float height = 0) : width(width), height(height) {}

  constexpr bool isEmpty() const { return width <= 0 || height <= 0; }

  constexpr bool operator==(const Size& other) const = default;
};

struct Vec2 {
  float x = 0;
  float y = 0;

  constexpr Vec2() = default;
  constexpr Vec2(float x, float y) : x(x), y(y) {}

  constexpr bool operator==(const Vec2& o) const = default;
};

using u64 = std::uint64_t;

using String = std::string;

using Clock = std::chrono::steady_clock;
using Instant = Clock::time_point;
using Duration = Clock::duration;

enum class MouseButton : std::uint8_t { None, Left, Right, Middle, Other };

/// Platform-specific virtual key / HID usage code; v1 stores raw macOS key codes where needed.
using KeyCode = std::uint16_t;

enum class Modifiers : std::uint32_t {
  None = 0,
  Shift = 1 << 0,
  Ctrl = 1 << 1,
  Alt = 1 << 2,
  Meta = 1 << 3,
};

constexpr Modifiers operator|(Modifiers a, Modifiers b) {
  return static_cast<Modifiers>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

constexpr Modifiers operator&(Modifiers a, Modifiers b) {
  return static_cast<Modifiers>(static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
}

constexpr bool any(Modifiers m) { return static_cast<std::uint32_t>(m) != 0; }

} // namespace flux
