#include <Flux/Core/Application.hpp>
#include <Flux/Core/KeyCodes.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/Detail/Runtime.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/InputFieldChrome.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/TextArea.hpp>
#include <Flux/UI/Views/TextEditKernel.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include <Flux/Graphics/TextLayout.hpp>

namespace flux {

namespace {

TextArea::Style resolveTextAreaStyle(TextArea::Style const& style, Theme const& theme) {
  InputFieldChromeSpec const spec{
      .textColor = style.textColor,
      .placeholderColor = style.placeholderColor,
      .backgroundColor = style.backgroundColor,
      .borderColor = style.borderColor,
      .borderFocusColor = style.borderFocusColor,
      .caretColor = style.caretColor,
      .selectionColor = style.selectionColor,
      .disabledColor = style.disabledColor,
      .borderWidth = style.borderWidth,
      .borderFocusWidth = style.borderFocusWidth,
      .cornerRadius = style.cornerRadius,
      .paddingH = style.paddingH,
      .paddingV = style.paddingV,
  };
  ResolvedInputFieldChrome const c = resolveInputFieldChrome(spec, theme);
  return TextArea::Style{
      .font = resolveFont(style.font, theme.typeBody.toFont()),
      .textColor = c.textColor,
      .placeholderColor = c.placeholderColor,
      .backgroundColor = c.backgroundColor,
      .borderColor = c.borderColor,
      .borderFocusColor = c.borderFocusColor,
      .caretColor = c.caretColor,
      .selectionColor = c.selectionColor,
      .disabledColor = c.disabledColor,
      .borderWidth = c.borderWidth,
      .borderFocusWidth = c.borderFocusWidth,
      .cornerRadius = c.cornerRadius,
      .paddingH = c.paddingH,
      .paddingV = c.paddingV,
      .lineHeight = style.lineHeight,
  };
}

} // namespace

namespace detail {
// Implemented in TextEditKernel.cpp (not in kernel header — TextArea-only consumer).
int caretByteAtPoint(TextSystem&, std::string const&, Font const&, Color const&, TextLayout const&, Point);
}

namespace {

using namespace std::chrono;

constexpr float kSelectionExtraBottomPx = 2.f;

/// Cached caret position in layout space (same coordinates as `LineMetrics` / `TextLayout::lines`).
struct CaretGeom {
  float x = 0.f;
  float y = 0.f;
  float h = 0.f;
};

int lineEndBeforeTrailingNewlines(std::string const& buf, detail::LineMetrics const& lm) {
  int e = lm.byteEnd;
  while (e > lm.byteStart && buf[static_cast<std::size_t>(e - 1)] == '\n') {
    --e;
  }
  return std::max(lm.byteStart, e);
}

/// Layout-space X of the caret ink at `byte` (same box as key handling / render).
float caretLayoutXForByte(std::string const& cur, int byte, float fieldW, float fieldH, float padH, float padV,
                          float lineHeightOpt, Font const& fnt, Color const& tc, Color const& plc,
                          std::string const& placeholderText) {
  TextSystem& ts = Application::instance().textSystem();
  TextLayoutOptions opts{};
  opts.wrapping = TextWrapping::Wrap;
  opts.verticalAlignment = VerticalAlignment::Top;
  opts.lineHeight = lineHeightOpt;
  float const cw = std::max(0.f, fieldW - 2.f * padH);
  float const ch = std::max(0.f, fieldH - 2.f * padV);
  Rect const box{0.f, 0.f, cw, ch};
  bool const empty = cur.empty();
  std::shared_ptr<TextLayout> const layout =
      empty ? ts.layout(placeholderText, fnt, plc, box, opts) : ts.layout(cur, fnt, tc, box, opts);
  std::string const& text = empty ? placeholderText : cur;
  Color const col = empty ? plc : tc;
  auto lines = detail::buildLineMetrics(text, *layout, ts, fnt, col);
  int const b = detail::utf8Clamp(text, byte);
  int const li = detail::lineIndexForByte(lines, b, text);
  if (li < 0 || li >= static_cast<int>(lines.size())) {
    return 0.f;
  }
  detail::LineMetrics const& le = lines[static_cast<std::size_t>(li)];
  std::string const slice = text.substr(static_cast<std::size_t>(le.byteStart),
                                        static_cast<std::size_t>(std::max(0, le.byteEnd - le.byteStart)));
  int const rel = b - le.byteStart;
  return detail::caretXPosition(ts, slice, std::max(0, rel), fnt, col) + le.lineMinX;
}

struct TextAreaView {
  Size measure(LayoutConstraints const& cs, LayoutHints const&) const {
    float const contentW = std::isfinite(cs.maxWidth) ? cs.maxWidth : 200.f;
    TextSystem& ts = Application::instance().textSystem();
    TextLayoutOptions opts{};
    opts.wrapping = TextWrapping::Wrap;
    opts.verticalAlignment = VerticalAlignment::Top;
    opts.lineHeight = lineHeight;
    std::string const& text = (*value).empty() ? placeholder : *value;
    Color const col = (*value).empty() ? placeholderColor : textColor;
    float wrappedH = ts.measure(text, font, col, contentW, opts).height + 2.f * paddingV;
    float intrinsic = wrappedH;
    intrinsic = std::max(intrinsic, height.minIntrinsic);
    if (height.maxIntrinsic > 0.f) {
      intrinsic = std::min(intrinsic, height.maxIntrinsic);
    }
    float const h = height.fixed > 0.f ? height.fixed : intrinsic;
    return {contentW, h};
  }

  void render(Canvas& canvas, Rect frame) const;

  State<std::string> value{};
  State<int> caretByte{};
  State<int> selAnchor{};
  State<float> scrollOffsetY{};
  State<std::chrono::nanoseconds> lastBlinkReset{};

  bool focused = false;
  bool disabled = false;
  std::string placeholder;
  Font font{};
  Color textColor{};
  Color placeholderColor{};
  Color borderColor{};
  Color borderFocusColor{};
  Color caretColor{};
  Color selectionColor{};
  Color disabledColor{};
  float borderWidth = 1.f;
  float borderFocusWidth = 2.f;
  FillStyle bgFillResolved = FillStyle::solid(Color::rgb(255, 255, 255));
  StrokeStyle strokeNormalResolved = StrokeStyle::none();
  StrokeStyle strokeFocusResolved = StrokeStyle::none();
  CornerRadius cornerRadiusResolved{};
  TextAreaHeight height{};
  float paddingH = 12.f;
  float paddingV = 8.f;
  float lineHeight = 0.f;

  float cachedCaretX = 0.f;
  float cachedCaretY = 0.f;
  float cachedCaretH = 0.f;

  int maxLength = 0;
  std::function<void(std::string const&)> onChange;
  std::function<void(std::string const&)> onEscape;

  std::function<void(KeyCode, Modifiers)> onKeyDown;
  std::function<void(std::string const&)> onTextInput;
  std::function<void(Point)> onPointerDown;
  std::function<void(Point)> onPointerMove;
  std::function<void(Point)> onPointerUp;
  std::function<void(Vec2)> onScroll;

  Cursor cursor = Cursor::Inherit;
  bool focusable = true;
};

void TextAreaView::render(Canvas& canvas, Rect frame) const {
  FillStyle const fill = disabled ? FillStyle::solid(disabledColor) : bgFillResolved;
  StrokeStyle const stroke =
      disabled ? StrokeStyle::solid(borderColor, borderWidth)
               : (focused ? strokeFocusResolved : strokeNormalResolved);
  Color const tc = disabled ? placeholderColor : textColor;

  canvas.drawRect(frame, cornerRadiusResolved, fill, StrokeStyle::none());
  canvas.drawRect(frame, cornerRadiusResolved, FillStyle::none(), stroke);

  Rect const content = {frame.x + paddingH, frame.y + paddingV, frame.width - 2.f * paddingH,
                        frame.height - 2.f * paddingV};

  canvas.save();
  canvas.clipRect(content);

  std::string const& buf = *value;
  bool const empty = buf.empty();

  TextSystem& ts = Application::instance().textSystem();
  TextLayoutOptions opts{};
  opts.wrapping = TextWrapping::Wrap;
  opts.verticalAlignment = VerticalAlignment::Top;
  opts.lineHeight = lineHeight;

  Color const phc = placeholderColor;
  std::shared_ptr<TextLayout> const layout =
      empty ? ts.layout(placeholder, font, phc, content, opts) : ts.layout(buf, font, tc, content, opts);

  std::string const& textForMetrics = empty ? placeholder : buf;
  Color const colForMetrics = empty ? phc : tc;
  // Line metrics are layout-space (`LineMetrics` matches `TextLayout::lines`); scroll only shifts the draw origin.
  std::vector<detail::LineMetrics> const lines =
      detail::buildLineMetrics(textForMetrics, *layout, ts, font, colForMetrics);

  float const scroll = *scrollOffsetY;
  int const cb = detail::utf8Clamp(textForMetrics, *caretByte);
  int const li = detail::lineIndexForByte(lines, cb, textForMetrics);
  if (li >= 0 && li < static_cast<int>(lines.size())) {
    detail::LineMetrics const& le = lines[static_cast<std::size_t>(li)];
    float const caretTop = le.top;
    float const caretBottom = le.bottom;
    float const viewTop = scroll;
    float const viewBottom = scroll + content.height;
    float const contentH = layout->measuredSize.height;
    float s = scroll;
    if (caretTop < viewTop) {
      s = caretTop;
    }
    if (caretBottom > viewBottom) {
      s = caretBottom - content.height;
    }
    s = std::clamp(s, 0.f, std::max(0.f, contentH - content.height));
    if (std::abs(s - scroll) > 1e-3f) {
      scrollOffsetY = s;
    }
  }

  float const scroll2 = *scrollOffsetY;
  Point const textOrigin2{content.x, content.y - scroll2};

  std::string const& textForSel = empty ? placeholder : buf;
  auto [i0, i1] = detail::orderedSelection(*caretByte, *selAnchor);
  i0 = detail::utf8Clamp(textForSel, i0);
  i1 = detail::utf8Clamp(textForSel, i1);

  if (i0 < i1) {
    for (detail::LineMetrics const& lm : lines) {
      int const lineA = lm.byteStart;
      int const lineB = lm.byteEnd;
      int const seg0 = std::max(i0, lineA);
      int const seg1 = std::min(i1, lineB);
      if (seg0 >= seg1) {
        continue;
      }
      if (lineB <= lineA) {
        continue;
      }
      std::string const lineText = textForSel.substr(static_cast<std::size_t>(lineA),
                                                     static_cast<std::size_t>(lineB - lineA));
      int const rel0 = seg0 - lineA;
      int const rel1 = seg1 - lineA;
      float const x0 =
          content.x + lm.lineMinX + detail::caretXPosition(ts, lineText, rel0, font, empty ? phc : tc);
      float const x1 =
          content.x + lm.lineMinX + detail::caretXPosition(ts, lineText, rel1, font, empty ? phc : tc);
      float const wsel = std::max(0.f, x1 - x0);
      float const yTop = textOrigin2.y + lm.top;
      float const yBot = textOrigin2.y + lm.bottom;
      canvas.drawRect(Rect{x0, yTop, wsel, yBot - yTop + kSelectionExtraBottomPx}, CornerRadius{},
                      FillStyle::solid(selectionColor), StrokeStyle::none());
    }
  }

  canvas.drawTextLayout(*layout, textOrigin2);

  if (focused && !disabled) {
    auto const t0 = steady_clock::time_point(duration_cast<steady_clock::duration>(*lastBlinkReset));
    double const elapsed = duration<double>(steady_clock::now() - t0).count();
    bool const caretVisible = std::fmod(elapsed, 1.06) < 0.53;
    if (caretVisible) {
      float const cx = content.x + cachedCaretX;
      float const cy = textOrigin2.y + cachedCaretY;
      canvas.drawLine({cx, cy}, {cx, cy + cachedCaretH}, StrokeStyle::solid(caretColor, 2.0f));
    }
  }

  canvas.restore();
}

} // namespace

Element TextArea::body() const {
  using namespace std::chrono;

  Theme const& theme = useEnvironment<Theme>();
  TextArea::Style const s = resolveTextAreaStyle(style, theme);
  Font const fnt = s.font;
  float const padHResolved = s.paddingH;
  float const padVResolved = s.paddingV;
  Color const tc = s.textColor;
  Color const plc = s.placeholderColor;
  Color const bc = s.borderColor;
  Color const bfc = s.borderFocusColor;
  Color const cc = s.caretColor;
  Color const sc = s.selectionColor;
  Color const dc = s.disabledColor;

  ElementModifiers const* const outerMods = useOuterElementModifiers();
  ResolvedInputFieldChrome const chrome{.textColor = tc,
                                        .placeholderColor = plc,
                                        .backgroundColor = s.backgroundColor,
                                        .borderColor = bc,
                                        .borderFocusColor = bfc,
                                        .caretColor = cc,
                                        .selectionColor = sc,
                                        .disabledColor = dc,
                                        .borderWidth = s.borderWidth,
                                        .borderFocusWidth = s.borderFocusWidth,
                                        .cornerRadius = s.cornerRadius,
                                        .paddingH = padHResolved,
                                        .paddingV = padVResolved};
  InputFieldDecoration const deco = applyOuterInputFieldDecoration(chrome, outerMods);
  FillStyle bgFill = deco.bgFill;
  StrokeStyle strokeN = deco.strokeNormal;
  StrokeStyle strokeF = deco.strokeFocus;
  CornerRadius cr = deco.cornerRadius;

  float const lineHeightOpt = s.lineHeight;
  std::string const placeholderText = placeholder;

  State<std::string> val = value;
  auto caretByte = useState(0);
  auto selAnchor = useState(0);
  auto scrollOffsetY = useState(0.f);
  auto lastBlink = useState(duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()));
  auto mouseDragSelecting = useState(false);
  auto mouseDragAnchorByte = useState(0);
  /// Preferred layout-space X for ↑/↓ (macOS-style column); `active` false until first vertical step uses line X.
  auto stickyCaretLayoutX = useState(0.f);
  auto stickyCaretActive = useState(false);
  detail::CaretBlinkTimerSlot& blinkCaretTimer = StateStore::current()->claimSlot<detail::CaretBlinkTimerSlot>();

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

  // Width for layout + caret memo must match render: prefer `useLayoutRect()` (this element's frame),
  // else `Runtime::buildSlotRect()` like TextInput's `frameX` fallback (stale by one frame on first build).
  std::optional<Rect> const layoutRect = useLayoutRect();
  float const fieldW = layoutRect ? layoutRect->width
                                  : ([]() -> float {
                                       Runtime* const rt = Runtime::current();
                                       return rt ? rt->buildSlotRect().width : 200.f;
                                     })();
  float const fieldH = layoutRect ? layoutRect->height
                                  : ([]() -> float {
                                       Runtime* const rt = Runtime::current();
                                       return rt ? rt->buildSlotRect().height : 100.f;
                                     })();
  float const contentW = std::max(0.f, fieldW - 2.f * padHResolved);

  CaretGeom const cachedCaret = useMemo(
      [&]() -> CaretGeom {
        std::string const& text = bufCopy.empty() ? placeholderText : bufCopy;
        Color const col = bufCopy.empty() ? plc : tc;
        TextLayoutOptions opts{};
        opts.wrapping = TextWrapping::Wrap;
        opts.verticalAlignment = VerticalAlignment::Top;
        opts.lineHeight = lineHeightOpt;
        Rect const box{0.f, 0.f, contentW, 10000.f};
        std::shared_ptr<TextLayout> const layout =
            bufCopy.empty() ? ts.layout(placeholderText, fnt, plc, box, opts) : ts.layout(bufCopy, fnt, tc, box, opts);
        auto lines = detail::buildLineMetrics(text, *layout, ts, fnt, col);
        int const li = detail::lineIndexForByte(lines, cbKey, text);
        if (li < 0 || li >= static_cast<int>(lines.size())) {
          return {};
        }
        detail::LineMetrics const& le = lines[static_cast<std::size_t>(li)];
        int const rel = cbKey - le.byteStart;
        std::string const slice =
            text.substr(static_cast<std::size_t>(le.byteStart),
                        static_cast<std::size_t>(std::max(0, le.byteEnd - le.byteStart)));
        float const x = detail::caretXPosition(ts, slice, std::max(0, rel), fnt, col) + le.lineMinX;
        float const y = le.top;
        float const h = std::max(0.f, le.bottom - le.top);
        return {x, y, h};
      },
      bufCopy, cbKey, placeholderText, contentW, lineHeightOpt, fnt.size, fnt.weight, fnt.family, fnt.italic, tc.r, tc.g,
      tc.b, tc.a, plc.r, plc.g, plc.b, plc.a);

  bool const isDisabled = disabled;
  Cursor cursorResolved = isDisabled ? Cursor::Inherit : Cursor::IBeam;
  if (!isDisabled && outerMods && outerMods->cursor != Cursor::Inherit) {
    cursorResolved = outerMods->cursor;
  }
  bool const focused = useFocus();
  bool const pressInSubtree = usePress();
  if (!pressInSubtree && *mouseDragSelecting) {
    mouseDragSelecting = false;
  }

  if (focused && !isDisabled) {
    detail::ensureCaretBlinkTimerBridge();
    if (blinkCaretTimer.get() == 0) {
      std::uint64_t const id = Application::instance().scheduleRepeatingTimer(std::chrono::nanoseconds(530'000'000), 0);
      blinkCaretTimer.set(id);
    }
  } else {
    blinkCaretTimer.cancel();
  }

  std::function<void(std::string const&)> const onCh = onChange;
  std::function<void(std::string const&)> const onEsc = onEscape;
  int const maxLen = maxLength;
  float const padH = padHResolved;
  float const padV = padVResolved;

  // Value-capture dimensions and theme copies — pointer/keyboard handlers run after `body()` returns; `[&]` would
  // leave dangling refs to `fieldW`, `fnt`, `placeholderText`, etc.
  auto updateStickyColumnFromCaret = [fieldW, fieldH, padH, padV, lineHeightOpt, fnt, tc, plc, placeholderText,
                                      stickyCaretLayoutX, stickyCaretActive](std::string const& text, int byte) {
    float const x = caretLayoutXForByte(text, byte, fieldW, fieldH, padH, padV, lineHeightOpt, fnt, tc, plc,
                                        placeholderText);
    stickyCaretLayoutX = x;
    stickyCaretActive = true;
  };

  useViewAction(
      "edit.copy",
      [val, caretByte, selAnchor]() {
        auto [a, b] = detail::orderedSelection(*caretByte, *selAnchor);
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
      [val, caretByte, selAnchor, onCh, lastBlink, isDisabled, updateStickyColumnFromCaret]() {
        if (isDisabled) {
          return;
        }
        auto [a, b] = detail::orderedSelection(*caretByte, *selAnchor);
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
        detail::resetBlink(lastBlink);
        updateStickyColumnFromCaret(*val, *caretByte);
      },
      [caretByte, selAnchor, isDisabled]() {
        return !isDisabled && *caretByte != *selAnchor;
      });

  useViewAction(
      "edit.paste",
      [val, caretByte, selAnchor, onCh, maxLen, lastBlink, isDisabled, updateStickyColumnFromCaret]() {
        if (isDisabled) {
          return;
        }
        if (auto s = Application::instance().clipboard().readText()) {
          detail::replaceSelection(val, caretByte, selAnchor, std::move(*s), maxLen, onCh, lastBlink);
          updateStickyColumnFromCaret(*val, *caretByte);
        }
      },
      [isDisabled]() { return !isDisabled && Application::instance().clipboard().hasText(); });

  useViewAction(
      "edit.selectAll",
      [val, caretByte, selAnchor, isDisabled, updateStickyColumnFromCaret]() {
        if (isDisabled) {
          return;
        }
        selAnchor = 0;
        caretByte = static_cast<int>((*val).size());
        updateStickyColumnFromCaret(*val, *caretByte);
      },
      [isDisabled]() { return !isDisabled; });

  auto onText = [val, caretByte, selAnchor, onCh, maxLen, lastBlink, isDisabled, updateStickyColumnFromCaret](
                    std::string const& chunk) {
    if (isDisabled || chunk.empty()) {
      return;
    }
    detail::replaceSelection(val, caretByte, selAnchor, chunk, maxLen, onCh, lastBlink);
    updateStickyColumnFromCaret(*val, *caretByte);
  };

  auto onKey = [val, caretByte, selAnchor, scrollOffsetY, stickyCaretLayoutX, stickyCaretActive, onCh, onEsc, lastBlink,
                  isDisabled, fnt, tc, plc, padH, padV, fieldW, fieldH, lineHeightOpt, placeholderText, maxLen,
                  updateStickyColumnFromCaret](KeyCode k, Modifiers m) {
    if (isDisabled) {
      return;
    }
    bool const shift = any(m & Modifiers::Shift);
    bool const alt = any(m & Modifiers::Alt);
    bool const meta = any(m & Modifiers::Meta);

    std::string cur = *val;
    int const n = static_cast<int>(cur.size());
    auto [i0, i1] = detail::orderedSelection(*caretByte, *selAnchor);

    auto touchCaret = [&]() { detail::resetBlink(lastBlink); };

    auto layoutForEdit = [&]() -> std::shared_ptr<TextLayout> {
      TextSystem& ts = Application::instance().textSystem();
      TextLayoutOptions opts{};
      opts.wrapping = TextWrapping::Wrap;
      opts.verticalAlignment = VerticalAlignment::Top;
      opts.lineHeight = lineHeightOpt;
      float const cw = std::max(0.f, fieldW - 2.f * padH);
      float const ch = std::max(0.f, fieldH - 2.f * padV);
      Rect const box{0.f, 0.f, cw, ch};
      return cur.empty() ? ts.layout(placeholderText, fnt, plc, box, opts) : ts.layout(cur, fnt, tc, box, opts);
    };

    auto linesForEdit = [&]() {
      TextSystem& ts = Application::instance().textSystem();
      auto layout = layoutForEdit();
      return detail::buildLineMetrics(cur, *layout, ts, fnt, tc);
    };

    if (k == keys::Escape) {
      if (onEsc) {
        onEsc(*val);
      }
      return;
    }

    if (k == keys::Return) {
      detail::replaceSelection(val, caretByte, selAnchor, std::string("\n"), maxLen, onCh, lastBlink);
      updateStickyColumnFromCaret(*val, *caretByte);
      return;
    }

    if (k == keys::PageUp || k == keys::PageDown) {
      float const ch = std::max(0.f, fieldH - 2.f * padV);
      auto layout = layoutForEdit();
      float const contentH = layout->measuredSize.height;
      float s = *scrollOffsetY;
      s += k == keys::PageUp ? -ch : ch;
      s = std::clamp(s, 0.f, std::max(0.f, contentH - ch));
      scrollOffsetY = s;
      return;
    }

    if (meta && (k == keys::UpArrow || k == keys::DownArrow)) {
      if (!shift) {
        caretByte = k == keys::UpArrow ? 0 : n;
        selAnchor = k == keys::UpArrow ? 0 : n;
      } else {
        if (i0 == i1) {
          selAnchor = *caretByte;
        }
        caretByte = k == keys::UpArrow ? 0 : n;
      }
      touchCaret();
      updateStickyColumnFromCaret(*val, *caretByte);
      return;
    }

    if (k == keys::UpArrow || k == keys::DownArrow) {
      TextSystem& ts = Application::instance().textSystem();
      auto layout = layoutForEdit();
      auto lines = detail::buildLineMetrics(cur, *layout, ts, fnt, tc);
      if (lines.empty()) {
        return;
      }
      int const li = detail::lineIndexForByte(lines, *caretByte, cur);
      std::string const slice = cur.substr(static_cast<std::size_t>(lines[static_cast<std::size_t>(li)].byteStart),
                                           static_cast<std::size_t>(
                                               std::max(0, lines[static_cast<std::size_t>(li)].byteEnd -
                                                                  lines[static_cast<std::size_t>(li)].byteStart)));
      int const rel = *caretByte - lines[static_cast<std::size_t>(li)].byteStart;
      float const caretRelX = detail::caretXPosition(ts, slice, std::max(0, rel), fnt, tc);
      float const lineCaretX = caretRelX + lines[static_cast<std::size_t>(li)].lineMinX;
      float const targetX = *stickyCaretActive ? *stickyCaretLayoutX : lineCaretX;

      if (k == keys::UpArrow) {
        if (li <= 0) {
          if (!shift) {
            caretByte = 0;
            selAnchor = 0;
          } else {
            if (i0 == i1) {
              selAnchor = *caretByte;
            }
            caretByte = 0;
          }
        } else {
          detail::LineMetrics const& tl = lines[static_cast<std::size_t>(li - 1)];
          float const my = (tl.top + tl.bottom) * 0.5f;
          Point const pt{targetX, my};
          int const nb = detail::caretByteAtPoint(ts, cur, fnt, tc, *layout, pt);
          if (!shift) {
            caretByte = nb;
            selAnchor = nb;
          } else {
            if (i0 == i1) {
              selAnchor = *caretByte;
            }
            caretByte = nb;
          }
        }
        stickyCaretLayoutX = targetX;
        stickyCaretActive = true;
        touchCaret();
        return;
      }

      if (k == keys::DownArrow) {
        if (li + 1 >= static_cast<int>(lines.size())) {
          if (!shift) {
            caretByte = n;
            selAnchor = n;
          } else {
            if (i0 == i1) {
              selAnchor = *caretByte;
            }
            caretByte = n;
          }
        } else {
          detail::LineMetrics const& tl = lines[static_cast<std::size_t>(li + 1)];
          float const my = (tl.top + tl.bottom) * 0.5f;
          Point const pt{targetX, my};
          int const nb = detail::caretByteAtPoint(ts, cur, fnt, tc, *layout, pt);
          if (!shift) {
            caretByte = nb;
            selAnchor = nb;
          } else {
            if (i0 == i1) {
              selAnchor = *caretByte;
            }
            caretByte = nb;
          }
        }
        stickyCaretLayoutX = targetX;
        stickyCaretActive = true;
        touchCaret();
        return;
      }
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
        updateStickyColumnFromCaret(*val, *caretByte);
        return;
      }
      if (meta) {
        auto lines = linesForEdit();
        int const li = detail::lineIndexForByte(lines, *caretByte, cur);
        int const ls = lines[static_cast<std::size_t>(li)].byteStart;
        if (!shift) {
          caretByte = ls;
          selAnchor = ls;
        } else {
          if (i0 == i1) {
            selAnchor = *caretByte;
          }
          caretByte = ls;
        }
        touchCaret();
        updateStickyColumnFromCaret(*val, *caretByte);
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
      updateStickyColumnFromCaret(*val, *caretByte);
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
        updateStickyColumnFromCaret(*val, *caretByte);
        return;
      }
      if (meta) {
        auto lines = linesForEdit();
        int const li = detail::lineIndexForByte(lines, *caretByte, cur);
        int le = lineEndBeforeTrailingNewlines(cur, lines[static_cast<std::size_t>(li)]);
        if (!shift) {
          caretByte = le;
          selAnchor = le;
        } else {
          if (i0 == i1) {
            selAnchor = *caretByte;
          }
          caretByte = le;
        }
        touchCaret();
        updateStickyColumnFromCaret(*val, *caretByte);
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
      updateStickyColumnFromCaret(*val, *caretByte);
      return;
    }

    if (k == keys::Home) {
      auto lines = linesForEdit();
      int const li = detail::lineIndexForByte(lines, *caretByte, cur);
      int const ls = lines[static_cast<std::size_t>(li)].byteStart;
      if (!shift) {
        caretByte = ls;
        selAnchor = ls;
      } else {
        if (i0 == i1) {
          selAnchor = *caretByte;
        }
        caretByte = ls;
      }
      touchCaret();
      updateStickyColumnFromCaret(*val, *caretByte);
      return;
    }

    if (k == keys::End) {
      auto lines = linesForEdit();
      int const li = detail::lineIndexForByte(lines, *caretByte, cur);
      int le = lineEndBeforeTrailingNewlines(cur, lines[static_cast<std::size_t>(li)]);
      if (!shift) {
        caretByte = le;
        selAnchor = le;
      } else {
        if (i0 == i1) {
          selAnchor = *caretByte;
        }
        caretByte = le;
      }
      touchCaret();
      updateStickyColumnFromCaret(*val, *caretByte);
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
          updateStickyColumnFromCaret(*val, *caretByte);
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
        updateStickyColumnFromCaret(*val, *caretByte);
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
        updateStickyColumnFromCaret(*val, *caretByte);
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
        updateStickyColumnFromCaret(*val, *caretByte);
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
        updateStickyColumnFromCaret(*val, *caretByte);
      }
      return;
    }
  };

  auto onPointerDown = [val, caretByte, selAnchor, scrollOffsetY, lastBlink, mouseDragSelecting, mouseDragAnchorByte,
                          padH, padV, fieldW, fieldH, fnt, tc, plc, isDisabled, lineHeightOpt, placeholderText,
                          updateStickyColumnFromCaret](
                           Point local) {
    if (isDisabled) {
      return;
    }
    mouseDragSelecting = true;
    std::string const& buf = *val;
    TextSystem& ts = Application::instance().textSystem();
    TextLayoutOptions opts{};
    opts.wrapping = TextWrapping::Wrap;
    opts.verticalAlignment = VerticalAlignment::Top;
    opts.lineHeight = lineHeightOpt;
    float const cw = std::max(0.f, fieldW - 2.f * padH);
    float const ch = std::max(0.f, fieldH - 2.f * padV);
    Rect const content{0.f, 0.f, cw, ch};
    float const scroll = *scrollOffsetY;
    Point const contentPt{local.x - padH, local.y - padV + scroll};
    if (buf.empty()) {
      std::shared_ptr<TextLayout> const layout = ts.layout(placeholderText, fnt, plc, content, opts);
      int const pos = detail::caretByteAtPoint(ts, placeholderText, fnt, plc, *layout, contentPt);
      mouseDragAnchorByte = pos;
      caretByte = pos;
      selAnchor = pos;
      detail::resetBlink(lastBlink);
      updateStickyColumnFromCaret(*val, *caretByte);
      return;
    }
    std::shared_ptr<TextLayout> const layout = ts.layout(buf, fnt, tc, content, opts);
    int const pos = detail::caretByteAtPoint(ts, buf, fnt, tc, *layout, contentPt);
    mouseDragAnchorByte = pos;
    caretByte = pos;
    selAnchor = pos;
    detail::resetBlink(lastBlink);
    updateStickyColumnFromCaret(*val, *caretByte);
  };

  auto onPointerMove = [val, caretByte, selAnchor, scrollOffsetY, lastBlink, mouseDragSelecting, mouseDragAnchorByte,
                          padH, padV, fieldW, fieldH, fnt, tc, plc, isDisabled, lineHeightOpt, placeholderText,
                          updateStickyColumnFromCaret](
                           Point local) {
    if (isDisabled || !*mouseDragSelecting) {
      return;
    }
    std::string const& buf = *val;
    TextSystem& ts = Application::instance().textSystem();
    TextLayoutOptions opts{};
    opts.wrapping = TextWrapping::Wrap;
    opts.verticalAlignment = VerticalAlignment::Top;
    opts.lineHeight = lineHeightOpt;
    float const cw = std::max(0.f, fieldW - 2.f * padH);
    float const ch = std::max(0.f, fieldH - 2.f * padV);
    Rect const content{0.f, 0.f, cw, ch};
    float const scroll = *scrollOffsetY;
    Point const contentPt{local.x - padH, local.y - padV + scroll};
    int const anchor = *mouseDragAnchorByte;
    if (buf.empty()) {
      std::shared_ptr<TextLayout> const layout = ts.layout(placeholderText, fnt, plc, content, opts);
      int const pos = detail::caretByteAtPoint(ts, placeholderText, fnt, plc, *layout, contentPt);
      caretByte = pos;
      selAnchor = anchor;
      detail::resetBlink(lastBlink);
      updateStickyColumnFromCaret(*val, *caretByte);
      return;
    }
    std::shared_ptr<TextLayout> const layout = ts.layout(buf, fnt, tc, content, opts);
    int const pos = detail::caretByteAtPoint(ts, buf, fnt, tc, *layout, contentPt);
    caretByte = pos;
    selAnchor = anchor;
    detail::resetBlink(lastBlink);
    updateStickyColumnFromCaret(*val, *caretByte);
  };

  auto onPointerUp = [mouseDragSelecting](Point) { mouseDragSelecting = false; };

  auto onScrollHandler = [scrollOffsetY, padH, padV, fieldW, fieldH, val, fnt, tc, plc, lineHeightOpt,
                             placeholderText](Vec2 d) {
    std::string const& buf = *val;
    TextSystem& ts = Application::instance().textSystem();
    TextLayoutOptions opts{};
    opts.wrapping = TextWrapping::Wrap;
    opts.verticalAlignment = VerticalAlignment::Top;
    opts.lineHeight = lineHeightOpt;
    float const cw = std::max(0.f, fieldW - 2.f * padH);
    float const ch = std::max(0.f, fieldH - 2.f * padV);
    Rect const content{0.f, 0.f, cw, ch};
    std::shared_ptr<TextLayout> const layout =
        buf.empty() ? ts.layout(placeholderText, fnt, plc, content, opts) : ts.layout(buf, fnt, tc, content, opts);
    float const contentH = layout->measuredSize.height;
    float s = *scrollOffsetY;
    s -= d.y;
    s = std::clamp(s, 0.f, std::max(0.f, contentH - ch));
    if (std::abs(s - *scrollOffsetY) > 1e-4f) {
      scrollOffsetY = s;
    }
  };

  return Element{TextAreaView{
      .value = val,
      .caretByte = caretByte,
      .selAnchor = selAnchor,
      .scrollOffsetY = scrollOffsetY,
      .lastBlinkReset = lastBlink,
      .focused = focused,
      .disabled = isDisabled,
      .placeholder = placeholder,
      .font = fnt,
      .textColor = tc,
      .placeholderColor = plc,
      .borderColor = bc,
      .borderFocusColor = bfc,
      .caretColor = cc,
      .selectionColor = sc,
      .disabledColor = dc,
      .borderWidth = s.borderWidth,
      .borderFocusWidth = s.borderFocusWidth,
      .bgFillResolved = bgFill,
      .strokeNormalResolved = strokeN,
      .strokeFocusResolved = strokeF,
      .cornerRadiusResolved = cr,
      .height = height,
      .paddingH = padHResolved,
      .paddingV = padVResolved,
      .lineHeight = lineHeightOpt,
      .cachedCaretX = cachedCaret.x,
      .cachedCaretY = cachedCaret.y,
      .cachedCaretH = cachedCaret.h,
      .maxLength = maxLength,
      .onChange = onChange,
      .onEscape = onEscape,
      .onKeyDown = std::move(onKey),
      .onTextInput = std::move(onText),
      .onPointerDown = std::move(onPointerDown),
      .onPointerMove = std::move(onPointerMove),
      .onPointerUp = std::move(onPointerUp),
      .onScroll = std::move(onScrollHandler),
      .cursor = cursorResolved,
      .focusable = !isDisabled,
  }};
}

} // namespace flux
