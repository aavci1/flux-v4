#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Font.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace flux {

struct AttributedRun {
  std::uint32_t start = 0; // byte offset into utf8, inclusive
  std::uint32_t end = 0;   // byte offset, exclusive
  Font font{};
  Color color = Colors::black;
};

struct AttributedString {
  std::string utf8;
  std::vector<AttributedRun> runs; // sorted by start, non-overlapping

  static AttributedString plain(std::string_view text, Font const& font, Color const& color) {
    return {std::string(text),
            {{0, static_cast<std::uint32_t>(text.size()), font, color}}};
  }
};

} // namespace flux
