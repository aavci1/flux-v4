#pragma once

/// \file Flux/UI/Alignment.hpp
///
/// Part of the Flux public API.

#include <cstdint>

namespace flux {

/// Shared alignment for layout containers (`VStack`, `HStack`, `Grid`, `ZStack`, …). `Start` is the
/// min axis (leading/top); `End` is the max axis (trailing/bottom). Text layout uses separate
/// \ref HorizontalAlignment / \ref VerticalAlignment.
enum class Alignment : std::uint8_t { Start, Center, End, Stretch };

} // namespace flux
