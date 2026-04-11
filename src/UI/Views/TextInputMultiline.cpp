#include "UI/Views/TextInputMultiline.hpp"

#include <Flux/Core/Application.hpp>
#include <Flux/Graphics/Font.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/TextEditBehavior.hpp>
#include <Flux/UI/Views/TextEditUtils.hpp>

#include "UI/Views/TextSupport.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>

namespace flux::text_input_detail {

namespace {

constexpr float kSelectionExtraBottomPx = 4.f;
constexpr float kCaretScrollMarginPx = 8.f;

bool attributedRunsFullyCoverBuffer(std::vector<AttributedRun> const& runs, std::uint32_t n) {
    if (n == 0) {
        return true;
    }
    std::vector<std::pair<std::uint32_t, std::uint32_t>> ranges;
    ranges.reserve(runs.size());
    for (auto const& run : runs) {
        if (run.start >= run.end || run.end > n) {
            return false;
        }
        ranges.push_back({run.start, run.end});
    }
    std::sort(ranges.begin(), ranges.end());
    std::uint32_t pos = 0;
    for (auto const& [start, end] : ranges) {
        if (start > pos) {
            return false;
        }
        pos = std::max(pos, end);
    }
    return pos >= n;
}

struct TextInputMultilineSnap {
    detail::TextEditLayoutResult layoutResult;
    std::string layoutSource;
    float layoutFrameW = 0.f;
    float layoutContentW = 0.f;
};

struct StylerMemo {
    std::string value;
    std::vector<AttributedRun> runs;
};

struct ResolvedMultilineStyle {
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

ResolvedMultilineStyle resolveStyle(TextInput::Style const& style, Theme const& theme) {
    return ResolvedMultilineStyle {
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

AttributedString buildAttributedString(std::string const& placeholderText,
                                       std::function<std::vector<AttributedRun>(std::string_view)> const& styler,
                                       ResolvedMultilineStyle const& rs, Font const& defaultFont,
                                       std::string const& value, bool showPlaceholder, StylerMemo& memo) {
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

int verticalMove(TextInputMultilineSnap const& snap, std::string const& buf, int currentByte, int direction) {
    if (snap.layoutResult.empty() || snap.layoutResult.lines.empty()) {
        return currentByte;
    }
    TextLayout const& layout = *snap.layoutResult.layout;
    int const srcIndex = detail::lineIndexForByte(snap.layoutResult.lines, currentByte);
    int const targetIndex =
        std::clamp(srcIndex + direction, 0, static_cast<int>(snap.layoutResult.lines.size()) - 1);
    if (srcIndex == targetIndex) {
        return currentByte;
    }

    auto const& srcLine = snap.layoutResult.lines[static_cast<std::size_t>(srcIndex)];
    auto const& dstLine = snap.layoutResult.lines[static_cast<std::size_t>(targetIndex)];
    float const x = detail::caretXForByte(layout, srcLine, currentByte);
    return detail::caretByteAtX(layout, dstLine, x, buf);
}

std::pair<int, bool> lineIndexAtYWithFallback(std::vector<detail::LineMetrics> const& lines, float layoutY) {
    if (lines.empty()) {
        return {0, false};
    }
    auto const& first = lines.front();
    auto const& last = lines.back();
    if (layoutY < first.top) {
        return {0, true};
    }
    if (layoutY >= last.bottom) {
        return {static_cast<int>(lines.size()) - 1, true};
    }
    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        auto const& line = lines[static_cast<std::size_t>(i)];
        if (layoutY >= line.top && layoutY < line.bottom) {
            return {i, false};
        }
    }
    float bestDistance = 1e9f;
    int bestIndex = 0;
    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        auto const& line = lines[static_cast<std::size_t>(i)];
        float const mid = (line.top + line.bottom) * 0.5f;
        float const distance = std::abs(layoutY - mid);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestIndex = i;
        }
    }
    return {bestIndex, true};
}

int hitTestByte(TextEditBehavior& behavior, ResolvedMultilineStyle const& rs, std::string const& placeholder,
                std::function<std::vector<AttributedRun>(std::string_view)> const& styler, StylerMemo& stylerMemo,
                Font const& defaultFont, bool focused, TextInputMultilineSnap& snap, float frameWidth, Point local,
                float scrollY) {
    std::string const& buf = behavior.value();
    if (buf.empty()) {
        return 0;
    }

    bool const showPlaceholder = buf.empty() && !focused;
    float const contentW = std::max(1.f, frameWidth - 2.f * (rs.borderWidth + rs.paddingH));
    bool const needRelayout =
        snap.layoutResult.empty() || snap.layoutResult.lines.empty() || buf != snap.layoutSource;
    if (needRelayout) {
        TextSystem& ts = Application::instance().textSystem();
        AttributedString text =
            buildAttributedString(placeholder, styler, rs, defaultFont, buf, showPlaceholder, stylerMemo);
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
    detail::LineMetrics const& line = snap.layoutResult.lines[static_cast<std::size_t>(lineIndex)];
    float const layoutX = local.x - rs.borderWidth - rs.paddingH;
    return detail::caretByteAtX(*snap.layoutResult.layout, line, layoutX, buf);
}

struct MultilineTextInputView {
    std::string placeholder;
    std::function<std::vector<AttributedRun>(std::string_view)> styler;
    TextEditBehavior* behavior = nullptr;
    TextInputMultilineSnap* snap = nullptr;
    StylerMemo* stylerMemo = nullptr;
    State<float> scrollY {};
    ResolvedMultilineStyle rs {};
    Font defaultFont {};
    TextInputHeight height {};
    bool disabled = false;
    bool focused = false;

    void render(Canvas& canvas, Rect frame) const {
        TextSystem& ts = Application::instance().textSystem();
        std::string const& buf = behavior->value();
        bool const showPlaceholder = buf.empty() && !focused;
        AttributedString text =
            buildAttributedString(placeholder, styler, rs, defaultFont, buf, showPlaceholder, *stylerMemo);

        StrokeStyle const stroke = focused
                                       ? StrokeStyle::solid(rs.borderFocusColor, rs.borderFocusWidth)
                                       : StrokeStyle::solid(rs.borderColor, rs.borderWidth);
        canvas.drawRect(frame, CornerRadius {rs.cornerRadius}, FillStyle::solid(rs.backgroundColor), stroke);

        float const innerLeft = frame.x + rs.borderWidth + rs.paddingH;
        float const innerTop = frame.y + rs.borderWidth + rs.paddingV;
        float const contentW = std::max(1.f, frame.width - 2.f * (rs.borderWidth + rs.paddingH));
        float const contentH = std::max(1.f, frame.height - 2.f * (rs.borderWidth + rs.paddingV));

        TextLayoutOptions const opts = text_detail::makeTextLayoutOptions(TextWrapping::Wrap, rs.lineHeight);
        auto layout = ts.layout(text, contentW, opts);
        if (!layout) {
            return;
        }

        snap->layoutResult = detail::makeTextEditLayoutResult(layout, static_cast<int>(text.utf8.size()), contentW);
        snap->layoutSource = buf;
        snap->layoutFrameW = frame.width;
        snap->layoutContentW = contentW;

        float const maxScroll = std::max(0.f, snap->layoutResult.layout->measuredSize.height - contentH);
        float scroll = std::clamp(*scrollY, 0.f, maxScroll);
        if (scroll != *scrollY) {
            scrollY = scroll;
        }
        if (behavior->consumeEnsureCaretVisibleRequest() && !snap->layoutResult.lines.empty()) {
            int const lineIndex = detail::lineIndexForByte(snap->layoutResult.lines, behavior->caretByte());
            auto const& line = snap->layoutResult.lines[static_cast<std::size_t>(lineIndex)];
            if (line.top < scroll + kCaretScrollMarginPx) {
                scroll = line.top - kCaretScrollMarginPx;
            } else if (line.bottom > scroll + contentH - kCaretScrollMarginPx) {
                scroll = line.bottom - contentH + kCaretScrollMarginPx;
            }
            scroll = std::clamp(scroll, 0.f, maxScroll);
            scrollY = scroll;
        }

        canvas.save();
        canvas.clipRect(Rect {innerLeft - 1.f, innerTop, contentW + 2.f, contentH});
        float const scrollOffsetY = innerTop - scroll;
        Point textOrigin {innerLeft, scrollOffsetY};

        if (!showPlaceholder && behavior->hasSelection()) {
            detail::TextEditSelection const selection {
                .caretByte = detail::utf8Clamp(buf, behavior->caretByte()),
                .anchorByte = detail::utf8Clamp(buf, behavior->selectionAnchorByte()),
            };
            for (Rect const& rect :
                 detail::selectionRects(snap->layoutResult, selection, innerLeft, scrollOffsetY, kSelectionExtraBottomPx)) {
                canvas.drawRect(rect, CornerRadius {}, FillStyle::solid(rs.selectionColor), StrokeStyle::none());
            }
        }

        canvas.drawTextLayout(*snap->layoutResult.layout, textOrigin);

        if (focused && !disabled && !showPlaceholder && !snap->layoutResult.lines.empty()) {
            int const lineIndex = detail::lineIndexForByte(snap->layoutResult.lines, behavior->caretByte());
            auto const& line = snap->layoutResult.lines[static_cast<std::size_t>(lineIndex)];
            float const caretX =
                innerLeft + detail::caretXForByte(*snap->layoutResult.layout, line, behavior->caretByte());
            auto const [caretY0, caretY1] = detail::lineCaretYRangeInLayout(*snap->layoutResult.layout, line);
            Color caretColor = rs.caretColor;
            caretColor.a *= behavior->caretBlinkPhase() <= 0.5f ? 1.f : 0.f;
            canvas.drawLine(Point {caretX, scrollOffsetY + caretY0}, Point {caretX, scrollOffsetY + caretY1},
                            StrokeStyle::solid(caretColor, detail::kTextCaretStrokeWidthPx));
        }

        canvas.restore();

        if (disabled) {
            Color overlay = rs.disabledColor;
            overlay.a *= 0.35f;
            canvas.drawRect(frame, CornerRadius {rs.cornerRadius}, FillStyle::solid(overlay), StrokeStyle::none());
        }
    }

    Size measure(LayoutConstraints const& cs, LayoutHints const&) const {
        float width = 200.f;
        if (std::isfinite(cs.maxWidth) && cs.maxWidth > 0.f) {
            width = cs.maxWidth;
        }
        float resolvedHeight = height.fixed;
        if (resolvedHeight <= 0.f) {
            resolvedHeight = height.minIntrinsic;
            if (height.maxIntrinsic > 0.f) {
                resolvedHeight = std::min(resolvedHeight, height.maxIntrinsic);
            }
        }
        return {width, resolvedHeight};
    }
};

} // namespace

Element buildMultilineTextInput(TextInput const& input) {
    Theme const& theme = useEnvironment<Theme>();
    ResolvedMultilineStyle const rs = resolveStyle(input.style, theme);
    Font const defaultFont = text_detail::resolveBodyTextStyle(input.style.font, kColorFromTheme).first;

    TextInputMultilineSnap& snap = StateStore::current()->claimSlot<TextInputMultilineSnap>();
    StylerMemo& stylerMemo = StateStore::current()->claimSlot<StylerMemo>();

    State<float> scrollY = useState(0.f);
    std::optional<Rect> layoutRect = useLayoutRect();

    auto& behavior = useTextEditBehavior(input.value, {.multiline = true,
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
    view.scrollY = scrollY;
    view.rs = rs;
    view.defaultFont = defaultFont;
    view.height = input.multilineHeight;
    view.disabled = input.disabled;
    view.focused = focused;

    return Element {view}
        .focusable(!input.disabled)
        .cursor(Cursor::IBeam)
        .onKeyDown([&behavior](KeyCode key, Modifiers mods) { behavior.handleKey(KeyEvent {key, mods}); })
        .onTextInput([&behavior](std::string const& text) { behavior.handleTextInput(text); })
        .onPointerDown([placeholder = input.placeholder, styler = input.styler, &behavior, &snap, rs, &stylerMemo,
                        defaultFont, focused, scrollY, layoutRect](Point local) {
            float const frameWidth = layoutRect ? layoutRect->width : 400.f;
            float const frameHeight = layoutRect ? layoutRect->height : 200.f;
            float const contentH = std::max(1.f, frameHeight - 2.f * (rs.borderWidth + rs.paddingV));
            float scroll = *scrollY;
            if (!snap.layoutResult.empty()) {
                float const maxScroll = std::max(0.f, snap.layoutResult.layout->measuredSize.height - contentH);
                scroll = std::clamp(scroll, 0.f, maxScroll);
                if (scroll != *scrollY) {
                    scrollY = scroll;
                }
            }
            int const byte =
                hitTestByte(behavior, rs, placeholder, styler, stylerMemo, defaultFont, focused, snap,
                            frameWidth, local, scroll);
            behavior.handlePointerDown(byte, false);
        })
        .onPointerMove([placeholder = input.placeholder, styler = input.styler, &behavior, &snap, rs, &stylerMemo,
                        defaultFont, focused, scrollY, layoutRect](Point local) {
            float const frameWidth = layoutRect ? layoutRect->width : 400.f;
            float const frameHeight = layoutRect ? layoutRect->height : 200.f;
            float const contentH = std::max(1.f, frameHeight - 2.f * (rs.borderWidth + rs.paddingV));
            float scroll = *scrollY;
            if (!snap.layoutResult.empty()) {
                float const maxScroll = std::max(0.f, snap.layoutResult.layout->measuredSize.height - contentH);
                scroll = std::clamp(scroll, 0.f, maxScroll);
                if (scroll != *scrollY) {
                    scrollY = scroll;
                }
            }
            int const byte =
                hitTestByte(behavior, rs, placeholder, styler, stylerMemo, defaultFont, focused, snap,
                            frameWidth, local, scroll);
            behavior.handlePointerDrag(byte);
        })
        .onPointerUp([&behavior](Point) { behavior.handlePointerUp(); })
        .onScroll([scrollY, &snap, layoutRect, rs](Vec2 delta) {
            if (snap.layoutResult.empty()) {
                return;
            }
            float const frameHeight = layoutRect ? layoutRect->height : 200.f;
            float const contentH = std::max(1.f, frameHeight - 2.f * (rs.borderWidth + rs.paddingV));
            float const maxScroll = std::max(0.f, snap.layoutResult.layout->measuredSize.height - contentH);
            float scroll = std::clamp(*scrollY - delta.y, 0.f, maxScroll);
            scrollY = scroll;
        });
}

} // namespace flux::text_input_detail
