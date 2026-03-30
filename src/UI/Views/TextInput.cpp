#include <Flux/Core/Application.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/Events.hpp>
#include <Flux/Detail/Runtime.hpp>
#include <Flux/Core/KeyCodes.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/InputFieldLayout.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/TextInput.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <Flux/Graphics/TextLayout.hpp>

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

} // namespace flux::detail

namespace flux {

namespace {

using namespace std::chrono;

std::unordered_set<std::uint64_t> gCaretBlinkTimerIds;
std::once_flag gCaretBlinkTimerBridgeOnce;

/// Owns the repeating caret-blink timer id for one `TextInput` instance. Destructor cancels the timer and
/// removes the id from `gCaretBlinkTimerIds` so orphaned ids are not left when the field is torn down
/// (e.g. window close while focused).
struct CaretBlinkTimerSlot {
  std::uint64_t timerId = 0;

  ~CaretBlinkTimerSlot() { cancel(); }

  void cancel() {
    if (timerId == 0) {
      return;
    }
    gCaretBlinkTimerIds.erase(timerId);
    if (Application::hasInstance()) {
      Application::instance().cancelTimer(timerId);
    }
    timerId = 0;
  }

  void set(std::uint64_t id) {
    if (id == timerId) {
      return;
    }
    cancel();
    timerId = id;
    if (id != 0) {
      gCaretBlinkTimerIds.insert(id);
    }
  }

  std::uint64_t get() const { return timerId; }
};

/// Half-period of the 1.06 s blink in render() — redraw twice per full cycle, no per-frame requestRedraw.
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

constexpr float kSelectionExtraBottomPx = 2.f;

/// Canvas-space top and height of the laid-out ink (ascent + descent), matching `drawTextLayout(..., textOrigin)`.
void lineInkTopAndHeight(TextLayout const& layout, Point textOrigin, float& outTop, float& outHeight) {
  if (layout.runs.empty()) {
    outTop = 0.f;
    outHeight = 0.f;
    return;
  }
  float minTop = std::numeric_limits<float>::infinity();
  float maxBot = -std::numeric_limits<float>::infinity();
  for (auto const& pr : layout.runs) {
    float const t = textOrigin.y + pr.origin.y - pr.run.ascent;
    float const b = textOrigin.y + pr.origin.y + pr.run.descent;
    minTop = std::min(minTop, t);
    maxBot = std::max(maxBot, b);
  }
  outTop = minTop;
  outHeight = std::max(0.f, maxBot - minTop);
}

void reconcileHorizontalScroll(TextSystem& ts, std::string const& buf, Font const& font, Color const& textColor,
                               float caretX, float contentWidth, State<float> scrollOffset) {
  float const textWidth = detail::caretXPosition(ts, buf, static_cast<int>(buf.size()), font, textColor);
  float const cw = std::max(0.f, contentWidth);
  float s = *scrollOffset;
  if (caretX < s) {
    s = caretX;
  }
  if (caretX > s + cw) {
    s = caretX - cw;
  }
  float const maxScroll = std::max(0.f, textWidth - cw);
  s = std::clamp(s, 0.f, maxScroll);
  if (std::abs(s - *scrollOffset) > 1e-3f) {
    scrollOffset = s;
  }
}

void resetBlink(State<std::chrono::nanoseconds> lastBlink) {
  lastBlink = duration_cast<nanoseconds>(steady_clock::now().time_since_epoch());
}

void replaceSelection(State<std::string> val, State<int> caretByte, State<int> selAnchor, std::string insert,
                      int maxLength, std::function<void(std::string const&)> const& onChange,
                      State<std::chrono::nanoseconds> lastBlink) {
  std::string buf = *val;
  auto [i0, i1] = orderedSelection(*caretByte, *selAnchor);
  i0 = detail::utf8Clamp(buf, i0);
  i1 = detail::utf8Clamp(buf, i1);
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

struct TextInputView {
  Size measure(LayoutConstraints const& cs) const {
    float const w = std::isfinite(cs.maxWidth) ? cs.maxWidth : 200.f;
    float const h = resolvedInputFieldHeight(font, textColor, paddingV, height);
    return {std::max(minSize, w), h};
  }

  void render(Canvas& canvas, Rect frame) const;

  State<std::string> value{};
  State<int> caretByte{};
  State<int> selAnchor{};
  State<float> scrollOffset{};
  State<std::chrono::nanoseconds> lastBlinkReset{};

  bool focused = false;
  bool disabled = false;
  std::string placeholder;
  Font font{};
  Color textColor{};
  Color placeholderColor{};
  Color backgroundColor{};
  Color borderColor{};
  Color borderFocusColor{};
  Color caretColor{};
  Color selectionColor{};
  Color disabledColor{};
  float borderWidth = 1.f;
  float borderFocusWidth = 2.f;
  CornerRadius cornerRadius{};
  float height = 0.f;
  float paddingH = 12.f;
  float paddingV = 8.f;
  float minSize = 0.f;

  float cachedCaretX = 0.f;
  float cachedSelX0 = 0.f;
  float cachedSelX1 = 0.f;

  int maxLength = 0;
  std::function<void(std::string const&)> onChange;
  std::function<void(std::string const&)> onSubmit;

  std::function<void(KeyCode, Modifiers)> onKeyDown;
  std::function<void(std::string const&)> onTextInput;
  std::function<void(Point)> onPointerDown;
  std::function<void(Point)> onPointerMove;
  std::function<void(Point)> onPointerUp;

  Cursor cursor = Cursor::Inherit;
  bool focusable = true;
};

void TextInputView::render(Canvas& canvas, Rect frame) const {
  Color const bg = disabled ? disabledColor : backgroundColor;
  Color const bc = disabled ? borderColor : (focused ? borderFocusColor : borderColor);
  float const bw = disabled ? borderWidth : (focused ? borderFocusWidth : borderWidth);
  Color const tc = disabled ? placeholderColor : textColor;

  canvas.drawRect(frame, cornerRadius, FillStyle::solid(bg), StrokeStyle::none());
  canvas.drawRect(frame, cornerRadius, FillStyle::none(), StrokeStyle::solid(bc, bw));

  Rect const content = {frame.x + paddingH, frame.y + paddingV, frame.width - 2.f * paddingH,
                        frame.height - 2.f * paddingV};

  canvas.save();
  canvas.clipRect(content);

  std::string const& buf = *value;
  bool const empty = buf.empty();

  TextSystem& ts = Application::instance().textSystem();
  reconcileHorizontalScroll(ts, buf, font, tc, cachedCaretX, content.width, scrollOffset);

  float const scroll = *scrollOffset;
  TextLayoutOptions opts{};
  opts.wrapping = TextWrapping::NoWrap;
  opts.verticalAlignment = VerticalAlignment::Center;

  Point const textOrigin{content.x - scroll, content.y};
  Color const phc = placeholderColor;
  std::shared_ptr<TextLayout> const layout =
      empty ? ts.layout(placeholder, font, phc, content, opts) : ts.layout(buf, font, tc, content, opts);

  float lineTop = 0.f;
  float lineH = 0.f;
  lineInkTopAndHeight(*layout, textOrigin, lineTop, lineH);
  if (lineH <= 0.f) {
    TextLayoutOptions mopts{};
    mopts.wrapping = TextWrapping::NoWrap;
    Size const fallback = ts.measure("Agy", font, tc, 0.f, mopts);
    lineH = fallback.height;
    lineTop = content.y + (content.height - lineH) * 0.5f;
  }

  auto [i0, i1] = orderedSelection(*caretByte, *selAnchor);
  i0 = detail::utf8Clamp(buf, i0);
  i1 = detail::utf8Clamp(buf, i1);

  if (i0 < i1) {
    float const x0 = content.x + cachedSelX0 - scroll;
    float const x1 = content.x + cachedSelX1 - scroll;
    float const wsel = std::max(0.f, x1 - x0);
    canvas.drawRect(Rect{x0, lineTop, wsel, lineH + kSelectionExtraBottomPx}, CornerRadius{},
                    FillStyle::solid(selectionColor), StrokeStyle::none());
  }

  canvas.drawTextLayout(*layout, textOrigin);

  if (focused && !disabled) {
    auto const t0 = steady_clock::time_point(duration_cast<steady_clock::duration>(*lastBlinkReset));
    double const elapsed = duration<double>(steady_clock::now() - t0).count();
    bool const caretVisible = std::fmod(elapsed, 1.06) < 0.53;
    if (caretVisible) {
      float const cx = content.x + cachedCaretX - scroll;
      canvas.drawLine({cx, lineTop}, {cx, lineTop + lineH}, StrokeStyle::solid(caretColor, 2.0f));
    }
  }

  canvas.restore();
}

} // namespace

Element TextInput::body() const {
  using namespace std::chrono;

  FluxTheme const& theme = useEnvironment<FluxTheme>();
  Font const fnt = resolveFont(font, theme.typeBody.toFont());
  float const padHResolved = resolveFloat(paddingH, theme.paddingFieldH);
  float const padVResolved = resolveFloat(paddingV, theme.paddingFieldV);
  CornerRadius const crResolved{resolveFloat(cornerRadius, theme.radiusMedium)};
  Color const tc = resolveColor(textColor, theme.colorTextPrimary);
  Color const plc = resolveColor(placeholderColor, theme.colorTextPlaceholder);
  Color const bg = resolveColor(backgroundColor, theme.colorSurfaceField);
  Color const bc = resolveColor(borderColor, theme.colorBorder);
  Color const bfc = resolveColor(borderFocusColor, theme.colorBorderFocus);
  Color const cc = resolveColor(caretColor, theme.colorAccent);
  Color const sc = resolveColor(selectionColor, theme.colorAccentSubtle);
  Color const dc = resolveColor(disabledColor, theme.colorSurfaceDisabled);

  State<std::string> val = value;
  auto caretByte = useState(0);
  auto selAnchor = useState(0);
  auto scrollOffset = useState(0.f);
  auto lastBlink = useState(duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()));
  auto mouseDragSelecting = useState(false);
  auto mouseDragAnchorByte = useState(0);
  CaretBlinkTimerSlot& blinkCaretTimer = StateStore::current()->claimSlot<CaretBlinkTimerSlot>();

  std::string const& bufRef = *val;
  int cb = detail::utf8Clamp(bufRef, *caretByte);
  int sa = detail::utf8Clamp(bufRef, *selAnchor);
  if (cb != *caretByte) {
    caretByte = cb;
  }
  if (sa != *selAnchor) {
    selAnchor = sa;
  }

  TextSystem& ts = Application::instance().textSystem();
  std::string const bufCopy = bufRef;

  int const cbKey = *caretByte;
  int const saKey = *selAnchor;
  // Use resolved `fnt` / `tc` — raw `font` / `textColor` may be kFromTheme / kFontFromTheme sentinels.
  float const cachedCaretX = useMemo(
      [&]() { return detail::caretXPosition(ts, bufCopy, cbKey, fnt, tc); }, bufCopy, cbKey, fnt.size, fnt.weight,
      fnt.family, fnt.italic, tc.r, tc.g, tc.b, tc.a);
  float const cachedSelX0 = useMemo(
      [&]() { return detail::caretXPosition(ts, bufCopy, std::min(cbKey, saKey), fnt, tc); }, bufCopy, cbKey, saKey,
      fnt.size, fnt.weight, fnt.family, fnt.italic, tc.r, tc.g, tc.b, tc.a);
  float const cachedSelX1 = useMemo(
      [&]() { return detail::caretXPosition(ts, bufCopy, std::max(cbKey, saKey), fnt, tc); }, bufCopy, cbKey, saKey,
      fnt.size, fnt.weight, fnt.family, fnt.italic, tc.r, tc.g, tc.b, tc.a);

  bool const isDisabled = disabled;
  bool const focused = useFocus();
  bool const pressInSubtree = usePress();
  // Hover routing delivers onPointerMove without PointerDown; mouseDragSelecting gates drag extension.
  // usePress() clears the flag when the primary button releases outside the press target (no PointerUp).
  if (!pressInSubtree && *mouseDragSelecting) {
    mouseDragSelecting = false;
  }

  if (focused && !isDisabled) {
    ensureCaretBlinkTimerBridge();
    if (blinkCaretTimer.get() == 0) {
      std::uint64_t const id = Application::instance().scheduleRepeatingTimer(std::chrono::nanoseconds(530'000'000), 0);
      blinkCaretTimer.set(id);
    }
  } else {
    blinkCaretTimer.cancel();
  }

  std::function<void(std::string const&)> const onCh = onChange;
  std::function<void(std::string const&)> const onSub = onSubmit;
  int const maxLen = maxLength;
  float const padH = padHResolved;
  // buildSlotRect() matches layout after the previous pass; first build can yield 0 — click-to-caret fixes next frame.
  float const frameX = [] {
    Runtime* const rt = Runtime::current();
    return rt ? rt->buildSlotRect().x : 0.f;
  }();

  useViewAction(
      "edit.copy",
      [val, caretByte, selAnchor]() {
        auto [a, b] = orderedSelection(*caretByte, *selAnchor);
        if (a < b) {
          Application::instance().clipboard().writeText(
              (*val).substr(static_cast<std::size_t>(a), static_cast<std::size_t>(b - a)));
        }
      },
      [caretByte, selAnchor, isDisabled]() {
        return !isDisabled && *caretByte != *selAnchor;
      });

  useViewAction(
      "edit.cut",
      [val, caretByte, selAnchor, onCh, lastBlink, isDisabled]() {
        if (isDisabled) {
          return;
        }
        auto [a, b] = orderedSelection(*caretByte, *selAnchor);
        if (a >= b) {
          return;
        }
        Application::instance().clipboard().writeText(
            (*val).substr(static_cast<std::size_t>(a), static_cast<std::size_t>(b - a)));
        std::string t = *val;
        t.erase(static_cast<std::size_t>(a), static_cast<std::size_t>(b - a));
        val = std::move(t);
        caretByte = a;
        selAnchor = a;
        if (onCh) {
          onCh(*val);
        }
        resetBlink(lastBlink);
      },
      [caretByte, selAnchor, isDisabled]() {
        return !isDisabled && *caretByte != *selAnchor;
      });

  useViewAction(
      "edit.paste",
      [val, caretByte, selAnchor, onCh, maxLen, lastBlink, isDisabled]() {
        if (isDisabled) {
          return;
        }
        if (auto s = Application::instance().clipboard().readText()) {
          replaceSelection(val, caretByte, selAnchor, std::move(*s), maxLen, onCh, lastBlink);
        }
      },
      [isDisabled]() { return !isDisabled && Application::instance().clipboard().hasText(); });

  useViewAction(
      "edit.selectAll",
      [val, caretByte, selAnchor, isDisabled]() {
        if (isDisabled) {
          return;
        }
        selAnchor = 0;
        caretByte = static_cast<int>((*val).size());
      },
      [isDisabled]() { return !isDisabled; });

  auto onText = [val, caretByte, selAnchor, onCh, maxLen, lastBlink, isDisabled](std::string const& chunk) {
    if (isDisabled || chunk.empty()) {
      return;
    }
    replaceSelection(val, caretByte, selAnchor, chunk, maxLen, onCh, lastBlink);
  };

  auto onKey = [val, caretByte, selAnchor, onCh, onSub, lastBlink, isDisabled](KeyCode k, Modifiers m) {
    if (isDisabled) {
      return;
    }
    bool const shift = any(m & Modifiers::Shift);
    bool const alt = any(m & Modifiers::Alt);
    bool const meta = any(m & Modifiers::Meta);

    std::string cur = *val;
    int const n = static_cast<int>(cur.size());
    auto [i0, i1] = orderedSelection(*caretByte, *selAnchor);

    auto touchCaret = [&]() { resetBlink(lastBlink); };

    if (k == keys::Return) {
      if (onSub) {
        onSub(*val);
      }
      return;
    }

    if (k == keys::LeftArrow) {
      if (alt && !meta) {
        int const target = detail::utf8PrevWord(cur, *caretByte);
        if (!shift) {
          caretByte = target;
          selAnchor = target;
        } else {
          if (i0 == i1) {
            selAnchor = *caretByte;
          }
          caretByte = target;
        }
        touchCaret();
        return;
      }
      if (meta) {
        if (!shift) {
          caretByte = 0;
          selAnchor = 0;
        } else {
          if (i0 == i1) {
            selAnchor = *caretByte;
          }
          caretByte = 0;
        }
        touchCaret();
        return;
      }
      if (!shift) {
        if (i0 < i1) {
          caretByte = i0;
          selAnchor = i0;
        } else {
          int const np = detail::utf8PrevChar(cur, i0);
          caretByte = np;
          selAnchor = np;
        }
      } else {
        if (i0 == i1) {
          selAnchor = *caretByte;
        }
        caretByte = detail::utf8PrevChar(cur, *caretByte);
      }
      touchCaret();
      return;
    }

    if (k == keys::RightArrow) {
      if (alt && !meta) {
        int const target = detail::utf8NextWord(cur, *caretByte);
        if (!shift) {
          caretByte = target;
          selAnchor = target;
        } else {
          if (i0 == i1) {
            selAnchor = *caretByte;
          }
          caretByte = target;
        }
        touchCaret();
        return;
      }
      if (meta) {
        if (!shift) {
          caretByte = n;
          selAnchor = n;
        } else {
          if (i0 == i1) {
            selAnchor = *caretByte;
          }
          caretByte = n;
        }
        touchCaret();
        return;
      }
      if (!shift) {
        if (i0 < i1) {
          caretByte = i1;
          selAnchor = i1;
        } else {
          int const np = detail::utf8NextChar(cur, i0);
          caretByte = np;
          selAnchor = np;
        }
      } else {
        if (i0 == i1) {
          selAnchor = *caretByte;
        }
        caretByte = detail::utf8NextChar(cur, *caretByte);
      }
      touchCaret();
      return;
    }

    if (k == keys::Home) {
      if (!shift) {
        caretByte = 0;
        selAnchor = 0;
      } else {
        if (i0 == i1) {
          selAnchor = *caretByte;
        }
        caretByte = 0;
      }
      touchCaret();
      return;
    }

    if (k == keys::End) {
      if (!shift) {
        caretByte = n;
        selAnchor = n;
      } else {
        if (i0 == i1) {
          selAnchor = *caretByte;
        }
        caretByte = n;
      }
      touchCaret();
      return;
    }

    if (k == keys::Delete) {
      if (alt) {
        int const from = detail::utf8PrevWord(cur, *caretByte);
        if (from < *caretByte) {
          cur.erase(static_cast<std::size_t>(from), static_cast<std::size_t>(*caretByte - from));
          val = std::move(cur);
          caretByte = from;
          selAnchor = from;
          if (onCh) {
            onCh(*val);
          }
          touchCaret();
        }
        return;
      }
      if (i0 < i1) {
        cur.erase(static_cast<std::size_t>(i0), static_cast<std::size_t>(i1 - i0));
        val = std::move(cur);
        caretByte = i0;
        selAnchor = i0;
        if (onCh) {
          onCh(*val);
        }
        touchCaret();
        return;
      }
      if (i0 > 0) {
        int const prev = detail::utf8PrevChar(cur, i0);
        cur.erase(static_cast<std::size_t>(prev), static_cast<std::size_t>(i0 - prev));
        val = std::move(cur);
        caretByte = prev;
        selAnchor = prev;
        if (onCh) {
          onCh(*val);
        }
        touchCaret();
      }
      return;
    }

    if (k == keys::ForwardDelete) {
      if (i0 < i1) {
        cur.erase(static_cast<std::size_t>(i0), static_cast<std::size_t>(i1 - i0));
        val = std::move(cur);
        caretByte = i0;
        selAnchor = i0;
        if (onCh) {
          onCh(*val);
        }
        touchCaret();
        return;
      }
      if (i0 < n) {
        int const next = detail::utf8NextChar(cur, i0);
        cur.erase(static_cast<std::size_t>(i0), static_cast<std::size_t>(next - i0));
        val = std::move(cur);
        caretByte = i0;
        selAnchor = i0;
        if (onCh) {
          onCh(*val);
        }
        touchCaret();
      }
      return;
    }
  };

  auto onPointerDown = [val, caretByte, selAnchor, scrollOffset, lastBlink, mouseDragSelecting, mouseDragAnchorByte,
                          frameX, padH, fnt, tc, isDisabled](Point local) {
    if (isDisabled) {
      return;
    }
    mouseDragSelecting = true;
    std::string const& buf = *val;
    TextSystem& ts = Application::instance().textSystem();
    float const scroll = *scrollOffset;
    float const textX = local.x - frameX - padH + scroll;
    if (buf.empty()) {
      mouseDragAnchorByte = 0;
      caretByte = 0;
      selAnchor = 0;
      resetBlink(lastBlink);
      return;
    }
    int const pos = detail::caretByteAtTextX(ts, buf, fnt, tc, textX);
    mouseDragAnchorByte = pos;
    caretByte = pos;
    selAnchor = pos;
    resetBlink(lastBlink);
  };

  auto onPointerMove = [val, caretByte, selAnchor, scrollOffset, lastBlink, mouseDragSelecting, mouseDragAnchorByte,
                          frameX, padH, fnt, tc, isDisabled](Point local) {
    if (isDisabled || !*mouseDragSelecting) {
      return;
    }
    std::string const& buf = *val;
    TextSystem& ts = Application::instance().textSystem();
    float const scroll = *scrollOffset;
    float const textX = local.x - frameX - padH + scroll;
    int const anchor = *mouseDragAnchorByte;
    if (buf.empty()) {
      caretByte = 0;
      selAnchor = anchor;
      resetBlink(lastBlink);
      return;
    }
    int const pos = detail::caretByteAtTextX(ts, buf, fnt, tc, textX);
    caretByte = pos;
    selAnchor = anchor;
    resetBlink(lastBlink);
  };

  auto onPointerUp = [mouseDragSelecting](Point) { mouseDragSelecting = false; };

  return Element{TextInputView{
      .value = val,
      .caretByte = caretByte,
      .selAnchor = selAnchor,
      .scrollOffset = scrollOffset,
      .lastBlinkReset = lastBlink,
      .focused = focused,
      .disabled = isDisabled,
      .placeholder = placeholder,
      .font = fnt,
      .textColor = tc,
      .placeholderColor = plc,
      .backgroundColor = bg,
      .borderColor = bc,
      .borderFocusColor = bfc,
      .caretColor = cc,
      .selectionColor = sc,
      .disabledColor = dc,
      .borderWidth = borderWidth,
      .borderFocusWidth = borderFocusWidth,
      .cornerRadius = crResolved,
      .height = height,
      .paddingH = padHResolved,
      .paddingV = padVResolved,
      .minSize = minSize,
      .cachedCaretX = cachedCaretX,
      .cachedSelX0 = cachedSelX0,
      .cachedSelX1 = cachedSelX1,
      .maxLength = maxLength,
      .onChange = onChange,
      .onSubmit = onSubmit,
      .onKeyDown = std::move(onKey),
      .onTextInput = std::move(onText),
      .onPointerDown = std::move(onPointerDown),
      .onPointerMove = std::move(onPointerMove),
      .onPointerUp = std::move(onPointerUp),
      .cursor = isDisabled ? Cursor::Inherit : Cursor::IBeam,
      .focusable = !isDisabled,
  }};
}

} // namespace flux
