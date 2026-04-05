#include <Flux/Core/Application.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/Events.hpp>
#include <Flux/Detail/Runtime.hpp>
#include <Flux/Core/KeyCodes.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/InputFieldChrome.hpp>
#include <Flux/UI/InputFieldLayout.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/TextInput.hpp>
#include <Flux/UI/Views/TextEditKernel.hpp>

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
#include <vector>

#include <Flux/Graphics/TextLayout.hpp>

namespace flux {

namespace {

using namespace std::chrono;

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

struct TextInputView {
  Size measure(LayoutConstraints const& cs, LayoutHints const&) const {
    float const w = std::isfinite(cs.maxWidth) ? cs.maxWidth : 200.f;
    float const h = resolvedInputFieldHeight(font, textColor, paddingV, height);
    return {std::max(cs.minWidth, w), h};
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
  float height = 0.f;
  float paddingH = 12.f;
  float paddingV = 8.f;

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

  auto [i0, i1] = detail::orderedSelection(*caretByte, *selAnchor);
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

  Theme const& theme = useEnvironment<Theme>();
  Font const fnt = resolveFont(font, theme.typeBody.toFont());
  ResolvedInputFieldChrome const resolved =
      resolveInputFieldChrome(InputFieldChromeSpec{.textColor = textColor,
                                                   .placeholderColor = placeholderColor,
                                                   .backgroundColor = backgroundColor,
                                                   .borderColor = borderColor,
                                                   .borderFocusColor = borderFocusColor,
                                                   .caretColor = caretColor,
                                                   .selectionColor = selectionColor,
                                                   .disabledColor = disabledColor,
                                                   .borderWidth = borderWidth,
                                                   .borderFocusWidth = borderFocusWidth,
                                                   .cornerRadius = cornerRadius,
                                                   .paddingH = paddingH,
                                                   .paddingV = paddingV},
                              theme);
  float const padHResolved = resolved.paddingH;
  float const padVResolved = resolved.paddingV;
  Color const tc = resolved.textColor;
  Color const plc = resolved.placeholderColor;
  Color const bc = resolved.borderColor;
  Color const bfc = resolved.borderFocusColor;
  Color const cc = resolved.caretColor;
  Color const sc = resolved.selectionColor;
  Color const dc = resolved.disabledColor;

  InputFieldDecoration const deco = applyOuterInputFieldDecoration(resolved, useOuterElementModifiers());
  FillStyle bgFill = deco.bgFill;
  StrokeStyle strokeN = deco.strokeNormal;
  StrokeStyle strokeF = deco.strokeFocus;
  CornerRadius cr = deco.cornerRadius;

  State<std::string> val = value;
  auto caretByte = useState(0);
  auto selAnchor = useState(0);
  auto scrollOffset = useState(0.f);
  auto lastBlink = useState(duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()));
  auto mouseDragSelecting = useState(false);
  auto mouseDragAnchorByte = useState(0);
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
    detail::ensureCaretBlinkTimerBridge();
    detail::caretBlinkSyncEpoch(*lastBlink);
    blinkCaretTimer.syncFocus(true, false);
  } else {
    blinkCaretTimer.syncFocus(false, false);
  }

  std::function<void(std::string const&)> const onCh = onChange;
  std::function<void(std::string const&)> const onSub = onSubmit;
  int const maxLen = maxLength;
  float const padH = padHResolved;

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
      [val, caretByte, selAnchor, onCh, lastBlink, isDisabled]() {
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
          detail::replaceSelection(val, caretByte, selAnchor, std::move(*s), maxLen, onCh, lastBlink);
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
    detail::replaceSelection(val, caretByte, selAnchor, chunk, maxLen, onCh, lastBlink);
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
    auto [i0, i1] = detail::orderedSelection(*caretByte, *selAnchor);

    auto touchCaret = [&]() { detail::resetBlink(lastBlink); };

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
                          padH, fnt, tc, isDisabled](Point local) {
    if (isDisabled) {
      return;
    }
    mouseDragSelecting = true;
    std::string const& buf = *val;
    TextSystem& ts = Application::instance().textSystem();
    float const scroll = *scrollOffset;
    float const textX = local.x - padH + scroll;
    if (buf.empty()) {
      mouseDragAnchorByte = 0;
      caretByte = 0;
      selAnchor = 0;
      detail::resetBlink(lastBlink);
      return;
    }
    int const pos = detail::caretByteAtTextX(ts, buf, fnt, tc, textX);
    mouseDragAnchorByte = pos;
    caretByte = pos;
    selAnchor = pos;
    detail::resetBlink(lastBlink);
  };

  auto onPointerMove = [val, caretByte, selAnchor, scrollOffset, lastBlink, mouseDragSelecting, mouseDragAnchorByte,
                          padH, fnt, tc, isDisabled](Point local) {
    if (isDisabled || !*mouseDragSelecting) {
      return;
    }
    std::string const& buf = *val;
    TextSystem& ts = Application::instance().textSystem();
    float const scroll = *scrollOffset;
    float const textX = local.x - padH + scroll;
    int const anchor = *mouseDragAnchorByte;
    if (buf.empty()) {
      caretByte = 0;
      selAnchor = anchor;
      detail::resetBlink(lastBlink);
      return;
    }
    int const pos = detail::caretByteAtTextX(ts, buf, fnt, tc, textX);
    caretByte = pos;
    selAnchor = anchor;
    detail::resetBlink(lastBlink);
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
      .borderColor = bc,
      .borderFocusColor = bfc,
      .caretColor = cc,
      .selectionColor = sc,
      .disabledColor = dc,
      .borderWidth = resolved.borderWidth,
      .borderFocusWidth = resolved.borderFocusWidth,
      .bgFillResolved = bgFill,
      .strokeNormalResolved = strokeN,
      .strokeFocusResolved = strokeF,
      .cornerRadiusResolved = cr,
      .height = height,
      .paddingH = padHResolved,
      .paddingV = padVResolved,
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
