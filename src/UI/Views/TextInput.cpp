#include <Flux/UI/Views/TextInput.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/UI/InputFieldLayout.hpp>
#include <Flux/UI/LayoutContext.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Render.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/TextEditBehavior.hpp>
#include <Flux/UI/Views/TextEditUtils.hpp>

#include <Flux/Graphics/Font.hpp>
#include <Flux/Scene/Nodes.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ScrollView.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <Flux/UI/Views/TextSupport.hpp>

#include <algorithm>
#include <cstdio>
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

struct PointerClickTracker {
    Point lastPoint {};
    std::chrono::steady_clock::time_point lastClickAt {};
    int clickCount = 0;
};

int registerPointerClick(PointerClickTracker &tracker, Point local) {
    auto const now = std::chrono::steady_clock::now();
    float const dx = local.x - tracker.lastPoint.x;
    float const dy = local.y - tracker.lastPoint.y;
    float const dist2 = dx * dx + dy * dy;
    bool const chained = tracker.clickCount > 0 &&
                         now - tracker.lastClickAt <= std::chrono::milliseconds(500) &&
                         dist2 <= 36.f;
    tracker.clickCount = chained ? tracker.clickCount + 1 : 1;
    tracker.lastClickAt = now;
    tracker.lastPoint = local;
    return tracker.clickCount;
}

/// Same rule as multiline TextInput: styler runs must cover every UTF-8 byte without gaps.
bool attributedRunsFullyCoverBuffer(std::vector<AttributedRun> const &runs, std::uint32_t n) {
    std::vector<std::pair<std::uint32_t, std::uint32_t>> se;
    se.reserve(runs.size());
    for (auto const &r : runs) {
        if (r.start >= r.end || r.end > n) {
            return false;
        }
        se.push_back({r.start, r.end});
    }
    if (n == 0) {
        return se.empty();
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
    float lineHeight = 0.f;
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
        .lineHeight = resolveFloat(style.lineHeight, 0.f),
    };
}

struct TextInputLayoutSnap {
    detail::TextEditLayoutResult layoutResult;
    std::string layoutSource;
    float layoutContentW = 0.f;
    bool showingPlaceholder = false;
    Font layoutFont {};
    Color layoutTextColor {};
    Color layoutPlaceholderColor {};
    TextWrapping layoutWrapping = TextWrapping::NoWrap;
    float layoutLineHeight = 0.f;
    bool layoutUsedStyler = false;
};

struct TextInputStylerMemo {
    std::string value;
    std::vector<AttributedRun> runs;
};

AttributedString buildAttributedString(std::string const &placeholderText,
                                       std::function<std::vector<AttributedRun>(std::string_view)> const &styler,
                                       std::function<Color(std::string_view)> const &validationColor,
                                       ResolvedTextInputStyle const &rs, Font const &defaultFont,
                                       std::string const &val, bool showPlaceholder,
                                       TextInputStylerMemo *memo = nullptr) {
    if (showPlaceholder) {
        AttributedString ph;
        ph.utf8 = placeholderText;
        if (!placeholderText.empty()) {
            ph.runs.push_back(AttributedRun {
                .start = 0,
                .end = static_cast<std::uint32_t>(placeholderText.size()),
                .font = defaultFont,
                .color = rs.placeholderColor
            });
        }
        return ph;
    }
    AttributedString as;
    as.utf8 = val;
    std::uint32_t const n = static_cast<std::uint32_t>(val.size());
    if (n == 0) {
        if (memo) {
            memo->value = val;
            memo->runs.clear();
        }
        return as;
    }
    if (styler) {
        if (memo && memo->value == val && !memo->runs.empty() && attributedRunsFullyCoverBuffer(memo->runs, n)) {
            as.runs = memo->runs;
        } else {
            as.runs = styler(val);
            if (memo) {
                memo->value = val;
                memo->runs = as.runs;
            }
        }
        if (as.runs.empty() || !attributedRunsFullyCoverBuffer(as.runs, n)) {
            as.runs.clear();
            as.runs.push_back(
                AttributedRun {.start = 0, .end = n, .font = defaultFont, .color = rs.textColor}
            );
            if (memo) {
                memo->runs = as.runs;
            }
        }
    } else {
        Color const c = validationColor ? validationColor(val) : rs.textColor;
        as.runs.push_back(AttributedRun {.start = 0, .end = static_cast<std::uint32_t>(val.size()), .font = defaultFont, .color = c});
    }
    return as;
}

bool ensureLayout(TextInputLayoutSnap &snap, std::string const &placeholderText,
                  std::function<std::vector<AttributedRun>(std::string_view)> const &styler,
                  std::function<Color(std::string_view)> const &validationColor, ResolvedTextInputStyle const &rs,
                  Font const &defaultFont, std::string const &value, float contentW, bool showPlaceholder,
                  TextWrapping wrapping, TextInputStylerMemo *memo = nullptr) {
    TextSystem &ts = Application::instance().textSystem();
    std::string const &layoutSource = showPlaceholder ? placeholderText : value;
    Color const layoutTextColor =
        showPlaceholder ? rs.placeholderColor : (validationColor ? validationColor(value) : rs.textColor);
    bool const styleChanged =
        snap.layoutFont.family != defaultFont.family || snap.layoutFont.size != defaultFont.size ||
        snap.layoutFont.weight != defaultFont.weight || snap.layoutFont.italic != defaultFont.italic ||
        snap.layoutTextColor != layoutTextColor || snap.layoutPlaceholderColor != rs.placeholderColor ||
        snap.layoutWrapping != wrapping || snap.layoutLineHeight != rs.lineHeight ||
        snap.layoutUsedStyler != static_cast<bool>(styler);
    bool const widthChanged = std::abs(snap.layoutContentW - contentW) > 0.5f;
    bool const needRelayout =
        snap.layoutResult.empty() || widthChanged || styleChanged || snap.layoutSource != layoutSource ||
        snap.showingPlaceholder != showPlaceholder;
    if (!needRelayout) {
        return !snap.layoutResult.empty();
    }

    AttributedString text =
        buildAttributedString(placeholderText, styler, validationColor, rs, defaultFont, value, showPlaceholder, memo);
    TextLayoutOptions const opts = text_detail::makeTextLayoutOptions(wrapping, rs.lineHeight);
    auto layout = ts.layout(text, contentW, opts);
    snap.layoutResult = detail::makeTextEditLayoutResult(layout, static_cast<int>(text.utf8.size()), contentW);
    snap.layoutSource = text.utf8;
    snap.layoutContentW = contentW;
    snap.showingPlaceholder = showPlaceholder;
    snap.layoutFont = defaultFont;
    snap.layoutTextColor = layoutTextColor;
    snap.layoutPlaceholderColor = rs.placeholderColor;
    snap.layoutWrapping = wrapping;
    snap.layoutLineHeight = rs.lineHeight;
    snap.layoutUsedStyler = static_cast<bool>(styler);
    return !snap.layoutResult.empty();
}

float intrinsicSingleLineContentWidth(std::string const &placeholderText,
                                      std::function<std::vector<AttributedRun>(std::string_view)> const &styler,
                                      std::function<Color(std::string_view)> const &validationColor,
                                      ResolvedTextInputStyle const &rs, Font const &defaultFont,
                                      std::string const &value, bool showPlaceholder,
                                      TextInputStylerMemo *memo = nullptr) {
    TextSystem &ts = Application::instance().textSystem();
    TextLayoutOptions const opts = text_detail::makeTextLayoutOptions(TextWrapping::NoWrap, rs.lineHeight);
    auto measureWidth = [&](bool placeholderVisible) {
        AttributedString text =
            buildAttributedString(placeholderText, styler, validationColor, rs, defaultFont, value, placeholderVisible, memo);
        if (text.utf8.empty()) {
            text.utf8 = " ";
            text.runs.clear();
            text.runs.push_back(AttributedRun {
                .start = 0,
                .end = 1,
                .font = defaultFont,
                .color = placeholderVisible ? rs.placeholderColor : rs.textColor,
            });
        }
        Size const measured = ts.measure(text, 0.f, opts);
        return std::max(1.f, measured.width);
    };

    float width = measureWidth(showPlaceholder);
    if (!placeholderText.empty()) {
        width = std::max(width, measureWidth(true));
    }
    return width;
}

StrokeStyle inputBorderStroke(ResolvedTextInputStyle const &rs, bool focused) {
    return focused ? StrokeStyle::solid(rs.borderFocusColor, rs.borderFocusWidth) : StrokeStyle::solid(rs.borderColor, rs.borderWidth);
}

float inputShellInset(ResolvedTextInputStyle const &rs) {
    return std::max(rs.borderWidth, rs.borderFocusWidth);
}

Element decorateInputField(Element field, ResolvedTextInputStyle const &rs, bool focused, bool disabled) {
    Element shell = Element {ZStack {
                                 .horizontalAlignment = Alignment::Start,
                                 .verticalAlignment = Alignment::Start,
                                 .children = children(std::move(field)),
                             }}
                        .fill(FillStyle::solid(rs.backgroundColor))
                        .stroke(inputBorderStroke(rs, focused))
                        .cornerRadius(CornerRadius {rs.cornerRadius});
    if (disabled) {
        Color overlay = rs.disabledColor;
        overlay.a *= 0.35f;
        shell = std::move(shell).overlay(Rectangle {}.fill(FillStyle::solid(overlay)));
    }
    return shell;
}

std::vector<Element> makeSelectionAndCaretElements(detail::TextEditLayoutResult const &layoutResult,
                                                   detail::TextEditSelection const *selection, int caretByte,
                                                   bool showCaret, std::string const *text, Point origin,
                                                   ResolvedTextInputStyle const &rs,
                                                   float blinkPhase) {
    std::vector<Element> out;
    if (selection && selection->hasSelection()) {
        for (Rect const &rect :
             detail::selectionRects(layoutResult, *selection, text, origin.x, origin.y,
                                    kSelectionExtraBottomPx)) {
            out.push_back(Element {Rectangle {}}
                              .fill(FillStyle::solid(rs.selectionColor))
                              .size(rect.width, rect.height)
                              .position(rect.x, rect.y));
        }
    }
    if (showCaret) {
        Rect const caret = detail::caretRect(layoutResult, caretByte, origin.x, origin.y, detail::kTextCaretStrokeWidthPx);
        Color caretColor = rs.caretColor;
        caretColor.a *= blinkPhase <= 0.5f ? 1.f : 0.f;
        out.push_back(Element {Rectangle {}}
                          .fill(FillStyle::solid(caretColor))
                          .size(caret.width, caret.height)
                          .position(caret.x, caret.y));
    }
    return out;
}

detail::TextEditSelection behaviorSelection(TextEditBehavior const &behavior, std::string const &text) {
    return detail::TextEditSelection {
        .caretByte = detail::utf8Clamp(text, behavior.caretByte()),
        .anchorByte = detail::utf8Clamp(text, behavior.selectionAnchorByte()),
    };
}

Element buildTextInputContent(detail::TextEditLayoutResult const &layoutResult, detail::TextEditSelection const *selection,
                              int caretByte, bool showCaret, std::string const *text, Point origin, float contentWidth,
                              float containerWidth, float contentHeight, ResolvedTextInputStyle const &rs,
                              float blinkPhase) {
    std::vector<Element> layers;
    if (layoutResult.layout) {
        std::shared_ptr<TextLayout const> textLayout = layoutResult.layout;
        auto overlayChildren =
            makeSelectionAndCaretElements(layoutResult, selection, caretByte, showCaret, text, origin, rs, blinkPhase);
        layers.insert(layers.end(), std::make_move_iterator(overlayChildren.begin()),
                      std::make_move_iterator(overlayChildren.end()));
        layers.push_back(Element {Render {
                             .measureFn = [textLayout](LayoutConstraints const &, LayoutHints const &) {
                                 return textLayout ? textLayout->measuredSize : Size {};
                             },
                             .draw = [textLayout](Canvas &canvas, Rect) {
                                 if (textLayout) {
                                     canvas.drawTextLayout(*textLayout, Point {});
                                 }
                             },
                             .pure = true,
                         }}
                             .size(contentWidth, layoutResult.layout->measuredSize.height)
                             .position(origin.x, origin.y));
    }

    return Element {ZStack {
                        .horizontalAlignment = Alignment::Start,
                        .verticalAlignment = Alignment::Start,
                        .children = std::move(layers),
                    }}
        .size(containerWidth, contentHeight)
        .clipContent(true);
}

template <typename HitTest>
Element attachTextInputHandlers(
    Element editor,
    TextEditBehavior &behavior,
    PointerClickTracker &clickTracker,
    bool disabled,
    HitTest hitTest
) {
    if (disabled) {
        return std::move(editor)
            .focusable(false)
            .cursor(Cursor::Arrow);
    }

    return std::move(editor)
        .focusable(true)
        .cursor(Cursor::IBeam)
        .onKeyDown([&behavior](KeyCode key, Modifiers mods) { behavior.handleKey(KeyEvent {key, mods}); })
        .onTextInput([&behavior](std::string const &text) { behavior.handleTextInput(text); })
        .onPointerDown([&behavior, &clickTracker, hitTest = std::move(hitTest)](Point local) mutable {
            int const hitByte = hitTest(local);
            int const clickCount = registerPointerClick(clickTracker, local);
            if (clickCount >= 3) {
                behavior.selectAll();
                return;
            }
            if (clickCount == 2) {
                behavior.selectWordAt(hitByte);
                return;
            }
            behavior.handlePointerDown(hitByte, false);
        })
        .onPointerMove([&behavior, hitTest = std::move(hitTest)](Point local) mutable {
            behavior.handlePointerDrag(hitTest(local));
        })
        .onPointerUp([&behavior](Point) { behavior.handlePointerUp(); });
}

int hitTestSingleLineByte(detail::TextEditLayoutResult const &layoutResult, Point local, Point contentOrigin,
                          int scrollByte, std::string const &text) {
    if (layoutResult.lines.empty()) {
        return 0;
    }
    float const scrollX = detail::scrollOffsetXForByte(layoutResult, detail::utf8Clamp(text, scrollByte));
    return detail::caretByteAtViewportPoint(layoutResult, local, contentOrigin, Point {scrollX, 0.f}, text);
}

int hitTestMultilineByte(detail::TextEditLayoutResult const &layoutResult, Point local, Point contentOrigin,
                         float scrollY, std::string const &text) {
    if (layoutResult.lines.empty()) {
        return 0;
    }
    return detail::caretByteAtViewportPoint(layoutResult, local, contentOrigin, Point {0.f, scrollY}, text);
}

Element buildMultilineTextInput(TextInput const &input) {
    Theme const &theme = useEnvironment<Theme>();
    ResolvedTextInputStyle const resolved = resolveTextInputStyle(input.style, theme);
    Font const defaultFont = text_detail::resolveBodyTextStyle(input.style.font, kColorFromTheme).first;

    TextInputLayoutSnap &snap = StateStore::current()->claimSlot<TextInputLayoutSnap>();
    TextInputStylerMemo &stylerMemo = StateStore::current()->claimSlot<TextInputStylerMemo>();

    State<Point> scrollOffset = useState(Point {0.f, 0.f});
    State<Size> viewportSize = useState(Size {0.f, 0.f});
    State<Size> contentSize = useState(Size {0.f, 0.f});
    PointerClickTracker &clickTracker = StateStore::current()->claimSlot<PointerClickTracker>();
    Rect const bounds = useBounds();

    auto &behavior = useTextEditBehavior(input.value, {.multiline = true,
                                                       .maxLength = input.maxLength,
                                                       .acceptsTab = true,
                                                       .submitsOnEnter = static_cast<bool>(input.onSubmit),
                                                       .onChange = input.onChange,
                                                       .onSubmit = input.onSubmit,
                                                       .onEscape = input.onEscape,
                                                       .verticalResolver = [&snap, st = input.value](int cur, int dir) {
                                                           return detail::moveCaretVertically(snap.layoutResult, *st, cur, dir);
                                                       }});
    behavior.setDisabled(input.disabled);

    bool const focused = useFocus();
    behavior.setFocused(focused);
    float const frameWidth =
        bounds.width > 0.f ? bounds.width :
        (snap.layoutContentW > 0.f ? snap.layoutContentW + 2.f * resolved.paddingH : 400.f);
    float const contentW = std::max(1.f, frameWidth - 2.f * resolved.paddingH);
    std::string const &buf = behavior.value();
    bool const showPlaceholder = buf.empty() && !focused;
    ensureLayout(snap, input.placeholder, input.styler, nullptr, resolved, defaultFont, buf, contentW, showPlaceholder,
                 TextWrapping::Wrap, &stylerMemo);

    Point scroll = clampScrollOffset(ScrollAxis::Vertical, *scrollOffset, *viewportSize, *contentSize);
    if (behavior.consumeEnsureCaretVisibleRequest() && !snap.layoutResult.lines.empty()) {
        scroll.y = detail::scrollOffsetYToKeepCaretVisible(snap.layoutResult, scroll.y, (*viewportSize).height,
                                                           behavior.caretByte(), kCaretScrollMarginPx);
    }
    scroll = clampScrollOffset(ScrollAxis::Vertical, scroll, *viewportSize, *contentSize);
    if (scroll.x != (*scrollOffset).x || scroll.y != (*scrollOffset).y) {
        scrollOffset = scroll;
    }

    float const fieldHeight = input.multilineHeight.fixed > 0.f ? input.multilineHeight.fixed :
                              input.multilineHeight.minIntrinsic;
    float const contentHeight =
        (snap.layoutResult.layout ? snap.layoutResult.layout->measuredSize.height : 0.f) + 2.f * resolved.paddingV;
    float const editorHeight = std::max(contentHeight, fieldHeight);
    auto selection = behaviorSelection(behavior, buf);
    Point const contentOrigin {resolved.paddingH, resolved.paddingV};
    Element content = buildTextInputContent(
        snap.layoutResult,
        (!showPlaceholder && behavior.hasSelection()) ? &selection : nullptr,
        behavior.caretByte(),
        focused && !input.disabled && !showPlaceholder && !snap.layoutResult.lines.empty(),
        &buf,
        contentOrigin,
        contentW,
        contentW + 2.f * resolved.paddingH,
        editorHeight,
        resolved,
        behavior.caretBlinkPhase()
    );

    Element editor = attachTextInputHandlers(
        std::move(content),
        behavior,
        clickTracker,
        input.disabled,
        [&snap, &buf, scrollOffset, contentOrigin](Point local) {
            return hitTestMultilineByte(snap.layoutResult, local, contentOrigin, (*scrollOffset).y, buf);
        }
    );

    Element scroller = Element {ScrollView {
        .axis = ScrollAxis::Vertical,
        .scrollOffset = scrollOffset,
        .viewportSize = viewportSize,
        .contentSize = contentSize,
        .dragScrollEnabled = false,
        .children = children(std::move(editor)),
    }}
                         .height(fieldHeight);

    return decorateInputField(std::move(scroller), resolved, focused, input.disabled)
        .height(fieldHeight);
}

Element buildSingleLineTextInput(TextInput const &input) {
    Theme const &theme = useEnvironment<Theme>();
    ResolvedTextInputStyle const resolved = resolveTextInputStyle(input.style, theme);
    Font const defaultFont = text_detail::resolveBodyTextStyle(input.style.font, kColorFromTheme).first;

    auto &behavior = useTextEditBehavior(input.value, {.multiline = false,
                                                       .maxLength = input.maxLength,
                                                       .acceptsTab = false,
                                                       .submitsOnEnter = true,
                                                       .onChange = input.onChange,
                                                       .onSubmit = input.onSubmit,
                                                       .onEscape = input.onEscape,
                                                       .verticalResolver = nullptr});
    behavior.setDisabled(input.disabled);

    bool const focused = useFocus();
    behavior.setFocused(focused);

    TextInputLayoutSnap &snap = StateStore::current()->claimSlot<TextInputLayoutSnap>();
    PointerClickTracker &clickTracker = StateStore::current()->claimSlot<PointerClickTracker>();
    State<int> scrollByte = useState(0);
    LayoutConstraints const *layoutConstraints = useLayoutConstraints();
    float const constrainedWidth =
        layoutConstraints && std::isfinite(layoutConstraints->maxWidth) && layoutConstraints->maxWidth > 0.f
            ? layoutConstraints->maxWidth
            : 0.f;
    float const shellInset = inputShellInset(resolved);
    float const fieldHeight =
        resolvedInputFieldHeight(defaultFont, resolved.textColor, resolved.paddingV + shellInset, input.style.height);
    std::string const &buf = behavior.value();
    bool const showPlaceholder = buf.empty() && !focused;
    float const intrinsicContentW =
        intrinsicSingleLineContentWidth(input.placeholder, input.styler, input.validationColor, resolved,
                                        defaultFont, buf, showPlaceholder);
    float const frameWidth =
        constrainedWidth > 0.f ? constrainedWidth :
        intrinsicContentW + 2.f * (shellInset + resolved.paddingH);
    float const contentW = std::max(1.f, frameWidth - 2.f * (shellInset + resolved.paddingH));
    ensureLayout(snap, input.placeholder, input.styler, input.validationColor, resolved, defaultFont, buf, contentW, showPlaceholder,
                 TextWrapping::NoWrap);

    int sb = !showPlaceholder ? detail::utf8Clamp(buf, *scrollByte) : 0;
    if (behavior.consumeEnsureCaretVisibleRequest() && !showPlaceholder) {
        sb = detail::scrollByteToKeepCaretVisible(snap.layoutResult, buf, sb, behavior.caretByte(),
                                                  contentW, kCaretScrollMarginPx);
    }
    if (sb != *scrollByte) {
        scrollByte = sb;
    }
    float const scrollX = detail::scrollOffsetXForByte(snap.layoutResult, sb);
    Point const textOrigin {shellInset + resolved.paddingH - scrollX, shellInset + resolved.paddingV};
    auto selection = behaviorSelection(behavior, buf);
    Element content = buildTextInputContent(
        snap.layoutResult,
        (!showPlaceholder && behavior.hasSelection()) ? &selection : nullptr,
        behavior.caretByte(),
        focused && !input.disabled && !showPlaceholder,
        &buf,
        textOrigin,
        contentW,
        0.f,
        fieldHeight,
        resolved,
        behavior.caretBlinkPhase()
    );

    Element editor = attachTextInputHandlers(
        std::move(content),
        behavior,
        clickTracker,
        input.disabled,
        [&snap, &buf, scrollByte, textOrigin](Point local) {
            return hitTestSingleLineByte(snap.layoutResult, local, textOrigin, *scrollByte, buf);
        }
    );

    return decorateInputField(std::move(editor), resolved, focused, input.disabled)
        .height(fieldHeight);
}

} // namespace

Element TextInput::body() const {
    if (multiline) {
        return buildMultilineTextInput(*this);
    }
    return buildSingleLineTextInput(*this);
}

} // namespace flux
