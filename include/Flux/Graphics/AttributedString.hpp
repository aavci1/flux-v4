#pragma once

#include <Flux/Core/Types.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace flux {

struct TextAttribute {
  std::string fontFamily; // "" = inherit from base style
  float fontSize = 0.f;   // 0 = inherit
  float fontWeight = 0.f; // 0 = inherit; 400 = regular, 700 = bold
  bool italic = false;
  Color color = Colors::black;
};

struct AttributedRun {
  std::uint32_t start = 0; // byte offset into utf8, inclusive
  std::uint32_t end = 0;   // byte offset, exclusive
  TextAttribute attr;
};

struct AttributedString {
  std::string utf8;
  std::vector<AttributedRun> runs; // sorted by start, non-overlapping

  static AttributedString plain(std::string_view text, TextAttribute const& attr) {
    return {std::string(text),
            {{0, static_cast<std::uint32_t>(text.size()), attr}}};
  }
};

} // namespace flux
