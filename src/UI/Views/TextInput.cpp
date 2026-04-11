#include <Flux/UI/Views/TextInput.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/UI/InputFieldLayout.hpp>
#include <Flux/UI/LayoutContext.hpp>
#include <Flux/UI/RenderContext.hpp>
#include <Flux/UI/Theme.hpp>
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
        ph.runs.push_back(AttributedRun {0, static_cast<std::uint32_t>(placeholderText.size()), defaultFont, rs.placeholderColor});
        return ph;
    }
    AttributedString as;
    as.utf8 = val;
    std::uint32_t const n = static_cast<std::uint32_t>(val.size());
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
            as.runs.push_back(AttributedRun {0, n, defaultFont, rs.textColor});
            if (memo) {
                memo->runs = as.runs;
            }
        }
    } else {
        Color const c = validationColor ? validationColor(val) : rs.textColor;
        as.runs.push_back(AttributedRun {0, static_cast<std::uint32_t>(val.size()), defaultFont, c});
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

struct TextLayoutDisplay {
    std::shared_ptr<TextLayout const> textLayout;

    void layout(LayoutContext &ctx) const {
        ComponentKey const stableKey = ctx.leafComponentKey();
        ctx.advanceChildSlot();
        Rect const bounds = ctx.layoutEngine().consumeAssignedFrame();
        LayoutNode n {};
        n.kind = LayoutNode::Kind::Leaf;
        n.frame = bounds;
        n.componentKey = stableKey;
        n.element = ctx.currentElement();
        n.constraints = ctx.constraints();
        n.hints = ctx.hints();
        ctx.pushLayoutNode(std::move(n));
    }

    void renderFromLayout(RenderContext &ctx, LayoutNode const &node) const {
        if (!textLayout) {
            return;
        }
        ctx.graph().addText(ctx.parentLayer(), TextNode {
                                                   .layout = textLayout,
                                                   .origin = {node.frame.x, node.frame.y},
                                                   .allocation = node.frame,
                                               });
    }

    Size measure(LayoutContext &ctx, LayoutConstraints const &, LayoutHints const &, TextSystem &) const {
        ctx.advanceChildSlot();
        return textLayout ? textLayout->measuredSize : Size {};
    }
};

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

int hitTestByte(TextInputLayoutSnap &snap, std::string const &placeholderText,
                std::function<std::vector<AttributedRun>(std::string_view)> const &styler,
                std::function<Color(std::string_view)> const &validationColor, TextEditBehavior &beh,
                ResolvedTextInputStyle const &rs, Font const &defaultFont, float frameWidth, Point local,
                int scrollByte, bool showPh) {
    std::string const &buf = beh.value();
    float const shellInset = inputShellInset(rs);
    float const contentW = std::max(1.f, frameWidth - 2.f * (shellInset + rs.paddingH));
    if (!ensureLayout(snap, placeholderText, styler, validationColor, rs, defaultFont, buf, contentW, showPh,
                      TextWrapping::NoWrap)) {
        return 0;
    }
    if (snap.layoutResult.lines.empty()) {
        return 0;
    }
    float const scrollX = detail::scrollOffsetXForByte(snap.layoutResult, detail::utf8Clamp(buf, scrollByte));
    float const lx = local.x - shellInset - rs.paddingH + scrollX;
    std::string const &sliceBuf = showPh ? placeholderText : buf;
    return detail::caretByteAtPoint(snap.layoutResult, Point {lx, 0.f}, sliceBuf);
}

int hitTestLineWrapByte(TextEditBehavior &behavior, ResolvedTextInputStyle const &rs,
                        std::string const &placeholder,
                        std::function<std::vector<AttributedRun>(std::string_view)> const &styler,
                        TextInputStylerMemo &stylerMemo, Font const &defaultFont, bool focused,
                        TextInputLayoutSnap &snap, float frameWidth, Point local, float scrollY) {
    std::string const &buf = behavior.value();
    if (buf.empty()) {
        return 0;
    }

    bool const showPlaceholder = buf.empty() && !focused;
    float const contentW = std::max(1.f, frameWidth - 2.f * (rs.borderWidth + rs.paddingH));
    if (!ensureLayout(snap, placeholder, styler, nullptr, rs, defaultFont, buf, contentW, showPlaceholder,
                      TextWrapping::Wrap, &stylerMemo)) {
        return 0;
    }

    float const layoutY = local.y - rs.paddingV + scrollY;
    float const layoutX = local.x - rs.paddingH;
    return detail::caretByteAtPoint(snap.layoutResult, Point {layoutX, layoutY}, buf);
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
    std::optional<Rect> layoutRect = useLayoutRect();

    auto &behavior = useTextEditBehavior(input.value, {.multiline = true,
                                                       .maxLength = input.maxLength,
                                                       .acceptsTab = true,
                                                       .submitsOnEnter = false,
                                                       .onChange = input.onChange,
                                                       .onSubmit = nullptr,
                                                       .onEscape = input.onEscape,
                                                       .verticalResolver = [&snap, st = input.value](int cur, int dir) {
                                                           return detail::moveCaretVertically(snap.layoutResult, *st, cur, dir);
                                                       }});
    behavior.setDisabled(input.disabled);

    bool const focused = useFocus();
    behavior.setFocused(focused);
    float const frameWidth = layoutRect ? layoutRect->width : (snap.layoutContentW > 0.f ? snap.layoutContentW + 2.f * resolved.paddingH : 400.f);
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

    std::vector<Element> contentChildren;
    if (snap.layoutResult.layout) {
        auto selection = detail::TextEditSelection {
            .caretByte = detail::utf8Clamp(buf, behavior.caretByte()),
            .anchorByte = detail::utf8Clamp(buf, behavior.selectionAnchorByte()),
        };
        auto overlayChildren = makeSelectionAndCaretElements(snap.layoutResult,
                                                             (!showPlaceholder && behavior.hasSelection()) ? &selection : nullptr,
                                                             behavior.caretByte(),
                                                             focused && !input.disabled && !showPlaceholder && !snap.layoutResult.lines.empty(),
                                                             &buf,
                                                             Point {resolved.paddingH, resolved.paddingV}, resolved,
                                                             behavior.caretBlinkPhase());
        contentChildren.insert(contentChildren.end(),
                               std::make_move_iterator(overlayChildren.begin()),
                               std::make_move_iterator(overlayChildren.end()));
        contentChildren.push_back(Element {TextLayoutDisplay {.textLayout = snap.layoutResult.layout}}
                                      .size(contentW, snap.layoutResult.layout->measuredSize.height)
                                      .position(resolved.paddingH, resolved.paddingV));
    }

    float const contentHeight =
        (snap.layoutResult.layout ? snap.layoutResult.layout->measuredSize.height : 0.f) + 2.f * resolved.paddingV;
    Element content = Element {ZStack {
                                   .horizontalAlignment = Alignment::Start,
                                   .verticalAlignment = Alignment::Start,
                                   .children = std::move(contentChildren),
                               }}
                          .size(contentW + 2.f * resolved.paddingH, contentHeight)
                          .clipContent(true);

    Element editor = std::move(content)
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

    return decorateInputField(std::move(scroller), resolved, focused, input.disabled)
        .height(input.multilineHeight.fixed > 0.f ? input.multilineHeight.fixed : input.multilineHeight.minIntrinsic);
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

    TextInputLayoutSnap &snap = StateStore::current()->claimSlot<TextInputLayoutSnap>();
    State<int> scrollByte = useState(0);
    std::optional<Rect> layoutRect = useLayoutRect();
    float const shellInset = inputShellInset(resolved);
    float const fieldHeight =
        resolvedInputFieldHeight(defaultFont, resolved.textColor, resolved.paddingV + shellInset, style.height);
    float const frameWidth =
        layoutRect ? layoutRect->width : (snap.layoutContentW > 0.f ? snap.layoutContentW + 2.f * (shellInset + resolved.paddingH) : 400.f);
    float const contentW = std::max(1.f, frameWidth - 2.f * (shellInset + resolved.paddingH));
    std::string const &buf = beh.value();
    bool const showPh = buf.empty() && !focused;
    ensureLayout(snap, placeholder, styler, validationColor, resolved, defaultFont, buf, contentW, showPh,
                 TextWrapping::NoWrap);

    int sb = !showPh ? detail::utf8Clamp(buf, *scrollByte) : 0;
    if (beh.consumeEnsureCaretVisibleRequest() && !showPh) {
        sb = detail::scrollByteToKeepCaretVisible(snap.layoutResult, buf, sb, beh.caretByte(),
                                                  contentW, kCaretScrollMarginPx);
    }
    if (sb != *scrollByte) {
        scrollByte = sb;
    }
    float const scrollX = detail::scrollOffsetXForByte(snap.layoutResult, sb);
    Point const textOrigin {shellInset + resolved.paddingH - scrollX, shellInset + resolved.paddingV};

    std::vector<Element> layers;
    if (snap.layoutResult.layout) {
        auto selection = detail::TextEditSelection {
            .caretByte = detail::utf8Clamp(buf, beh.caretByte()),
            .anchorByte = detail::utf8Clamp(buf, beh.selectionAnchorByte()),
        };
        auto overlayChildren = makeSelectionAndCaretElements(snap.layoutResult,
                                                             (!showPh && beh.hasSelection()) ? &selection : nullptr,
                                                             beh.caretByte(),
                                                             focused && !disabled && !showPh,
                                                             &buf,
                                                             textOrigin, resolved, beh.caretBlinkPhase());
        layers.insert(layers.end(), std::make_move_iterator(overlayChildren.begin()),
                      std::make_move_iterator(overlayChildren.end()));
        layers.push_back(Element {TextLayoutDisplay {.textLayout = snap.layoutResult.layout}}
                             .size(contentW, snap.layoutResult.layout->measuredSize.height)
                             .position(textOrigin.x, textOrigin.y));
    }

    Element content = Element {ZStack {
                                   .horizontalAlignment = Alignment::Start,
                                   .verticalAlignment = Alignment::Start,
                                   .children = std::move(layers),
                               }}
                          .size(0.f, fieldHeight)
                          .clipContent(true);

    return decorateInputField(std::move(content), resolved, focused, disabled)
        .height(fieldHeight)
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
