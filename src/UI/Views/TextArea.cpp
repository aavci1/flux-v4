#include <Flux/UI/Views/TextArea.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/UI/Theme.hpp>
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
#include <utility>

namespace flux {

namespace {

constexpr float kSelectionExtraBottomPx = 4.f;
constexpr float kCaretScrollMarginPx = 8.f;

/// Styler output must cover every byte of \p n without gaps (CoreText / layout assume contiguous spans).
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

struct TextAreaSnap {
    std::shared_ptr<TextLayout const> layout;
    std::vector<detail::LineMetrics> lines;
    /// Last buffer string used to build `layout` (hit-test invalidates when this differs from `beh.value()`).
    std::string layoutSource;
    /// Last layout pass frame/content width (from `render` or hit-test relayout when cache was cold).
    float layoutFrameW = 0.f;
    float layoutContentW = 0.f;
};

struct StylerMemo {
    std::string value;
    std::vector<AttributedRun> runs;
};

struct ResolvedTextAreaStyle {
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

ResolvedTextAreaStyle resolveTextAreaStyle(TextArea::Style const &style, Theme const &theme) {
    return ResolvedTextAreaStyle {
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

AttributedString buildAttributedString(std::string const &placeholderText,
                                       std::function<std::vector<AttributedRun>(std::string_view)> const &styler,
                                       ResolvedTextAreaStyle const &rs, Font const &defFont, std::string const &val,
                                       bool showPlaceholder, StylerMemo &memo) {
    if (showPlaceholder) {
        AttributedString ph;
        ph.utf8 = placeholderText;
        ph.runs.push_back(AttributedRun {0, static_cast<std::uint32_t>(placeholderText.size()), defFont, rs.placeholderColor});
        return ph;
    }
    AttributedString as;
    as.utf8 = val;
    if (styler) {
        std::uint32_t const n = static_cast<std::uint32_t>(val.size());
        if (memo.value == val && !memo.runs.empty() && attributedRunsFullyCoverBuffer(memo.runs, n)) {
            as.runs = memo.runs;
        } else {
            memo.runs = styler(val);
            memo.value = val;
            as.runs = memo.runs;
            if (as.runs.empty() || !attributedRunsFullyCoverBuffer(as.runs, n)) {
                as.runs.clear();
                as.runs.push_back(AttributedRun {0, n, defFont, rs.textColor});
                memo.runs = as.runs;
            }
        }
    } else {
        as.runs.push_back(AttributedRun {0, static_cast<std::uint32_t>(val.size()), defFont, rs.textColor});
    }
    return as;
}

int verticalMove(TextAreaSnap const &snap, std::string const &buf, int currentByte, int direction) {
    if (!snap.layout || snap.lines.empty()) {
        return currentByte;
    }
    TextLayout const &tl = *snap.layout;
    int const li = detail::lineIndexForByte(snap.lines, currentByte);
    int const n = static_cast<int>(snap.lines.size());
    int const targetIdx = std::clamp(li + direction, 0, n - 1);
    if (targetIdx == li) {
        return currentByte;
    }
    auto const &srcLine = snap.lines[static_cast<std::size_t>(li)];
    auto const &dstLine = snap.lines[static_cast<std::size_t>(targetIdx)];
    float const x = detail::caretXForByte(tl, srcLine, currentByte);
    int const out = detail::caretByteAtX(tl, dstLine, x, buf);
    return out;
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
        auto const &L = lines[static_cast<std::size_t>(i)];
        if (layoutY >= L.top && layoutY < L.bottom) {
            return {i, false};
        }
    }
    float bestD = 1e9f;
    int best = 0;
    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        float const mid = (lines[static_cast<std::size_t>(i)].top + lines[static_cast<std::size_t>(i)].bottom) * 0.5f;
        float const d = std::abs(layoutY - mid);
        if (d < bestD) {
            bestD = d;
            best = i;
        }
    }
    return {best, true};
}

int hitTestByte(TextEditBehavior &beh, ResolvedTextAreaStyle const &rs, std::string const &placeholder,
                std::function<std::vector<AttributedRun>(std::string_view)> const &styler,
                StylerMemo &stylerMemo, Font const &defaultFont, bool focused, TextAreaSnap &snap,
                float frameW, Point local, float scrollY) {
    std::string const &buf = beh.value();
    if (buf.empty()) {
        return 0;
    }
    bool const showPh = buf.empty() && !focused;
    float const contentW = std::max(1.f, frameW - 2.f * (rs.borderWidth + rs.paddingH));
    // Hit-test must not rebuild layout on pointer vs render width mismatch: `render()` lays out every
    // paint and updates `snap` for the current frame width. Comparing `useLayoutRect().width` to the last
    // paint's `frame.width` caused relayout thrash on pointer moves (OOM/crash on large paste). Invalidate
    // only when the text buffer changed vs the cached layout or the cache is empty.
    bool const needRelayout = !snap.layout || snap.lines.empty() || buf != snap.layoutSource;
    if (needRelayout) {
        TextSystem &ts = Application::instance().textSystem();
        AttributedString text =
            buildAttributedString(placeholder, styler, rs, defaultFont, buf, showPh, stylerMemo);
        TextLayoutOptions const opts = text_detail::makeTextLayoutOptions(TextWrapping::Wrap, rs.lineHeight);
        auto layout = ts.layout(text, contentW, opts);
        if (!layout) {
            return 0;
        }
        snap.layout = layout;
        snap.lines = detail::buildLineMetrics(*layout);
        snap.layoutSource = buf;
        snap.layoutFrameW = frameW;
        snap.layoutContentW = contentW;
    }
    float const innerPad = rs.borderWidth + rs.paddingH;
    float const layoutY = local.y - rs.borderWidth - rs.paddingV + scrollY;
    auto const [li, yFallback] = lineIndexAtYWithFallback(snap.lines, layoutY);
    detail::LineMetrics const &line = snap.lines[static_cast<std::size_t>(li)];
    float const lx = local.x - innerPad;
    int const ret = detail::caretByteAtX(*snap.layout, line, lx, buf);
    return ret;
}

struct TextAreaView {
    /// Copied from \c TextArea in \c body() — the view outlives the temporary component value.
    std::string placeholder;
    std::function<std::vector<AttributedRun>(std::string_view)> styler;
    TextEditBehavior *behavior = nullptr;
    TextAreaSnap *snap = nullptr;
    StylerMemo *stylerMemo = nullptr;
    /// Copy of \c State handle — must not be a pointer to a stack \c State local.
    State<float> scrollY {};
    ResolvedTextAreaStyle rs {};
    Font defaultFont {};
    float fixedHeight = 0.f;
    float minIntrinsic = 80.f;
    float maxIntrinsic = 0.f;
    bool disabled = false;
    bool focused = false;

    void render(Canvas &canvas, Rect frame) const {
        TextSystem &ts = Application::instance().textSystem();
        std::string const &buf = behavior->value();
        bool const showPh = buf.empty() && !focused;
        AttributedString text =
            buildAttributedString(placeholder, styler, rs, defaultFont, buf, showPh, *stylerMemo);

        StrokeStyle const stroke =
            focused ? StrokeStyle::solid(rs.borderFocusColor, rs.borderFocusWidth) : StrokeStyle::solid(rs.borderColor, rs.borderWidth);
        canvas.drawRect(frame, CornerRadius {rs.cornerRadius}, FillStyle::solid(rs.backgroundColor), stroke);

        float const innerLeft = frame.x + rs.borderWidth + rs.paddingH;
        float const innerTop = frame.y + rs.borderWidth + rs.paddingV;
        float const contentW =
            std::max(1.f, frame.width - 2.f * (rs.borderWidth + rs.paddingH));
        float const contentH =
            std::max(1.f, frame.height - 2.f * (rs.borderWidth + rs.paddingV));

        TextLayoutOptions const opts = text_detail::makeTextLayoutOptions(TextWrapping::Wrap, rs.lineHeight);

        auto layout = ts.layout(text, contentW, opts);
        if (!layout) {
            return;
        }
        snap->layout = layout;
        snap->lines = detail::buildLineMetrics(*layout);
        snap->layoutSource = buf;
        snap->layoutFrameW = frame.width;
        snap->layoutContentW = contentW;

        float const maxScroll = std::max(0.f, layout->measuredSize.height - contentH);
        float sy = std::clamp(*scrollY, 0.f, maxScroll);
        if (sy != *scrollY) {
            scrollY = sy;
        }
        if (behavior->consumeEnsureCaretVisibleRequest() && !snap->lines.empty()) {
            int const li = detail::lineIndexForByte(snap->lines, behavior->caretByte());
            auto const &L = snap->lines[static_cast<std::size_t>(li)];
            float const carety = L.top;
            float const caretb = L.bottom;
            if (carety < sy + kCaretScrollMarginPx) {
                sy = carety - kCaretScrollMarginPx;
            } else if (caretb > sy + contentH - kCaretScrollMarginPx) {
                sy = caretb - contentH + kCaretScrollMarginPx;
            }
            sy = std::clamp(sy, 0.f, maxScroll);
            scrollY = sy;
        }

        canvas.save();
        canvas.clipRect(Rect {innerLeft - 1.f, innerTop, contentW + 2.f, contentH});
        // Scroll the text by offsetting the layout origin instead of `translate` after `clipRect`.
        // Combining clip + translate updates the clip via `boundsOfTransformedRect` and can incorrectly
        // cull primitives (e.g. caret) at the left edge of the content box.
        float const scrollOffsetY = innerTop - sy;
        Point textOrigin {innerLeft, scrollOffsetY};

        if (!showPh && behavior->hasSelection()) {
            auto [s0, s1] = behavior->orderedSelection();
            s0 = detail::utf8Clamp(buf, s0);
            s1 = detail::utf8Clamp(buf, s1);
            for (auto const &line : snap->lines) {
                int const a = std::max(s0, line.byteStart);
                int const b = std::min(s1, line.byteEnd);
                if (a >= b) {
                    continue;
                }
                float x0 = detail::caretXForByte(*layout, line, a) + innerLeft;
                float x1 = detail::caretXForByte(*layout, line, b) + innerLeft;
                if (x0 > x1) {
                    std::swap(x0, x1);
                }
                float const yTop = scrollOffsetY + line.top;
                float const yBot = scrollOffsetY + line.bottom;
                canvas.drawRect(Rect {x0, yTop, x1 - x0, yBot - yTop + kSelectionExtraBottomPx}, CornerRadius {},
                                FillStyle::solid(rs.selectionColor), StrokeStyle::none());
            }
        }

        canvas.drawTextLayout(*layout, textOrigin);

        if (focused && !disabled && !showPh && !snap->lines.empty()) {
            int const li = detail::lineIndexForByte(snap->lines, behavior->caretByte());
            auto const &line = snap->lines[static_cast<std::size_t>(li)];
            float const cx = innerLeft + detail::caretXForByte(*layout, line, behavior->caretByte());
            auto const [caretY0, caretY1] = detail::lineCaretYRangeInLayout(*layout, line);
            float const phase = behavior->caretBlinkPhase();
            float const alpha = phase <= 0.5f ? 1.f : 0.f;
            Color cc = rs.caretColor;
            cc.a *= alpha;
            canvas.drawLine(Point {cx, scrollOffsetY + caretY0}, Point {cx, scrollOffsetY + caretY1},
                            StrokeStyle::solid(cc, detail::kTextCaretStrokeWidthPx));
        }

        canvas.restore();

        if (disabled) {
            Color dc = rs.disabledColor;
            dc.a *= 0.35f;
            canvas.drawRect(frame, CornerRadius {rs.cornerRadius}, FillStyle::solid(dc), StrokeStyle::none());
        }
    }

    Size measure(LayoutConstraints const &cs, LayoutHints const &) const {
        float w = 200.f;
        if (std::isfinite(cs.maxWidth) && cs.maxWidth > 0.f) {
            w = cs.maxWidth;
        }
        float h = fixedHeight;
        if (h <= 0.f) {
            h = minIntrinsic;
            if (maxIntrinsic > 0.f) {
                h = std::min(h, maxIntrinsic);
            }
        }
        return {w, h};
    }
};

} // namespace

Element TextArea::body() const {
    Theme const &theme = useEnvironment<Theme>();
    ResolvedTextAreaStyle const rs = resolveTextAreaStyle(style, theme);
    Font const defaultFont = text_detail::resolveBodyTextStyle(style.font, kColorFromTheme).first;

    TextAreaSnap &snap = StateStore::current()->claimSlot<TextAreaSnap>();
    StylerMemo &stylerMemo = StateStore::current()->claimSlot<StylerMemo>();

    State<float> scrollY = useState(0.f);
    std::optional<Rect> layoutRect = useLayoutRect();

    auto &beh = useTextEditBehavior(value, {.multiline = true,
                                            .maxLength = maxLength,
                                            .acceptsTab = true,
                                            .submitsOnEnter = false,
                                            .onChange = onChange,
                                            .onSubmit = nullptr,
                                            .onEscape = onEscape,
                                            .verticalResolver = [&snap, st = value](int cur, int dir) {
                                                return verticalMove(snap, *st, cur, dir);
                                            }});
    beh.setDisabled(disabled);

    bool const focused = useFocus();
    beh.setFocused(focused);

    TextAreaView view {};
    view.placeholder = placeholder;
    view.styler = styler;
    view.behavior = &beh;
    view.snap = &snap;
    view.stylerMemo = &stylerMemo;
    view.scrollY = scrollY;
    view.rs = rs;
    view.defaultFont = defaultFont;
    view.fixedHeight = height.fixed;
    view.minIntrinsic = height.minIntrinsic;
    view.maxIntrinsic = height.maxIntrinsic;
    view.disabled = disabled;
    view.focused = focused;

    return Element {view}
        .focusable(!disabled)
        .cursor(Cursor::IBeam)
        .onKeyDown([&beh](KeyCode k, Modifiers m) { beh.handleKey(KeyEvent {k, m}); })
        .onTextInput([&beh](std::string const &t) { beh.handleTextInput(t); })
        .onPointerDown([this, &beh, &snap, rs, &stylerMemo, defaultFont, focused, scrollY, layoutRect](
                           Point local
                       ) {
            float const fw = layoutRect ? layoutRect->width : 400.f;
            float const fh = layoutRect ? layoutRect->height : 200.f;
            float const contentH = std::max(1.f, fh - 2.f * (rs.borderWidth + rs.paddingV));
            float sy = *scrollY;
            if (snap.layout) {
                float const maxScroll = std::max(0.f, snap.layout->measuredSize.height - contentH);
                sy = std::clamp(sy, 0.f, maxScroll);
                if (sy != *scrollY) {
                    scrollY = sy;
                }
            }
            int const byte = hitTestByte(beh, rs, placeholder, styler, stylerMemo, defaultFont, focused, snap, fw,
                                         local, sy);
            beh.handlePointerDown(byte, false);
        })
        .onPointerMove([this, &beh, &snap, rs, &stylerMemo, defaultFont, focused, scrollY, layoutRect](
                           Point local
                       ) {
            float const fw = layoutRect ? layoutRect->width : 400.f;
            float const fh = layoutRect ? layoutRect->height : 200.f;
            float const contentH = std::max(1.f, fh - 2.f * (rs.borderWidth + rs.paddingV));
            float sy = *scrollY;
            if (snap.layout) {
                float const maxScroll = std::max(0.f, snap.layout->measuredSize.height - contentH);
                sy = std::clamp(sy, 0.f, maxScroll);
                if (sy != *scrollY) {
                    scrollY = sy;
                }
            }
            int const byte = hitTestByte(beh, rs, placeholder, styler, stylerMemo, defaultFont, focused, snap, fw,
                                         local, sy);
            beh.handlePointerDrag(byte);
        })
        .onPointerUp([&beh](Point) { beh.handlePointerUp(); })
        .onScroll([scrollY, &snap, layoutRect, rs](Vec2 delta) {
            if (!snap.layout) {
                return;
            }
            float const h = layoutRect ? layoutRect->height : 200.f;
            float const contentH = std::max(1.f, h - 2.f * (rs.borderWidth + rs.paddingV));
            float const maxScroll = std::max(0.f, snap.layout->measuredSize.height - contentH);
            float sy = *scrollY;
            sy -= delta.y;
            sy = std::clamp(sy, 0.f, maxScroll);
            scrollY = sy;
        });
}

} // namespace flux
