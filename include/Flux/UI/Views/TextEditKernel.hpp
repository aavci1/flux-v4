#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/TextLayout.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace flux {

class TextSystem;
struct Font;
struct Color;
template<typename T>
struct State;

namespace detail {

// UTF-8 navigation
int utf8NextChar(std::string const& s, int pos);
int utf8PrevChar(std::string const& s, int pos);
int utf8Clamp(std::string const& s, int pos);
int utf8PrevWord(std::string const& s, int pos);
int utf8NextWord(std::string const& s, int pos);
int utf8CountChars(std::string const& s);
std::string utf8TruncateToChars(std::string const& s, int maxChars);

// Caret geometry — single-line (used by TextInput)
float caretXPosition(TextSystem&, std::string const&, int byteEnd, Font const&, Color const&);
int caretByteAtTextX(TextSystem&, std::string const&, Font const&, Color const&, float textX);

// Caret geometry — multi-line (used by TextArea)
int caretByteAtPoint(TextSystem&, std::string const&, Font const&, Color const&, TextLayout const&, Point localPoint);

struct LineMetrics {
  float top{};
  float bottom{};
  float baseline{};
  /// Minimum `PlacedRun::origin.x` among runs on this visual line (layout space). Used to map pointer /
  /// caret X to `caretByteAtTextX`, which measures from the line's left ink edge.
  float lineMinX{};
  int byteStart = 0;
  int byteEnd = 0;
};

/// Line metrics are in layout space (same coordinates as `PlacedRun::origin`), i.e. relative to the
/// layout origin passed to `drawTextLayout`, not including the canvas `textOrigin` offset.
std::vector<LineMetrics> buildLineMetrics(std::string const& buf, TextLayout const&, TextSystem& ts, Font const& font,
                                        Color const& color);

int lineIndexForByte(std::vector<LineMetrics> const& lines, int byteOffset, std::string const& buf);

std::pair<int, int> orderedSelection(int caret, int anchor);

void replaceSelection(State<std::string> val, State<int> caretByte, State<int> selAnchor, std::string insert,
                      int maxLength, std::function<void(std::string const&)> const& onChange,
                      State<std::chrono::nanoseconds> lastBlink);

void resetBlink(State<std::chrono::nanoseconds> lastBlink);

struct CaretBlinkTimerSlot {
  std::uint64_t timerId = 0;

  ~CaretBlinkTimerSlot();

  void cancel();
  void set(std::uint64_t id);
  std::uint64_t get() const;
};

void ensureCaretBlinkTimerBridge();

} // namespace detail

} // namespace flux
