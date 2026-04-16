#include <Flux/UI/Views/Text.hpp>

#include <Flux/UI/Detail/LayoutDebugDump.hpp>
#include <Flux/UI/Detail/LeafBounds.hpp>
#include <Flux/UI/EventMap.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/LayoutContext.hpp>
#include <Flux/UI/RenderContext.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/SelectableTextSupport.hpp>
#include <Flux/UI/Views/TextEditUtils.hpp>

#include <Flux/Core/Cursor.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Scene/Nodes.hpp>
#include <Flux/Scene/SceneGraph.hpp>

#include "UI/Detail/EventHelpers.hpp"
#include "UI/Layout/LayoutHelpers.hpp"
#include "UI/Views/TextSupport.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <functional>
#include <memory>
#include <utility>

namespace flux {

namespace {

Rect explicitLeafBox(Text const &) {
    return {};
}

bool hasRenderableTextGeometry(TextLayout const &layout) {
    return !layout.runs.empty() || !layout.lines.empty() || layout.measuredSize.width > 0.f ||
           layout.measuredSize.height > 0.f;
}

float textMeasureWidth(TextWrapping wrapping, Rect const &bounds) {
    return wrapping == TextWrapping::NoWrap ? 0.f : std::max(0.f, bounds.width);
}

float multiLineFitTolerance(TextLayoutOptions const &options) {
    return options.maxLines > 0 && options.wrapping != TextWrapping::NoWrap ? 1.5f : 0.5f;
}

bool plainTextWouldOverflow(std::string const &text, Font const &font, Color const &color, Rect const &bounds,
                            TextLayoutOptions const &options, TextSystem &ts) {
    float const tolerance = multiLineFitTolerance(options);
    if (text.empty()) {
        return false;
    }

    if (options.wrapping == TextWrapping::NoWrap && bounds.width > 0.f) {
        Size const fullSize = ts.measure(text, font, color, 0.f, options);
        if (fullSize.width > bounds.width + tolerance) {
            return true;
        }
    }

    if (options.maxLines > 0 && bounds.height > 0.f) {
        TextLayoutOptions unlimited = options;
        unlimited.maxLines = 0;
        Size const fullSize = ts.measure(text, font, color, textMeasureWidth(options.wrapping, bounds), unlimited);
        if (fullSize.height > bounds.height + tolerance) {
            return true;
        }
    }

    return false;
}

bool candidateFitsWithEllipsis(std::string const &candidate, Font const &font, Color const &color, Rect const &bounds,
                               TextLayoutOptions const &options, TextSystem &ts) {
    float const tolerance = multiLineFitTolerance(options);
    if (options.wrapping == TextWrapping::NoWrap) {
        if (bounds.width <= 0.f) {
            return true;
        }
        Size const size = ts.measure(candidate, font, color, 0.f, options);
        return size.width <= bounds.width + tolerance;
    }

    if (options.maxLines > 0 && bounds.height > 0.f) {
        TextLayoutOptions unlimited = options;
        unlimited.maxLines = 0;
        Size const size = ts.measure(candidate, font, color, textMeasureWidth(options.wrapping, bounds), unlimited);
        return size.height <= bounds.height + tolerance;
    }

    return true;
}

std::uint64_t hashCombine(std::uint64_t seed, std::uint64_t value) {
    seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
    return seed;
}

std::uint64_t hashFloat(float value) {
    std::uint32_t bits{};
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

std::uint64_t hashBool(bool value) {
    return value ? 0x9b8d6f43a2c17e5dull : 0x1f2e3d4c5b6a7988ull;
}

std::uint64_t hashString(std::string const &value) {
    return std::hash<std::string>{}(value);
}

std::uint64_t hashColor(Color value) {
    std::uint64_t h = 0x2db4f7a681c5930eull;
    h = hashCombine(h, hashFloat(value.r));
    h = hashCombine(h, hashFloat(value.g));
    h = hashCombine(h, hashFloat(value.b));
    h = hashCombine(h, hashFloat(value.a));
    return h;
}

std::string trimTrailingWhitespace(std::string s) {
    while (!s.empty()) {
        unsigned char const ch = static_cast<unsigned char>(s.back());
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
            s.pop_back();
            continue;
        }
        break;
    }
    return s;
}

std::string ellipsizedPlainText(std::string const &text, Font const &font, Color const &color, Rect const &bounds,
                                TextLayoutOptions const &options, TextSystem &ts) {
    if (!plainTextWouldOverflow(text, font, color, bounds, options, ts)) {
        return text;
    }

    std::string const ellipsis = "...";
    if (!candidateFitsWithEllipsis(ellipsis, font, color, bounds, options, ts)) {
        return ellipsis;
    }

    std::vector<int> boundaries;
    boundaries.reserve(text.size() + 1);
    boundaries.push_back(0);
    for (int pos = 0; pos < static_cast<int>(text.size());) {
        pos = detail::utf8NextChar(text, pos);
        boundaries.push_back(pos);
    }

    int low = 0;
    int high = static_cast<int>(boundaries.size()) - 1;
    int best = 0;
    while (low <= high) {
        int const mid = low + (high - low) / 2;
        std::string candidate = trimTrailingWhitespace(text.substr(0, static_cast<std::size_t>(boundaries[static_cast<std::size_t>(mid)])));
        candidate += ellipsis;
        if (candidateFitsWithEllipsis(candidate, font, color, bounds, options, ts)) {
            best = mid;
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }

    std::string fitted = trimTrailingWhitespace(text.substr(0, static_cast<std::size_t>(boundaries[static_cast<std::size_t>(best)])));
    fitted += ellipsis;
    return fitted;
}

} // namespace

using flux::detail::eventHandlersFromModifiers;
using flux::detail::shouldInsertHandlers;

void Text::layout(LayoutContext &ctx) const {
    ComponentKey const stableKey = ctx.leafComponentKey();
    ctx.advanceChildSlot();
    Rect const assigned = ctx.layoutEngine().consumeAssignedFrame();
    LayoutConstraints const &cs = ctx.constraints();
    Rect const bounds = flux::detail::resolveLeafLayoutBounds(
        explicitLeafBox(*this), assigned, cs, ctx.hints()
    );
    // Degenerate axis is valid when the parent assigned no space there, or when constraints cap that axis at <= 0.
    bool const widthExplained = bounds.width > 0.f || assigned.width <= 0.f ||
                                (std::isfinite(cs.maxWidth) && cs.maxWidth <= 0.f);
    bool const heightExplained = bounds.height > 0.f || assigned.height <= 0.f ||
                                 (std::isfinite(cs.maxHeight) && cs.maxHeight <= 0.f);
    assert(text.empty() || (widthExplained && heightExplained));
#ifdef NDEBUG
    (void)widthExplained;
    (void)heightExplained;
#endif
    LayoutNode n {};
    n.kind = LayoutNode::Kind::Leaf;
    n.frame = bounds;
    n.componentKey = stableKey;
    n.element = ctx.currentElement();
    n.constraints = ctx.constraints();
    n.hints = ctx.hints();
    LayoutNodeId const id = ctx.pushLayoutNode(std::move(n));
    ctx.registerCompositeSubtreeRootIfPending(id);
    layoutDebugLogLeaf("Text", ctx.constraints(), bounds, detail::flexGrowOf(*this),
                       detail::flexShrinkOf(*this), detail::minMainSizeOf(*this));
}

void Text::renderFromLayout(RenderContext &ctx, LayoutNode const &node) const {
    ComponentKey const stableKey = node.componentKey;
    Rect const bounds = node.frame;
    std::shared_ptr<TextLayout const> textLayout;
    Theme const &theme = useEnvironment<Theme>();
    Color const resolvedSelectionColor = resolveColor(selectionColor, theme.colorAccentSubtle);
    if (!text.empty()) {
        auto [font, color] = text_detail::resolveBodyTextStyle(this->font, this->color);
        TextLayoutOptions const opts = text_detail::makeTextLayoutOptions(*this);
        std::string const displayText =
            selectable ? text : ellipsizedPlainText(text, font, color, bounds, opts, ctx.textSystem());
        textLayout = ctx.textSystem().layout(displayText, font, color, bounds, opts);
    }

    if (textLayout && hasRenderableTextGeometry(*textLayout)) {
        std::shared_ptr<detail::SelectableTextState> selectableState;
        if (selectable) {
            selectableState = detail::selectableTextState(stableKey);
            detail::updateSelectableTextLayout(*selectableState, textLayout, text, bounds.width);
            if (selectableState->selection.hasSelection()) {
                for (Rect const &rect :
                     detail::selectionRects(selectableState->layoutResult, selectableState->selection,
                                            &selectableState->text, bounds.x, bounds.y)) {
                    ctx.graph().addRect(ctx.parentLayer(), RectNode {
                                                               .bounds = rect,
                                                               .fill = FillStyle::solid(resolvedSelectionColor),
                                                           });
                }
            }
        }

        NodeId const textId = ctx.graph().addText(ctx.parentLayer(), TextNode {
                                                                         .layout = textLayout,
                                                                         .origin = {bounds.x, bounds.y},
                                                                         .allocation = bounds,
                                                                     });
        if (!ctx.suppressLeafModifierEvents()) {
            EventHandlers handlers {};
            if (ElementModifiers const *mods = ctx.activeElementModifiers()) {
                handlers = eventHandlersFromModifiers(*mods, stableKey, bounds);
            }
            if (selectable && selectableState) {
                handlers.stableTargetKey = stableKey;
                handlers.focusable = true;
                handlers.cursor = Cursor::IBeam;
                handlers.onPointerDown = [state = selectableState](Point local) {
                    detail::handleSelectableTextPointerDown(*state, local);
                };
                handlers.onPointerMove = [state = selectableState](Point local) {
                    detail::handleSelectableTextPointerDrag(*state, local);
                };
                handlers.onPointerUp = [state = selectableState](Point) {
                    detail::handleSelectableTextPointerUp(*state);
                };
                handlers.onKeyDown = [state = selectableState](KeyCode key, Modifiers modifiers) {
                    detail::handleSelectableTextKey(*state, key, modifiers);
                };
            }
            if (shouldInsertHandlers(handlers)) {
                ctx.eventMap().insert(textId, std::move(handlers));
            }
        }
    }
}

std::uint64_t Text::measureCacheKey() const noexcept {
    std::uint64_t h = 0x4fb3d8c2a9716e05ull;
    h = hashCombine(h, hashString(text));
    h = hashCombine(h, hashString(font.family));
    h = hashCombine(h, hashFloat(font.size));
    h = hashCombine(h, hashFloat(font.weight));
    h = hashCombine(h, hashBool(font.italic));
    h = hashCombine(h, hashColor(color));
    h = hashCombine(h, hashColor(selectionColor));
    h = hashCombine(h, hashBool(selectable));
    h = hashCombine(h, std::hash<int>{}(static_cast<int>(horizontalAlignment)));
    h = hashCombine(h, std::hash<int>{}(static_cast<int>(verticalAlignment)));
    h = hashCombine(h, std::hash<int>{}(static_cast<int>(wrapping)));
    h = hashCombine(h, std::hash<int>{}(maxLines));
    h = hashCombine(h, hashFloat(firstBaselineOffset));
    return h;
}

Size Text::measure(LayoutContext &ctx, LayoutConstraints const &c, LayoutHints const &hints, TextSystem &ts) const {
    ctx.advanceChildSlot();
    (void)hints;
    auto [font, color] = text_detail::resolveBodyTextStyle(this->font, this->color);
    TextLayoutOptions const opts = text_detail::makeTextLayoutOptions(*this);
    // Match boxed layout (`layoutBoxedImpl`): NoWrap uses maxWidth 0 so Core Text measures one line.
    // A finite maxWidth here would wrap during measure (e.g. ScrollView passes viewport width) and inflate
    // height while render still lays out as nowrap.
    float mw = std::isfinite(c.maxWidth) ? c.maxWidth : 0.f;
    if (wrapping == TextWrapping::NoWrap) {
        mw = 0.f;
    }
    Size s = ts.measure(text, font, color, mw, opts);
    if (std::isfinite(c.maxWidth) && c.maxWidth > 0.f) {
        s.width = std::min(s.width, c.maxWidth);
    }
    if (std::isfinite(c.maxHeight) && c.maxHeight > 0.f) {
        s.height = std::min(s.height, c.maxHeight);
    }
    return s;
}

} // namespace flux
