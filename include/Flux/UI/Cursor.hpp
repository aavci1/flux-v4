#pragma once

/// \file Flux/UI/Cursor.hpp
///
/// Public UI cursor value type.

#include <cstdint>

namespace flux {

enum class Cursor : std::uint8_t {
  Inherit,
  Arrow,
  IBeam,
  Hand,
  ResizeEW,
  ResizeNS,
  ResizeNESW,
  ResizeNWSE,
  ResizeAll,
  Crosshair,
  NotAllowed,
};

} // namespace flux
