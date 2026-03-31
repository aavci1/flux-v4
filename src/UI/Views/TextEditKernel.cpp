#include <Flux/Core/Application.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/Events.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Views/TextEditKernel.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

bool utf8DecodeAt(std::string const& s, int i, char32_t& outCp, int& outLen) {
  int const n = static_cast<int>(s.size());
  if (i < 0 || i >= n) {
    return false;
  }
  auto const u = static_cast<unsigned char>(s[static_cast<std::size_t>(i)]);
  if ((u & 0x80) == 0) {
    outCp = u;
    outLen = 1;
    return true;
  }
  if ((u & 0xE0) == 0xC0 && i + 1 < n) {
    auto const u1 = static_cast<unsigned char>(s[static_cast<std::size_t>(i + 1)]);
    if ((u1 & 0xC0) != 0x80) {
      return false;
    }
    outCp = (u & 0x1F) << 6 | (u1 & 0x3F);
    outLen = 2;
    return true;
  }
  if ((u & 0xF0) == 0xE0 && i + 2 < n) {
    auto const u1 = static_cast<unsigned char>(s[static_cast<std::size_t>(i + 1)]);
    auto const u2 = static_cast<unsigned char>(s[static_cast<std::size_t>(i + 2)]);
    if ((u1 & 0xC0) != 0x80 || (u2 & 0xC0) != 0x80) {
      return false;
    }
    outCp = (u & 0x0F) << 12 | (u1 & 0x3F) << 6 | (u2 & 0x3F);
    outLen = 3;
    return true;
  }
  if ((u & 0xF8) == 0xF0 && i + 3 < n) {
    auto const u1 = static_cast<unsigned char>(s[static_cast<std::size_t>(i + 1)]);
    auto const u2 = static_cast<unsigned char>(s[static_cast<std::size_t>(i + 2)]);
    auto const u3 = static_cast<unsigned char>(s[static_cast<std::size_t>(i + 3)]);
    if ((u1 & 0xC0) != 0x80 || (u2 & 0xC0) != 0x80 || (u3 & 0xC0) != 0x80) {
      return false;
    }
    outCp = (u & 0x07) << 18 | (u1 & 0x3F) << 12 | (u2 & 0x3F) << 6 | (u3 & 0x3F);
    outLen = 4;
    return true;
  }
  return false;
}

bool isSpaceChar(char32_t cp) {
  return cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' || cp == 0x3000;
}

bool isWordChar(char32_t cp) {
  if (cp <= 0x7F) {
    return (cp >= '0' && cp <= '9') || (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z') || cp == '_';
  }
  if (isSpaceChar(cp)) {
    return false;
  }
  return true;
}

int utf8NextCharImpl(std::string const& s, int pos) {
  int const n = static_cast<int>(s.size());
  if (pos >= n) {
    return pos;
  }
  int len = 1;
  char32_t cp{};
  if (utf8DecodeAt(s, pos, cp, len)) {
    return pos + len;
  }
  return pos + 1;
}

int utf8PrevCharImpl(std::string const& s, int pos) {
  if (pos <= 0) {
    return 0;
  }
  int p = pos - 1;
  while (p > 0 && (static_cast<unsigned char>(s[static_cast<std::size_t>(p)]) & 0xC0) == 0x80) {
    --p;
  }
  return p;
}

int utf8ClampImpl(std::string const& s, int pos) {
  int const n = static_cast<int>(s.size());
  if (pos <= 0) {
    return 0;
  }
  if (pos >= n) {
    return n;
  }
  int p = pos;
  while (p > 0 && (static_cast<unsigned char>(s[static_cast<std::size_t>(p)]) & 0xC0) == 0x80) {
    --p;
  }
  char32_t cp{};
  int len = 1;
  if (!utf8DecodeAt(s, p, cp, len)) {
    return pos;
  }
  if (p + len < pos) {
    return p + len;
  }
  return p;
}

int utf8PrevWordImpl(std::string const& s, int pos) {
  pos = utf8ClampImpl(s, pos);
  if (pos <= 0) {
    return 0;
  }
  int p = pos;
  while (p > 0) {
    int const prevStart = utf8PrevCharImpl(s, p);
    char32_t cp = 0;
    int len = 1;
    if (!utf8DecodeAt(s, prevStart, cp, len)) {
      p = prevStart;
      continue;
    }
    if (!isSpaceChar(cp)) {
      break;
    }
    p = prevStart;
  }
  while (p > 0) {
    int const prevStart = utf8PrevCharImpl(s, p);
    char32_t cp = 0;
    int len = 1;
    if (!utf8DecodeAt(s, prevStart, cp, len)) {
      p = prevStart;
      continue;
    }
    if (!isWordChar(cp)) {
      break;
    }
    p = prevStart;
  }
  return p;
}

int utf8NextWordImpl(std::string const& s, int pos) {
  int p = utf8ClampImpl(s, pos);
  int const n = static_cast<int>(s.size());
  while (p < n) {
    char32_t cp = 0;
    int len = 1;
    if (!utf8DecodeAt(s, p, cp, len)) {
      p = utf8NextCharImpl(s, p);
      continue;
    }
    if (!isSpaceChar(cp)) {
      break;
    }
    p += len;
  }
  while (p < n) {
    char32_t cp = 0;
    int len = 1;
    if (!utf8DecodeAt(s, p, cp, len)) {
      p = utf8NextCharImpl(s, p);
      continue;
    }
    if (!isWordChar(cp)) {
      break;
    }
    p += len;
  }
  return p;
}

float caretXPositionImpl(flux::TextSystem& ts, std::string const& s, int byteEnd, flux::Font const& font,
                         flux::Color const& color) {
  byteEnd = utf8ClampImpl(s, byteEnd);
  if (byteEnd <= 0) {
    return 0.f;
  }
  std::string_view const prefix(s.data(), static_cast<std::size_t>(byteEnd));
  flux::TextLayoutOptions opts{};
  opts.wrapping = flux::TextWrapping::NoWrap;
  return ts.measure(prefix, font, color, 0.f, opts).width;
}

} // namespace

namespace flux::detail {

int utf8NextChar(std::string const& s, int pos) {
  return utf8NextCharImpl(s, pos);
}

int utf8PrevChar(std::string const& s, int pos) {
  return utf8PrevCharImpl(s, pos);
}

int utf8Clamp(std::string const& s, int pos) {
  return utf8ClampImpl(s, pos);
}

int utf8PrevWord(std::string const& s, int pos) {
  return utf8PrevWordImpl(s, pos);
}

int utf8NextWord(std::string const& s, int pos) {
  return utf8NextWordImpl(s, pos);
}

float caretXPosition(TextSystem& ts, std::string const& s, int byteEnd, Font const& font, Color const& color) {
  return caretXPositionImpl(ts, s, byteEnd, font, color);
}

int caretByteAtTextX(TextSystem& ts, std::string const& s, Font const& font, Color const& color, float textX) {
  int const n = static_cast<int>(s.size());
  if (n == 0) {
    return 0;
  }
  std::vector<int> offs;
  offs.reserve(static_cast<std::size_t>(n) + 2);
  for (int i = 0; i < n;) {
    offs.push_back(i);
    i = utf8NextChar(s, i);
  }
  offs.push_back(n);
  int lo = 0;
  int hi = static_cast<int>(offs.size()) - 1;
  int j = 0;
  while (lo <= hi) {
    int const mid = (lo + hi) / 2;
    float const x = caretXPosition(ts, s, offs[static_cast<std::size_t>(mid)], font, color);
    if (x <= textX) {
      j = mid;
      lo = mid + 1;
    } else {
      hi = mid - 1;
    }
  }
  if (j + 1 < static_cast<int>(offs.size())) {
    float const x0 = caretXPosition(ts, s, offs[static_cast<std::size_t>(j)], font, color);
    float const x1 = caretXPosition(ts, s, offs[static_cast<std::size_t>(j + 1)], font, color);
    if (textX - x0 > x1 - textX) {
      return offs[static_cast<std::size_t>(j) + 1];
    }
  }
  return offs[static_cast<std::size_t>(j)];
}

namespace {

std::vector<std::vector<TextLayout::PlacedRun const*>> groupRunsIntoVisualLines(TextLayout const& layout) {
  std::vector<std::vector<TextLayout::PlacedRun const*>> lines;
  for (auto const& pr : layout.runs) {
    if (lines.empty() || pr.ctLineIndex != lines.back().front()->ctLineIndex) {
      lines.push_back({&pr});
    } else {
      lines.back().push_back(&pr);
    }
  }
  return lines;
}

} // namespace

std::vector<LineMetrics> buildLineMetrics(std::string const& buf, TextLayout const& layout, TextSystem& ts,
                                          Font const& font, Color const& color) {
  std::vector<LineMetrics> out;
  TextLayoutOptions mopts{};
  mopts.wrapping = TextWrapping::NoWrap;
  float const fallbackLineH = ts.measure("Agy", font, color, 0.f, mopts).height;

  auto pushFallbackLine = [&]() {
    LineMetrics lm{};
    lm.top = 0.f;
    lm.bottom = fallbackLineH;
    lm.baseline = fallbackLineH * 0.8f;
    lm.byteStart = 0;
    lm.byteEnd = 0;
    out.push_back(lm);
  };

  if (buf.empty()) {
    pushFallbackLine();
    return out;
  }

  if (layout.runs.empty()) {
    pushFallbackLine();
    return out;
  }

  auto grouped = groupRunsIntoVisualLines(layout);
  out.reserve(grouped.size() + 2);

  for (auto const& grp : grouped) {
    if (grp.empty()) {
      continue;
    }
    float baselineY = -std::numeric_limits<float>::infinity();
    float minTop = std::numeric_limits<float>::infinity();
    float maxBot = -std::numeric_limits<float>::infinity();
    float minOriginX = std::numeric_limits<float>::infinity();
    int b0 = std::numeric_limits<int>::max();
    int b1 = 0;

    for (auto const* pr : grp) {
      baselineY = std::max(baselineY, pr->origin.y);
      minTop = std::min(minTop, pr->origin.y - pr->run.ascent);
      maxBot = std::max(maxBot, pr->origin.y + pr->run.descent);
      minOriginX = std::min(minOriginX, pr->origin.x);
      b0 = std::min(b0, static_cast<int>(pr->utf8Begin));
      b1 = std::max(b1, static_cast<int>(pr->utf8End));
    }

    LineMetrics lm{};
    lm.baseline = baselineY;
    lm.top = minTop;
    lm.bottom = maxBot;
    lm.lineMinX = std::isfinite(minOriginX) ? minOriginX : 0.f;
    lm.byteStart = b0;
    lm.byteEnd = b1;
    out.push_back(lm);
  }

  if (!buf.empty() && buf.back() == '\n') {
    int const n = static_cast<int>(buf.size());
    float const lh = out.size() >= 2 ? (out.back().baseline - out[out.size() - 2].baseline) : fallbackLineH;
    LineMetrics const& last = out.back();
    LineMetrics trail{};
    trail.byteStart = n;
    trail.byteEnd = n;
    trail.lineMinX = 0.f;
    float const ascent = last.baseline - last.top;
    float const descent = last.bottom - last.baseline;
    trail.baseline = last.baseline + lh;
    trail.top = trail.baseline - ascent;
    trail.bottom = trail.baseline + descent;
    out.push_back(trail);
  }

  return out;
}

int lineIndexForByte(std::vector<LineMetrics> const& lines, int byteOffset, std::string const& buf) {
  if (lines.empty()) {
    return 0;
  }
  int const n = static_cast<int>(buf.size());
  int k = utf8Clamp(buf, byteOffset);
  if (k < 0) {
    k = 0;
  }
  if (k > n) {
    k = n;
  }

  for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
    int const a = lines[static_cast<std::size_t>(i)].byteStart;
    int const b = lines[static_cast<std::size_t>(i)].byteEnd;
    if (a == b && a == n && k == n) {
      return i;
    }
    if (a <= k && k < b) {
      return i;
    }
  }
  if (k == n) {
    return static_cast<int>(lines.size()) - 1;
  }
  for (int i = static_cast<int>(lines.size()) - 1; i >= 0; --i) {
    if (k >= lines[static_cast<std::size_t>(i)].byteStart) {
      return i;
    }
  }
  return 0;
}

int caretByteAtPoint(TextSystem& ts, std::string const& buf, Font const& font, Color const& color,
                     TextLayout const& layout, Point localPoint) {
  auto lines = buildLineMetrics(buf, layout, ts, font, color);
  if (lines.empty()) {
    return 0;
  }

  float const y = localPoint.y;
  int bestLine = 0;
  float bestDist = std::numeric_limits<float>::infinity();
  for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
    float const mid = (lines[static_cast<std::size_t>(i)].top + lines[static_cast<std::size_t>(i)].bottom) * 0.5f;
    float const d = std::abs(y - mid);
    if (d < bestDist) {
      bestDist = d;
      bestLine = i;
    }
    if (y >= lines[static_cast<std::size_t>(i)].top && y <= lines[static_cast<std::size_t>(i)].bottom) {
      bestLine = i;
      bestDist = 0.f;
      break;
    }
  }

  LineMetrics const& ln = lines[static_cast<std::size_t>(bestLine)];
  int const a = ln.byteStart;
  int const b = ln.byteEnd;
  if (a >= b) {
    return utf8Clamp(buf, a);
  }
  std::string const slice = buf.substr(static_cast<std::size_t>(a), static_cast<std::size_t>(b - a));
  float const relX = localPoint.x - ln.lineMinX;
  int const rel = caretByteAtTextX(ts, slice, font, color, relX);
  return utf8Clamp(buf, a + rel);
}

namespace {

using namespace std::chrono;

std::unordered_set<std::uint64_t> gCaretBlinkTimerIds;
std::once_flag gCaretBlinkTimerBridgeOnce;

} // namespace

CaretBlinkTimerSlot::~CaretBlinkTimerSlot() {
  cancel();
}

void CaretBlinkTimerSlot::cancel() {
  if (timerId == 0) {
    return;
  }
  gCaretBlinkTimerIds.erase(timerId);
  if (Application::hasInstance()) {
    Application::instance().cancelTimer(timerId);
  }
  timerId = 0;
}

void CaretBlinkTimerSlot::set(std::uint64_t id) {
  if (id == timerId) {
    return;
  }
  cancel();
  timerId = id;
  if (id != 0) {
    gCaretBlinkTimerIds.insert(id);
  }
}

std::uint64_t CaretBlinkTimerSlot::get() const {
  return timerId;
}

void ensureCaretBlinkTimerBridge() {
  std::call_once(gCaretBlinkTimerBridgeOnce, [] {
    Application::instance().eventQueue().on<TimerEvent>([](TimerEvent const& e) {
      if (gCaretBlinkTimerIds.count(e.timerId)) {
        Application::instance().requestRedraw();
      }
    });
  });
}

int utf8CountChars(std::string const& s) {
  int n = 0;
  int i = 0;
  int const len = static_cast<int>(s.size());
  while (i < len) {
    char32_t cp{};
    int L = 1;
    if (!utf8DecodeAt(s, i, cp, L)) {
      ++i;
      ++n;
      continue;
    }
    i += L;
    ++n;
  }
  return n;
}

std::string utf8TruncateToChars(std::string const& s, int maxChars) {
  if (maxChars <= 0) {
    return {};
  }
  int n = 0;
  int i = 0;
  int const len = static_cast<int>(s.size());
  while (i < len && n < maxChars) {
    char32_t cp{};
    int L = 1;
    if (!utf8DecodeAt(s, i, cp, L)) {
      ++i;
      ++n;
      continue;
    }
    i += L;
    ++n;
  }
  return s.substr(0, static_cast<std::size_t>(i));
}

std::pair<int, int> orderedSelection(int caret, int anchor) {
  return {std::min(caret, anchor), std::max(caret, anchor)};
}

void resetBlink(State<std::chrono::nanoseconds> lastBlink) {
  lastBlink = duration_cast<nanoseconds>(steady_clock::now().time_since_epoch());
}

void replaceSelection(State<std::string> val, State<int> caretByte, State<int> selAnchor, std::string insert,
                      int maxLength, std::function<void(std::string const&)> const& onChange,
                      State<std::chrono::nanoseconds> lastBlink) {
  std::string buf = *val;
  auto [i0, i1] = orderedSelection(*caretByte, *selAnchor);
  i0 = utf8Clamp(buf, i0);
  i1 = utf8Clamp(buf, i1);
  if (maxLength > 0) {
    int const before = utf8CountChars(buf.substr(0, static_cast<std::size_t>(i0)));
    int const after = utf8CountChars(buf.substr(static_cast<std::size_t>(i1)));
    int remaining = maxLength - (before + after);
    if (remaining < 0) {
      remaining = 0;
    }
    insert = utf8TruncateToChars(insert, remaining);
  }
  buf.erase(static_cast<std::size_t>(i0), static_cast<std::size_t>(i1 - i0));
  buf.insert(static_cast<std::size_t>(i0), insert);
  int const newPos = i0 + static_cast<int>(insert.size());
  val = std::move(buf);
  caretByte = newPos;
  selAnchor = newPos;
  if (onChange) {
    onChange(*val);
  }
  resetBlink(lastBlink);
}

} // namespace flux::detail
