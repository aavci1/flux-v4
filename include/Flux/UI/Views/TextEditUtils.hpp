#pragma once

/// \file Flux/UI/Views/TextEditUtils.hpp
///
/// Pure, stateless UTF-8 and text-layout helpers for text editing.

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/TextLayout.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace flux {

namespace detail {

/// Default caret stroke width (logical px) for single-line and multiline text fields.
inline constexpr float kTextCaretStrokeWidthPx = 2.f;

// UTF-8 navigation
int utf8NextChar(std::string const& s, int pos) noexcept;
int utf8PrevChar(std::string const& s, int pos) noexcept;
int utf8Clamp(std::string const& s, int pos) noexcept;
int utf8PrevWord(std::string const& s, int pos) noexcept;
int utf8NextWord(std::string const& s, int pos) noexcept;
int utf8CountChars(std::string const& s) noexcept;
std::string utf8TruncateToChars(std::string const& s, int maxChars);

/// Returns true if inserting `inserted` at `pos` into `prev` should coalesce with the previous typing
/// group for undo purposes.
bool shouldCoalesceInsert(std::string const& prev, int pos, std::string_view inserted) noexcept;

struct LineMetrics {
  float top{};
  float bottom{};
  float baseline{};
  float lineMinX{};
  int byteStart = 0;
  int byteEnd = 0;
  std::uint32_t ctLineIndex = 0;
};

std::vector<LineMetrics> buildLineMetrics(TextLayout const& layout);

/// Binary search over sorted `byteStart`. Returns index of the line containing `byteOffset`, clamped.
int lineIndexForByte(std::vector<LineMetrics> const& lines, int byteOffset) noexcept;

float caretXForByte(TextLayout const& layout, LineMetrics const& line, int byteOffset) noexcept;

/// Vertical extent for drawing a caret on \p line (layout Y, same as `LineMetrics::top/bottom`).
/// Computed from runs on that CT line (`min(origin.y - ascent)`, `max(origin.y + descent)`), matching
/// `drawTextLayout` / Core Text line boxes. When the line has no runs (empty line), falls back to
/// `LineMetrics` (extending the box to the layout’s max typographic line height when needed) or
/// baseline ± max ascent/descent from any run in the layout.
std::pair<float, float> lineCaretYRangeInLayout(TextLayout const& layout, LineMetrics const& line) noexcept;

int caretByteAtX(TextLayout const& layout, LineMetrics const& line, float layoutX, std::string const& buf) noexcept;

std::pair<int, int> orderedSelection(int caret, int anchor) noexcept;

} // namespace detail

} // namespace flux
