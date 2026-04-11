#include <Flux/UI/Views/TextInput.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/UI/InputFieldLayout.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/TextArea.hpp>
#include <Flux/UI/Views/TextEditBehavior.hpp>
#include <Flux/UI/Views/TextEditUtils.hpp>

#include <Flux/Graphics/Font.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/StateStore.hpp>

#include "UI/Views/TextSupport.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace flux {

namespace {

constexpr float kSelectionExtraBottomPx = 4.f;
constexpr float kCaretScrollMarginPx = 8.f;

/// Same rule as TextArea: styler runs must cover every UTF-8 byte without gaps.
bool attributedRunsFullyCoverBuffer(std::vector<AttributedRun> const &runs, std::uint32_t n) {
    if (n == 0) {
        return true;
    }
    std::vector<std::pair<std::uint32_t, std::uint32_t>> se;
    se.reserve(runs.size());
    for (auto const &r : runs) {
        if (r.start >= r.end || r.end > n) {
            return false;
        }
        se.push_back({r.start, r.end});
    }
    std::sort(se.begin(), se.end());
    std::uint32_t pos = 0;
    for (auto const &[a, b] : se) {
        if (a > pos) {
            return false;
        }
        pos = std::max(pos, b);
    }
    return pos >= n;
}

struct ResolvedTextInputStyle {
    Color textColor;
    Color placeholderColor;
    Color backgroundColor;
    Color borderColor;
    Color borderFocusColor;
    Color caretColor;
    Color selectionColor;
    Color disabledColor;
    float borderWidth;
    float borderFocusWidth;
    float cornerRadius;
    float paddingH;
    float paddingV;
};

ResolvedTextInputStyle resolveTextInputStyle(TextInput::Style const &style, Theme const &theme) {
    return ResolvedTextInputStyle {
        .textColor = resolveColor(style.textColor, theme.colorTextPrimary),
        .placeholderColor = resolveColor(style.placeholderColor, theme.colorTextPlaceholder),
        .backgroundColor = resolveColor(style.backgroundColor, theme.colorSurfaceField),
        .borderColor = resolveColor(style.borderColor, theme.colorBorder),
        .borderFocusColor = resolveColor(style.borderFocusColor, theme.colorBorderFocus),
        .caretColor = resolveColor(style.caretColor, theme.colorAccent),
        .selectionColor = resolveColor(style.selectionColor, theme.colorAccentSubtle),
        .disabledColor = resolveColor(style.disabledColor, theme.colorSurfaceDisabled),
        .borderWidth = resolveFloat(style.borderWidth, 1.f),
        .borderFocusWidth = resolveFloat(style.borderFocusWidth, 2.f),
        .cornerRadius = resolveFloat(style.cornerRadius, theme.radiusMedium),
        .paddingH = resolveFloat(style.paddingH, theme.paddingFieldH),
        .paddingV = resolveFloat(style.paddingV, theme.paddingFieldV),
    };
}

TextArea::Style asTextAreaStyle(TextInput::Style const &style) {
    return TextArea::Style {
        .font = style.font,
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
        .lineHeight = style.lineHeight,
    };
}

struct TextInputSnap {
    detail::TextEditLayoutResult layoutResult;
    std::string layoutSource;
    float layoutContentW = 0.f;
    bool showingPlaceholder = false;
};

AttributedString buildAttributedString(std::string const &placeholderText,
                                       std::function<std::vector<AttributedRun>(std::string_view)> const &styler,
                                       std::function<Color(std::string_view)> const &validationColor,
                                       ResolvedTextInputStyle const &rs, Font const &defaultFont,
                                       std::string const &val, bool showPlaceholder) {
    if (showPlaceholder) {
        AttributedString ph;
        ph.utf8 = placeholderText;
        ph.runs.push_back(AttributedRun {0, static_cast<std::uint32_t>(placeholderText.size()), defaultFont, rs.placeholderColor});
        return ph;
    }
    AttributedString as;
    as.utf8 = val;
    std::uint32_t const n = static_cast<std::uint32_t>(val.size());
    if (styler) {
        as.runs = styler(val);
        if (as.runs.empty() || !attributedRunsFullyCoverBuffer(as.runs, n)) {
            as.runs.clear();
            as.runs.push_back(AttributedRun {0, n, defaultFont, rs.textColor});
        }
    } else {
        Color const c = validationColor ? validationColor(val) : rs.textColor;
        as.runs.push_back(AttributedRun {0, static_cast<std::uint32_t>(val.size()), defaultFont, c});
    }
    return as;
}

bool ensureLayout(TextInputSnap &snap, std::string const &placeholderText,
                  std::function<std::vector<AttributedRun>(std::string_view)> const &styler,
                  std::function<Color(std::string_view)> const &validationColor, ResolvedTextInputStyle const &rs,
                  Font const &defaultFont, std::string const &value, float contentW, bool showPlaceholder) {
    TextSystem &ts = Application::instance().textSystem();
    std::string const &layoutSource = showPlaceholder ? placeholderText : value;
    bool const widthChanged = std::abs(snap.layoutContentW - contentW) > 0.5f;
    bool const needRelayout =
        snap.layoutResult.empty() || widthChanged || snap.layoutSource != layoutSource || snap.showingPlaceholder != showPlaceholder;
    if (!needRelayout) {
        return !snap.layoutResult.empty();
    }

    AttributedString text =
        buildAttributedString(placeholderText, styler, validationColor, rs, defaultFont, value, showPlaceholder);
    TextLayoutOptions const opts = text_detail::makeTextLayoutOptions(TextWrapping::NoWrap);
    auto layout = ts.layout(text, contentW, opts);
    snap.layoutResult = detail::makeTextEditLayoutResult(layout, static_cast<int>(text.utf8.size()), contentW);
    snap.layoutSource = text.utf8;
    snap.layoutContentW = contentW;
    snap.showingPlaceholder = showPlaceholder;
    return !snap.layoutResult.empty();
}

int hitTestByte(TextInputSnap &snap, std::string const &placeholderText,
                std::function<std::vector<AttributedRun>(std::string_view)> const &styler,
                std::function<Color(std::string_view)> const &validationColor, TextEditBehavior &beh,
                ResolvedTextInputStyle const &rs, Font const &defaultFont, float frameWidth, Point local,
                int scrollByte, bool showPh) {
    std::string const &buf = beh.value();
    float const contentW = std::max(1.f, frameWidth - 2.f * (rs.borderWidth + rs.paddingH));
    if (!ensureLayout(snap, placeholderText, styler, validationColor, rs, defaultFont, buf, contentW, showPh)) {
        return 0;
    }
    if (snap.layoutResult.lines.empty()) {
        return 0;
    }
    detail::LineMetrics const &line0 = snap.layoutResult.lines[0];
    float const scrollX = detail::caretXForByte(*snap.layoutResult.layout, line0, detail::utf8Clamp(buf, scrollByte));
    float const lx = local.x - rs.borderWidth - rs.paddingH + scrollX;
    std::string const &sliceBuf = showPh ? placeholderText : buf;
    return detail::caretByteAtX(*snap.layoutResult.layout, line0, lx, sliceBuf);
}

struct TextInputView {
    /// Copied from \c TextInput in \c body() — the view outlives the temporary component value.
    std::string placeholder;
    std::function<std::vector<AttributedRun>(std::string_view)> styler;
    std::function<Color(std::string_view)> validationColor;
    TextEditBehavior *behavior = nullptr;
    TextInputSnap *snap = nullptr;
    /// Copy of \c State handle (same \c Signal* as \c useState in \c body) — must not be a pointer to a
    /// stack \c State local, which would dangle after \c body() returns.
    State<int> scroll {};
    ResolvedTextInputStyle rs {};
    Font defaultFont {};
    float explicitHeight = 0.f;
    bool disabled = false;
    bool focused = false;

    void render(Canvas &canvas, Rect frame) const {
        std::string const &buf = behavior->value();
        bool const showPh = buf.empty() && !focused;

        StrokeStyle const stroke =
            focused ? StrokeStyle::solid(rs.borderFocusColor, rs.borderFocusWidth) : StrokeStyle::solid(rs.borderColor, rs.borderWidth);
        canvas.drawRect(frame, CornerRadius {rs.cornerRadius}, FillStyle::solid(rs.backgroundColor), stroke);

        float const innerLeft = frame.x + rs.borderWidth + rs.paddingH;
        float const innerTop = frame.y + rs.borderWidth + rs.paddingV;
        float const contentW =
            std::max(1.f, frame.width - 2.f * (rs.borderWidth + rs.paddingH));

        if (!ensureLayout(*snap, placeholder, styler, validationColor, rs, defaultFont, buf, contentW, showPh)) {
            return;
        }
        if (snap->layoutResult.lines.empty()) {
            Point textOrigin {innerLeft, innerTop};
            int sb = *scroll;
            if (!showPh) {
                sb = detail::utf8Clamp(buf, sb);
            } else {
                sb = 0;
            }
            scroll = sb;
            canvas.drawTextLayout(*snap->layoutResult.layout, textOrigin);
            if (disabled) {
                Color dc = rs.disabledColor;
                dc.a *= 0.35f;
                canvas.drawRect(frame, CornerRadius {rs.cornerRadius}, FillStyle::solid(dc), StrokeStyle::none());
            }
            return;
        }
        detail::LineMetrics const &line0 = snap->layoutResult.lines[0];

        Point textOrigin {innerLeft, innerTop};
        int sb = *scroll;
        if (!showPh) {
            sb = detail::utf8Clamp(buf, sb);
        } else {
            sb = 0;
        }
        scroll = sb;

        if (behavior->consumeEnsureCaretVisibleRequest() && !showPh) {
            int cur = sb;
            for (int iter = 0; iter < 128; ++iter) {
                float const cx = detail::caretXForByte(*snap->layoutResult.layout, line0, behavior->caretByte());
                float const rel = cx - detail::caretXForByte(*snap->layoutResult.layout, line0, cur);
                if (rel >= kCaretScrollMarginPx && rel <= contentW - kCaretScrollMarginPx) {
                    break;
                }
                if (rel < kCaretScrollMarginPx) {
                    if (cur <= 0) {
                        break;
                    }
                    cur = detail::utf8PrevChar(buf, cur);
                } else {
                    if (cur >= static_cast<int>(buf.size())) {
                        break;
                    }
                    cur = detail::utf8NextChar(buf, cur);
                }
            }
            scroll = cur;
        }

        float const scrollX2 = detail::caretXForByte(*snap->layoutResult.layout, line0, *scroll);
        textOrigin.x -= scrollX2;

        if (!showPh && behavior->hasSelection()) {
            detail::TextEditSelection const selection {
                .caretByte = detail::utf8Clamp(buf, behavior->caretByte()),
                .anchorByte = detail::utf8Clamp(buf, behavior->selectionAnchorByte()),
            };
            for (Rect const &r : detail::selectionRects(snap->layoutResult, selection, textOrigin.x, innerTop,
                                                        kSelectionExtraBottomPx)) {
                canvas.drawRect(r, CornerRadius {}, FillStyle::solid(rs.selectionColor), StrokeStyle::none());
            }
        }

        canvas.drawTextLayout(*snap->layoutResult.layout, textOrigin);

        if (focused && !disabled && !showPh) {
            float const cx =
                detail::caretXForByte(*snap->layoutResult.layout, line0, behavior->caretByte()) + textOrigin.x;
            auto const [caretY0, caretY1] = detail::lineCaretYRangeInLayout(*snap->layoutResult.layout, line0);
            float const phase = behavior->caretBlinkPhase();
            float const alpha = phase <= 0.5f ? 1.f : 0.f;
            Color cc = rs.caretColor;
            cc.a *= alpha;
            canvas.drawLine(Point {cx, innerTop + caretY0}, Point {cx, innerTop + caretY1},
                            StrokeStyle::solid(cc, detail::kTextCaretStrokeWidthPx));
        }

        if (disabled) {
            Color dc = rs.disabledColor;
            dc.a *= 0.35f;
            canvas.drawRect(frame, CornerRadius {rs.cornerRadius}, FillStyle::solid(dc), StrokeStyle::none());
        }
    }

    Size measure(LayoutConstraints const &cs, LayoutHints const &) const {
        float const h =
            resolvedInputFieldHeight(defaultFont, rs.textColor, rs.paddingV, explicitHeight);
        float w = 120.f;
        if (std::isfinite(cs.maxWidth) && cs.maxWidth > 0.f) {
            w = cs.maxWidth;
        }
        return {w, h};
    }
};

} // namespace

Element TextInput::body() const {
    if (multiline) {
        return TextArea {
            .value = value,
            .placeholder = placeholder,
            .style = asTextAreaStyle(style),
            .height =
                TextAreaHeight {
                    .fixed = multilineHeight.fixed,
                    .minIntrinsic = multilineHeight.minIntrinsic,
                    .maxIntrinsic = multilineHeight.maxIntrinsic,
                },
            .styler = styler,
            .disabled = disabled,
            .maxLength = maxLength,
            .onChange = onChange,
            .onEscape = onEscape,
        };
    }

    Theme const &theme = useEnvironment<Theme>();
    ResolvedTextInputStyle const resolved = resolveTextInputStyle(style, theme);
    Font const defaultFont = text_detail::resolveBodyTextStyle(style.font, kColorFromTheme).first;

    auto &beh = useTextEditBehavior(value, {.multiline = false,
                                            .maxLength = maxLength,
                                            .acceptsTab = false,
                                            .submitsOnEnter = true,
                                            .onChange = onChange,
                                            .onSubmit = onSubmit,
                                            .onEscape = onEscape,
                                            .verticalResolver = nullptr});
    beh.setDisabled(disabled);

    bool const focused = useFocus();
    beh.setFocused(focused);

    TextInputSnap &snap = StateStore::current()->claimSlot<TextInputSnap>();
    State<int> scrollByte = useState(0);
    std::optional<Rect> layoutRect = useLayoutRect();

    TextInputView view {};
    view.placeholder = placeholder;
    view.styler = styler;
    view.validationColor = validationColor;
    view.behavior = &beh;
    view.snap = &snap;
    view.scroll = scrollByte;
    view.rs = resolved;
    view.defaultFont = defaultFont;
    view.explicitHeight = style.height;
    view.disabled = disabled;
    view.focused = focused;

    return Element {view}
        .focusable(!disabled)
        .cursor(Cursor::IBeam)
        .onKeyDown([&beh](KeyCode key, Modifiers mods) { beh.handleKey(KeyEvent {key, mods}); })
        .onTextInput([&beh](std::string const &t) { beh.handleTextInput(t); })
        .onPointerDown([ph = placeholder, stylerFn = styler, validationFn = validationColor, &beh, &snap, scrollByte,
                        resolved, defaultFont, layoutRect, focused](Point local) {
            float const fw = layoutRect ? layoutRect->width : 400.f;
            bool const showPh = beh.value().empty() && !focused;
            int const byte =
                hitTestByte(snap, ph, stylerFn, validationFn, beh, resolved, defaultFont, fw, local, *scrollByte, showPh);
            beh.handlePointerDown(byte, false);
        })
        .onPointerMove([ph = placeholder, stylerFn = styler, validationFn = validationColor, &beh, &snap, scrollByte,
                        resolved, defaultFont, layoutRect, focused](Point local) {
            float const fw = layoutRect ? layoutRect->width : 400.f;
            bool const showPh = beh.value().empty() && !focused;
            int const byte =
                hitTestByte(snap, ph, stylerFn, validationFn, beh, resolved, defaultFont, fw, local, *scrollByte, showPh);
            beh.handlePointerDrag(byte);
        })
        .onPointerUp([&beh](Point) { beh.handlePointerUp(); });
}

} // namespace flux
