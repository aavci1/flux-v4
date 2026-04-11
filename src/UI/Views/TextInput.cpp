#include <Flux/UI/Views/TextInput.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/UI/InputFieldLayout.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/TextEditBehavior.hpp>
#include <Flux/UI/Views/TextEditUtils.hpp>

#include <Flux/Graphics/Font.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ScrollView.hpp>

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

/// Same rule as multiline TextInput: styler runs must cover every UTF-8 byte without gaps.
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

struct TextInputLineWrapSnap {
    detail::TextEditLayoutResult layoutResult;
    std::string layoutSource;
    float layoutFrameW = 0.f;
    float layoutContentW = 0.f;
};

struct TextInputStylerMemo {
    std::string value;
    std::vector<AttributedRun> runs;
};

struct ResolvedLineWrapTextInputStyle {
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
    float lineHeight = 0.f;
};

ResolvedLineWrapTextInputStyle resolveLineWrapTextInputStyle(TextInput::Style const &style, Theme const &theme) {
    return ResolvedLineWrapTextInputStyle {
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
        .lineHeight = resolveFloat(style.lineHeight, 0.f),
    };
}

AttributedString buildLineWrapAttributedString(std::string const &placeholderText,
                                               std::function<std::vector<AttributedRun>(std::string_view)> const &styler,
                                               ResolvedLineWrapTextInputStyle const &rs, Font const &defaultFont,
                                               std::string const &value, bool showPlaceholder,
                                               TextInputStylerMemo &memo) {
    if (showPlaceholder) {
        AttributedString ph;
        ph.utf8 = placeholderText;
        ph.runs.push_back(
            AttributedRun {0, static_cast<std::uint32_t>(placeholderText.size()), defaultFont, rs.placeholderColor});
        return ph;
    }

    AttributedString as;
    as.utf8 = value;
    if (styler) {
        std::uint32_t const n = static_cast<std::uint32_t>(value.size());
        if (memo.value == value && !memo.runs.empty() && attributedRunsFullyCoverBuffer(memo.runs, n)) {
            as.runs = memo.runs;
        } else {
            memo.runs = styler(value);
            memo.value = value;
            as.runs = memo.runs;
            if (as.runs.empty() || !attributedRunsFullyCoverBuffer(as.runs, n)) {
                as.runs.clear();
                as.runs.push_back(AttributedRun {0, n, defaultFont, rs.textColor});
                memo.runs = as.runs;
            }
        }
    } else {
        as.runs.push_back(
            AttributedRun {0, static_cast<std::uint32_t>(value.size()), defaultFont, rs.textColor});
    }
    return as;
}

int verticalMove(TextInputLineWrapSnap const &snap, std::string const &buf, int currentByte, int direction) {
    if (snap.layoutResult.empty() || snap.layoutResult.lines.empty()) {
        return currentByte;
    }
    TextLayout const &layout = *snap.layoutResult.layout;
    int const srcIndex = detail::lineIndexForByte(snap.layoutResult.lines, currentByte);
    int const targetIndex =
        std::clamp(srcIndex + direction, 0, static_cast<int>(snap.layoutResult.lines.size()) - 1);
    if (srcIndex == targetIndex) {
        return currentByte;
    }

    auto const &srcLine = snap.layoutResult.lines[static_cast<std::size_t>(srcIndex)];
    auto const &dstLine = snap.layoutResult.lines[static_cast<std::size_t>(targetIndex)];
    float const x = detail::caretXForByte(layout, srcLine, currentByte);
    return detail::caretByteAtX(layout, dstLine, x, buf);
}

std::pair<int, bool> lineIndexAtYWithFallback(std::vector<detail::LineMetrics> const &lines, float layoutY) {
    if (lines.empty()) {
        return {0, false};
    }
    auto const &first = lines.front();
    auto const &last = lines.back();
    if (layoutY < first.top) {
        return {0, true};
    }
    if (layoutY >= last.bottom) {
        return {static_cast<int>(lines.size()) - 1, true};
    }
    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        auto const &line = lines[static_cast<std::size_t>(i)];
        if (layoutY >= line.top && layoutY < line.bottom) {
            return {i, false};
        }
    }
    float bestDistance = 1e9f;
    int bestIndex = 0;
    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        auto const &line = lines[static_cast<std::size_t>(i)];
        float const mid = (line.top + line.bottom) * 0.5f;
        float const distance = std::abs(layoutY - mid);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestIndex = i;
        }
    }
    return {bestIndex, true};
}

int hitTestLineWrapByte(TextEditBehavior &behavior, ResolvedLineWrapTextInputStyle const &rs,
                        std::string const &placeholder,
                        std::function<std::vector<AttributedRun>(std::string_view)> const &styler,
                        TextInputStylerMemo &stylerMemo, Font const &defaultFont, bool focused,
                        TextInputLineWrapSnap &snap, float frameWidth, Point local, float scrollY) {
    std::string const &buf = behavior.value();
    if (buf.empty()) {
        return 0;
    }

    bool const showPlaceholder = buf.empty() && !focused;
    float const contentW = std::max(1.f, frameWidth - 2.f * (rs.borderWidth + rs.paddingH));
    bool const needRelayout =
        snap.layoutResult.empty() || snap.layoutResult.lines.empty() || buf != snap.layoutSource;
    if (needRelayout) {
        TextSystem &ts = Application::instance().textSystem();
        AttributedString text =
            buildLineWrapAttributedString(placeholder, styler, rs, defaultFont, buf, showPlaceholder, stylerMemo);
        TextLayoutOptions const opts = text_detail::makeTextLayoutOptions(TextWrapping::Wrap, rs.lineHeight);
        auto layout = ts.layout(text, contentW, opts);
        if (!layout) {
            return 0;
        }
        snap.layoutResult = detail::makeTextEditLayoutResult(layout, static_cast<int>(text.utf8.size()), contentW);
        snap.layoutSource = buf;
        snap.layoutFrameW = frameWidth;
        snap.layoutContentW = contentW;
    }

    float const layoutY = local.y - rs.borderWidth - rs.paddingV + scrollY;
    auto const [lineIndex, _fallback] = lineIndexAtYWithFallback(snap.layoutResult.lines, layoutY);
    detail::LineMetrics const &line = snap.layoutResult.lines[static_cast<std::size_t>(lineIndex)];
    float const layoutX = local.x - rs.borderWidth - rs.paddingH;
    return detail::caretByteAtX(*snap.layoutResult.layout, line, layoutX, buf);
}

struct MultilineTextInputView {
    std::string placeholder;
    std::function<std::vector<AttributedRun>(std::string_view)> styler;
    TextEditBehavior *behavior = nullptr;
    TextInputLineWrapSnap *snap = nullptr;
    TextInputStylerMemo *stylerMemo = nullptr;
    State<Point> scrollOffset {};
    State<Size> viewportSize {};
    State<Size> contentSize {};
    ResolvedLineWrapTextInputStyle rs {};
    Font defaultFont {};
    bool disabled = false;
    bool focused = false;

    void render(Canvas &canvas, Rect frame) const {
        TextSystem &ts = Application::instance().textSystem();
        std::string const &buf = behavior->value();
        bool const showPlaceholder = buf.empty() && !focused;
        AttributedString text =
            buildLineWrapAttributedString(placeholder, styler, rs, defaultFont, buf, showPlaceholder, *stylerMemo);

        float const innerLeft = frame.x + rs.paddingH;
        float const innerTop = frame.y + rs.paddingV;
        float const contentW = std::max(1.f, frame.width - 2.f * rs.paddingH);

        TextLayoutOptions const opts = text_detail::makeTextLayoutOptions(TextWrapping::Wrap, rs.lineHeight);
        auto layout = ts.layout(text, contentW, opts);
        if (!layout) {
            return;
        }

        snap->layoutResult = detail::makeTextEditLayoutResult(layout, static_cast<int>(text.utf8.size()), contentW);
        snap->layoutSource = buf;
        snap->layoutFrameW = frame.width;
        snap->layoutContentW = contentW;

        Point scroll = clampScrollOffset(ScrollAxis::Vertical, *scrollOffset, *viewportSize, *contentSize);
        if (scroll.x != (*scrollOffset).x || scroll.y != (*scrollOffset).y) {
            scrollOffset = scroll;
        }
        if (behavior->consumeEnsureCaretVisibleRequest() && !snap->layoutResult.lines.empty()) {
            int const lineIndex = detail::lineIndexForByte(snap->layoutResult.lines, behavior->caretByte());
            auto const &line = snap->layoutResult.lines[static_cast<std::size_t>(lineIndex)];
            if (line.top < scroll.y + kCaretScrollMarginPx) {
                scroll.y = line.top - kCaretScrollMarginPx;
            } else if (line.bottom > scroll.y + (*viewportSize).height - kCaretScrollMarginPx) {
                scroll.y = line.bottom - (*viewportSize).height + kCaretScrollMarginPx;
            }
            scroll = clampScrollOffset(ScrollAxis::Vertical, scroll, *viewportSize, *contentSize);
            scrollOffset = scroll;
        }

        Point textOrigin {innerLeft, innerTop};

        if (!showPlaceholder && behavior->hasSelection()) {
            detail::TextEditSelection const selection {
                .caretByte = detail::utf8Clamp(buf, behavior->caretByte()),
                .anchorByte = detail::utf8Clamp(buf, behavior->selectionAnchorByte()),
            };
            for (Rect const &rect :
                 detail::selectionRects(snap->layoutResult, selection, innerLeft, innerTop, kSelectionExtraBottomPx)) {
                canvas.drawRect(rect, CornerRadius {}, FillStyle::solid(rs.selectionColor), StrokeStyle::none());
            }
        }

        canvas.drawTextLayout(*snap->layoutResult.layout, textOrigin);

        if (focused && !disabled && !showPlaceholder && !snap->layoutResult.lines.empty()) {
            int const lineIndex = detail::lineIndexForByte(snap->layoutResult.lines, behavior->caretByte());
            auto const &line = snap->layoutResult.lines[static_cast<std::size_t>(lineIndex)];
            float const caretX =
                innerLeft + detail::caretXForByte(*snap->layoutResult.layout, line, behavior->caretByte());
            auto const [caretY0, caretY1] = detail::lineCaretYRangeInLayout(*snap->layoutResult.layout, line);
            Color caretColor = rs.caretColor;
            caretColor.a *= behavior->caretBlinkPhase() <= 0.5f ? 1.f : 0.f;
            canvas.drawLine(Point {caretX, innerTop + caretY0}, Point {caretX, innerTop + caretY1},
                            StrokeStyle::solid(caretColor, detail::kTextCaretStrokeWidthPx));
        }
    }

    Size measure(LayoutConstraints const &cs, LayoutHints const &) const {
        TextSystem &ts = Application::instance().textSystem();
        std::string const &buf = behavior->value();
        bool const showPlaceholder = buf.empty() && !focused;
        AttributedString text =
            buildLineWrapAttributedString(placeholder, styler, rs, defaultFont, buf, showPlaceholder, *stylerMemo);
        float const width = std::isfinite(cs.maxWidth) && cs.maxWidth > 0.f ? cs.maxWidth : 200.f;
        TextLayoutOptions const opts = text_detail::makeTextLayoutOptions(TextWrapping::Wrap, rs.lineHeight);
        auto layout = ts.layout(text, std::max(1.f, width - 2.f * rs.paddingH), opts);
        if (!layout) {
            return {width, 0.f};
        }
        return {width, layout->measuredSize.height + 2.f * rs.paddingV};
    }
};

Element buildMultilineTextInput(TextInput const &input) {
    Theme const &theme = useEnvironment<Theme>();
    ResolvedLineWrapTextInputStyle const resolved = resolveLineWrapTextInputStyle(input.style, theme);
    Font const defaultFont = text_detail::resolveBodyTextStyle(input.style.font, kColorFromTheme).first;

    TextInputLineWrapSnap &snap = StateStore::current()->claimSlot<TextInputLineWrapSnap>();
    TextInputStylerMemo &stylerMemo = StateStore::current()->claimSlot<TextInputStylerMemo>();

    State<Point> scrollOffset = useState(Point {0.f, 0.f});
    State<Size> viewportSize = useState(Size {0.f, 0.f});
    State<Size> contentSize = useState(Size {0.f, 0.f});
    std::optional<Rect> layoutRect = useLayoutRect();

    auto &behavior = useTextEditBehavior(input.value, {.multiline = true,
                                                       .maxLength = input.maxLength,
                                                       .acceptsTab = true,
                                                       .submitsOnEnter = false,
                                                       .onChange = input.onChange,
                                                       .onSubmit = nullptr,
                                                       .onEscape = input.onEscape,
                                                       .verticalResolver = [&snap, st = input.value](int cur, int dir) {
                                                           return verticalMove(snap, *st, cur, dir);
                                                       }});
    behavior.setDisabled(input.disabled);

    bool const focused = useFocus();
    behavior.setFocused(focused);

    MultilineTextInputView view {};
    view.placeholder = input.placeholder;
    view.styler = input.styler;
    view.behavior = &behavior;
    view.snap = &snap;
    view.stylerMemo = &stylerMemo;
    view.scrollOffset = scrollOffset;
    view.viewportSize = viewportSize;
    view.contentSize = contentSize;
    view.rs = resolved;
    view.defaultFont = defaultFont;
    view.disabled = input.disabled;
    view.focused = focused;

    Element editor = Element {view}
                         .focusable(!input.disabled)
                         .cursor(Cursor::IBeam)
                         .onKeyDown([&behavior](KeyCode key, Modifiers mods) { behavior.handleKey(KeyEvent {key, mods}); })
                         .onTextInput([&behavior](std::string const &text) { behavior.handleTextInput(text); })
                         .onPointerDown([placeholder = input.placeholder, styler = input.styler, &behavior, &snap, resolved,
                                         &stylerMemo, defaultFont, focused, scrollOffset, layoutRect](Point local) {
                             float const frameWidth = layoutRect ? layoutRect->width : 400.f;
                             float const scroll = (*scrollOffset).y;
                             int const byte =
                                 hitTestLineWrapByte(behavior, resolved, placeholder, styler, stylerMemo, defaultFont,
                                                     focused, snap, frameWidth, local, scroll);
                             behavior.handlePointerDown(byte, false);
                         })
                         .onPointerMove([placeholder = input.placeholder, styler = input.styler, &behavior, &snap, resolved,
                                         &stylerMemo, defaultFont, focused, scrollOffset, layoutRect](Point local) {
                             float const frameWidth = layoutRect ? layoutRect->width : 400.f;
                             float const scroll = (*scrollOffset).y;
                             int const byte =
                                 hitTestLineWrapByte(behavior, resolved, placeholder, styler, stylerMemo, defaultFont,
                                                     focused, snap, frameWidth, local, scroll);
                             behavior.handlePointerDrag(byte);
                         })
                         .onPointerUp([&behavior](Point) { behavior.handlePointerUp(); });

    Element scroller = Element {ScrollView {
        .axis = ScrollAxis::Vertical,
        .scrollOffset = scrollOffset,
        .viewportSize = viewportSize,
        .contentSize = contentSize,
        .dragScrollEnabled = false,
        .children = children(std::move(editor)),
    }};

    StrokeStyle const stroke = focused ? StrokeStyle::solid(resolved.borderFocusColor, resolved.borderFocusWidth)
                                       : StrokeStyle::solid(resolved.borderColor, resolved.borderWidth);

    Element field = std::move(scroller)
                        .fill(FillStyle::solid(resolved.backgroundColor))
                        .stroke(stroke)
                        .cornerRadius(CornerRadius {resolved.cornerRadius})
                        .height(input.multilineHeight.fixed > 0.f ? input.multilineHeight.fixed : input.multilineHeight.minIntrinsic);

    if (input.disabled) {
        Color overlay = resolved.disabledColor;
        overlay.a *= 0.35f;
        field = std::move(field).overlay(Rectangle {}.fill(FillStyle::solid(overlay)));
    }

    return field;
}

} // namespace

Element TextInput::body() const {
    if (multiline) {
        return buildMultilineTextInput(*this);
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
