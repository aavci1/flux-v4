#pragma once

#include <Flux/Core/Application.hpp>
#include <Flux/Core/Clipboard.hpp>
#include <Flux/Core/KeyCodes.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/AttributedString.hpp>
#include <Flux/Graphics/Font.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/ViewModifiers.hpp>
#include <Flux/UI/Views/Render.hpp>
#include <Flux/UI/Views/TextEditUtils.hpp>

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lambda {

struct MarkdownResolvedStyle {
    flux::Font baseFont {};
    flux::Font boldFont {};
    flux::Font h1Font {};
    flux::Font h2Font {};
    flux::Font h3Font {};
    flux::Font codeFont {};
    flux::Color baseColor {};
    flux::Color codeBackground {};
};

inline MarkdownResolvedStyle resolveMarkdownStyle(flux::Theme const &theme, flux::Font const &font, flux::Color color) {
    flux::Font const baseFont = flux::resolveFont(font, theme.bodyFont, theme);
    flux::Font const codeFont = flux::resolveFont(theme.monospacedBodyFont, baseFont);
    flux::Color const baseColor = flux::resolveColor(color, theme.labelColor, theme);

    flux::Font boldFont = baseFont;
    boldFont.weight = std::clamp((boldFont.weight > 0.f ? boldFont.weight : 400.f) + 250.f, 400.f, 900.f);

    flux::Font h1Font = baseFont;
    h1Font.size = std::max(baseFont.size * 2.f, theme.titleFont.size);
    h1Font.weight = std::max(baseFont.weight, theme.titleFont.weight);

    flux::Font h2Font = baseFont;
    h2Font.size = std::max(baseFont.size * 1.6f, theme.title2Font.size);
    h2Font.weight = std::max(baseFont.weight, theme.titleFont.weight);

    flux::Font h3Font = baseFont;
    h3Font.size = std::max(baseFont.size * 1.3f, theme.title3Font.size);
    h3Font.weight = std::max(baseFont.weight, theme.headlineFont.weight);

    flux::Color codeBackground = theme.controlBackgroundColor;
    codeBackground.a = 0.9f;

    return MarkdownResolvedStyle {
        .baseFont = baseFont,
        .boldFont = boldFont,
        .h1Font = h1Font,
        .h2Font = h2Font,
        .h3Font = h3Font,
        .codeFont = codeFont,
        .baseColor = baseColor,
        .codeBackground = codeBackground,
    };
}

namespace detail {

inline bool sameFont(flux::Font const &lhs, flux::Font const &rhs) {
    return lhs.family == rhs.family && lhs.size == rhs.size && lhs.weight == rhs.weight && lhs.italic == rhs.italic;
}

inline std::size_t hashCombine(std::size_t seed, std::size_t value) {
    return seed ^ (value + 0x9e3779b9u + (seed << 6) + (seed >> 2));
}

inline std::size_t hashFloat(float value) {
    return std::hash<std::uint32_t> {}(std::bit_cast<std::uint32_t>(value));
}

inline std::size_t hashFont(flux::Font const &font) {
    std::size_t seed = std::hash<std::string> {}(font.family);
    seed = hashCombine(seed, hashFloat(font.size));
    seed = hashCombine(seed, hashFloat(font.weight));
    seed = hashCombine(seed, std::hash<bool> {}(font.italic));
    return seed;
}

inline std::size_t hashColor(flux::Color const &color) {
    std::size_t seed = hashFloat(color.r);
    seed = hashCombine(seed, hashFloat(color.g));
    seed = hashCombine(seed, hashFloat(color.b));
    seed = hashCombine(seed, hashFloat(color.a));
    return seed;
}

inline void appendStyledText(flux::AttributedString &attributed, std::string_view text, flux::Font const &font,
                             flux::Color color, std::optional<flux::Color> backgroundColor = std::nullopt) {
    if (text.empty()) {
        return;
    }

    std::uint32_t const start = static_cast<std::uint32_t>(attributed.utf8.size());
    attributed.utf8.append(text.data(), text.size());
    std::uint32_t const end = static_cast<std::uint32_t>(attributed.utf8.size());

    if (!attributed.runs.empty()) {
        flux::AttributedRun &last = attributed.runs.back();
        if (last.end == start && sameFont(last.font, font) && last.color == color && last.backgroundColor == backgroundColor) {
            last.end = end;
            return;
        }
    }

    attributed.runs.push_back(flux::AttributedRun {
        .start = start,
        .end = end,
        .font = font,
        .color = color,
        .backgroundColor = backgroundColor,
    });
}

inline flux::Font markdownBoldFont(flux::Font font) {
    font.weight = std::clamp((font.weight > 0.f ? font.weight : 400.f) + 250.f, 400.f, 900.f);
    return font;
}

inline void appendInlineMarkdown(flux::AttributedString &attributed, std::string_view text, flux::Font const &baseFont,
                                 flux::Font const &codeFont, flux::Color baseColor,
                                 std::optional<flux::Color> codeBackground = std::nullopt) {
    std::size_t p = 0;
    while (p < text.size()) {
        if (p + 1 < text.size() && text[p] == '*' && text[p + 1] == '*') {
            std::size_t q = p + 2;
            while (q + 1 < text.size()) {
                if (text[q] == '*' && text[q + 1] == '*') {
                    break;
                }
                ++q;
            }
            if (q + 1 < text.size() && text[q] == '*' && text[q + 1] == '*') {
                appendStyledText(attributed, text.substr(p + 2, q - (p + 2)), markdownBoldFont(baseFont), baseColor);
                p = q + 2;
                continue;
            }
        }

        if (text[p] == '`') {
            std::size_t q = p + 1;
            while (q < text.size() && text[q] != '`') {
                ++q;
            }
            if (q < text.size()) {
                appendStyledText(attributed, text.substr(p + 1, q - (p + 1)), codeFont, baseColor, codeBackground);
                p = q + 1;
                continue;
            }
        }

        std::size_t const start = p;
        ++p;
        while (p < text.size()) {
            char const ch = text[p];
            if (ch == '`' || (p + 1 < text.size() && ch == '*' && text[p + 1] == '*')) {
                break;
            }
            ++p;
        }
        appendStyledText(attributed, text.substr(start, p - start), baseFont, baseColor);
    }
}

inline flux::AttributedString makeMarkdownAttributedString(std::string_view source, MarkdownResolvedStyle const &style) {
    enum class BlockKind {
        Paragraph,
        Fence,
    };

    flux::AttributedString attributed;
    if (source.empty()) {
        return attributed;
    }

    auto appendNewline = [&](flux::Font const &font, std::optional<flux::Color> backgroundColor = std::nullopt) {
        appendStyledText(attributed, "\n", font, style.baseColor, backgroundColor);
    };

    auto processParagraphLine = [&](std::string_view line, bool endsNewline) {
        if (line.empty()) {
            if (endsNewline) {
                appendNewline(style.baseFont);
            }
            return;
        }

        std::size_t i = 0;
        int hashes = 0;
        while (i < line.size() && hashes < 3 && line[i] == '#') {
            ++hashes;
            ++i;
        }
        if (hashes > 0 && i < line.size() && line[i] == ' ') {
            flux::Font headingFont = style.h1Font;
            if (hashes == 2) {
                headingFont = style.h2Font;
            } else if (hashes >= 3) {
                headingFont = style.h3Font;
            }
            appendStyledText(attributed, line.substr(i + 1), headingFont, style.baseColor);
            if (endsNewline) {
                appendNewline(style.baseFont);
            }
            return;
        }

        std::size_t j = 0;
        while (j < line.size() && line[j] == ' ') {
            ++j;
        }
        if (j < line.size() && line[j] == '-' && j + 1 < line.size() && line[j + 1] == ' ') {
            appendStyledText(attributed, line.substr(0, j), style.baseFont, style.baseColor);
            appendStyledText(attributed, "\xE2\x80\xA2 ", style.baseFont, style.baseColor);
            appendInlineMarkdown(
                attributed,
                line.substr(j + 2),
                style.baseFont,
                style.codeFont,
                style.baseColor,
                style.codeBackground
            );
            if (endsNewline) {
                appendNewline(style.baseFont);
            }
            return;
        }

        appendInlineMarkdown(attributed, line, style.baseFont, style.codeFont, style.baseColor, style.codeBackground);
        if (endsNewline) {
            appendNewline(style.baseFont);
        }
    };

    BlockKind blockKind = BlockKind::Paragraph;
    std::size_t lineStart = 0;
    while (lineStart < source.size()) {
        std::size_t lineEnd = lineStart;
        while (lineEnd < source.size() && source[lineEnd] != '\n') {
            ++lineEnd;
        }
        bool const endsNewline = lineEnd < source.size() && source[lineEnd] == '\n';
        std::string_view const line = source.substr(lineStart, lineEnd - lineStart);
        bool const fenceLine = line.size() >= 3 && line.substr(0, 3) == "```";

        if (fenceLine) {
            blockKind = blockKind == BlockKind::Fence ? BlockKind::Paragraph : BlockKind::Fence;
            lineStart = endsNewline ? lineEnd + 1 : lineEnd;
            continue;
        }

        if (blockKind == BlockKind::Fence) {
            appendStyledText(attributed, line, style.codeFont, style.baseColor, style.codeBackground);
            if (endsNewline) {
                appendNewline(style.codeFont, style.codeBackground);
            }
        } else {
            processParagraphLine(line, endsNewline);
        }

        lineStart = endsNewline ? lineEnd + 1 : lineEnd;
    }

    return attributed;
}

struct MarkdownLayoutCacheKey {
    std::uint64_t cacheKey = 0;
    std::uint64_t textRevision = 0;
    flux::Font baseFont {};
    flux::Font codeFont {};
    flux::Font h1Font {};
    flux::Font h2Font {};
    flux::Font h3Font {};
    flux::Color baseColor {};
    flux::Color codeBackground {};
    float maxWidth = 0.f;
    flux::HorizontalAlignment horizontalAlignment = flux::HorizontalAlignment::Leading;
    flux::VerticalAlignment verticalAlignment = flux::VerticalAlignment::Top;
    flux::TextWrapping wrapping = flux::TextWrapping::Wrap;
    int maxLines = 0;
    float firstBaselineOffset = 0.f;

    bool operator==(MarkdownLayoutCacheKey const &other) const {
        return cacheKey == other.cacheKey && textRevision == other.textRevision && sameFont(baseFont, other.baseFont) &&
               sameFont(codeFont, other.codeFont) && sameFont(h1Font, other.h1Font) && sameFont(h2Font, other.h2Font) &&
               sameFont(h3Font, other.h3Font) && baseColor == other.baseColor &&
               codeBackground == other.codeBackground && maxWidth == other.maxWidth &&
               horizontalAlignment == other.horizontalAlignment && verticalAlignment == other.verticalAlignment &&
               wrapping == other.wrapping && maxLines == other.maxLines &&
               firstBaselineOffset == other.firstBaselineOffset;
    }
};

struct MarkdownLayoutCacheKeyHash {
    std::size_t operator()(MarkdownLayoutCacheKey const &key) const {
        std::size_t seed = std::hash<std::uint64_t> {}(key.cacheKey);
        seed = hashCombine(seed, std::hash<std::uint64_t> {}(key.textRevision));
        seed = hashCombine(seed, hashFont(key.baseFont));
        seed = hashCombine(seed, hashFont(key.codeFont));
        seed = hashCombine(seed, hashFont(key.h1Font));
        seed = hashCombine(seed, hashFont(key.h2Font));
        seed = hashCombine(seed, hashFont(key.h3Font));
        seed = hashCombine(seed, hashColor(key.baseColor));
        seed = hashCombine(seed, hashColor(key.codeBackground));
        seed = hashCombine(seed, hashFloat(key.maxWidth));
        seed = hashCombine(seed, std::hash<int> {}(static_cast<int>(key.horizontalAlignment)));
        seed = hashCombine(seed, std::hash<int> {}(static_cast<int>(key.verticalAlignment)));
        seed = hashCombine(seed, std::hash<int> {}(static_cast<int>(key.wrapping)));
        seed = hashCombine(seed, std::hash<int> {}(key.maxLines));
        seed = hashCombine(seed, hashFloat(key.firstBaselineOffset));
        return seed;
    }
};

inline auto &markdownLayoutCache() {
    static std::unordered_map<MarkdownLayoutCacheKey, std::shared_ptr<flux::TextLayout const>, MarkdownLayoutCacheKeyHash>
        cache;
    return cache;
}

inline std::shared_ptr<flux::TextLayout const> resolveMarkdownLayout(MarkdownLayoutCacheKey const &key,
                                                                     std::string_view source, flux::TextSystem &textSystem) {
    if (source.empty()) {
        return {};
    }

    auto &cache = markdownLayoutCache();
    auto it = cache.find(key);
    if (it != cache.end()) {
        return it->second;
    }

    MarkdownResolvedStyle const style {
        .baseFont = key.baseFont,
        .boldFont = markdownBoldFont(key.baseFont),
        .h1Font = key.h1Font,
        .h2Font = key.h2Font,
        .h3Font = key.h3Font,
        .codeFont = key.codeFont,
        .baseColor = key.baseColor,
        .codeBackground = key.codeBackground,
    };
    flux::AttributedString attributed = makeMarkdownAttributedString(source, style);
    flux::TextLayoutOptions const options {
        .horizontalAlignment = key.horizontalAlignment,
        .verticalAlignment = key.verticalAlignment,
        .wrapping = key.wrapping,
        .maxLines = key.maxLines,
        .firstBaselineOffset = key.firstBaselineOffset,
    };
    std::shared_ptr<flux::TextLayout const> layout = textSystem.layout(attributed, key.maxWidth, options);

    if (cache.size() >= 512) {
        cache.clear();
    }
    cache.emplace(key, layout);
    return layout;
}

struct SelectableMarkdownClickTracker {
    flux::Point lastPoint {};
    std::chrono::steady_clock::time_point lastClickAt {};
    int clickCount = 0;
};

struct SelectableMarkdownState {
    flux::detail::TextEditSelection selection {};
    bool dragging = false;
    SelectableMarkdownClickTracker clickTracker {};
    flux::detail::TextEditLayoutResult layoutResult {};
    std::string text;
};

inline bool selectableMarkdownHasMod(flux::Modifiers m, flux::Modifiers bit) noexcept {
    return (static_cast<std::uint32_t>(m) & static_cast<std::uint32_t>(bit)) != 0;
}

inline bool selectableMarkdownCmdLike(flux::Modifiers m) noexcept {
    return selectableMarkdownHasMod(m, flux::Modifiers::Meta) || selectableMarkdownHasMod(m, flux::Modifiers::Ctrl);
}

inline void markSelectableMarkdownDirty() {
    if (!flux::Application::hasInstance()) {
        return;
    }
    flux::Application::instance().requestRedraw();
}

inline void updateSelectableMarkdownLayout(
    SelectableMarkdownState &state,
    std::shared_ptr<flux::TextLayout const> layout,
    std::string text,
    float contentWidth
) {
    state.text = std::move(text);
    state.layoutResult = flux::detail::makeTextEditLayoutResult(
        std::move(layout),
        static_cast<int>(state.text.size()),
        contentWidth
    );
    state.selection = flux::detail::clampSelection(state.text, state.selection);
}

inline void copySelectableMarkdownSelection(SelectableMarkdownState const &state) {
    if (!flux::Application::hasInstance()) {
        return;
    }
    auto const [start, end] = state.selection.ordered();
    if (start >= end || start < 0 || end > static_cast<int>(state.text.size())) {
        return;
    }
    flux::Application::instance().clipboard().writeText(
        state.text.substr(static_cast<std::size_t>(start), static_cast<std::size_t>(end - start))
    );
}

inline int registerSelectableMarkdownClick(SelectableMarkdownClickTracker &tracker, flux::Point local) {
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

inline void handleSelectableMarkdownPointerDown(SelectableMarkdownState &state, flux::Point local) {
    int const byte = flux::detail::caretByteAtPoint(state.layoutResult, local, state.text);
    int const clickCount = registerSelectableMarkdownClick(state.clickTracker, local);
    state.dragging = (clickCount == 1);
    if (clickCount >= 3) {
        state.selection = flux::detail::selectAllSelection(state.text);
    } else if (clickCount == 2) {
        state.selection = flux::detail::wordSelectionAtByte(state.text, byte);
    } else {
        state.selection = flux::detail::moveSelectionToByte(state.text, state.selection, byte, false);
    }
    markSelectableMarkdownDirty();
}

inline void handleSelectableMarkdownPointerDrag(SelectableMarkdownState &state, flux::Point local) {
    if (!state.dragging) {
        return;
    }
    int const byte = flux::detail::caretByteAtPoint(state.layoutResult, local, state.text);
    state.selection = flux::detail::moveSelectionToByte(state.text, state.selection, byte, true);
    markSelectableMarkdownDirty();
}

inline void handleSelectableMarkdownPointerUp(SelectableMarkdownState &state) {
    state.dragging = false;
}

inline bool handleSelectableMarkdownKey(
    SelectableMarkdownState &state,
    flux::KeyCode key,
    flux::Modifiers modifiers
) {
    bool const shift = selectableMarkdownHasMod(modifiers, flux::Modifiers::Shift);
    bool const alt = selectableMarkdownHasMod(modifiers, flux::Modifiers::Alt);
    bool const cmd = selectableMarkdownCmdLike(modifiers);

    if (cmd && !shift && key == flux::keys::A) {
        state.selection = flux::detail::selectAllSelection(state.text);
        markSelectableMarkdownDirty();
        return true;
    }
    if (cmd && !shift && key == flux::keys::C) {
        copySelectableMarkdownSelection(state);
        return true;
    }
    if (key == flux::keys::LeftArrow) {
        state.selection = cmd ? flux::detail::moveSelectionToDocumentBoundary(state.text, state.selection, false, shift) :
                         alt ? flux::detail::moveSelectionByWord(state.text, state.selection, -1, shift) :
                               flux::detail::moveSelectionByChar(state.text, state.selection, -1, shift);
        markSelectableMarkdownDirty();
        return true;
    }
    if (key == flux::keys::RightArrow) {
        state.selection = cmd ? flux::detail::moveSelectionToDocumentBoundary(state.text, state.selection, true, shift) :
                         alt ? flux::detail::moveSelectionByWord(state.text, state.selection, 1, shift) :
                               flux::detail::moveSelectionByChar(state.text, state.selection, 1, shift);
        markSelectableMarkdownDirty();
        return true;
    }
    if (key == flux::keys::Home) {
        state.selection = flux::detail::moveSelectionToDocumentBoundary(state.text, state.selection, false, shift);
        markSelectableMarkdownDirty();
        return true;
    }
    if (key == flux::keys::End) {
        state.selection = flux::detail::moveSelectionToDocumentBoundary(state.text, state.selection, true, shift);
        markSelectableMarkdownDirty();
        return true;
    }
    if ((key == flux::keys::UpArrow || key == flux::keys::DownArrow) && state.layoutResult.lines.size() > 1) {
        int const caret = flux::detail::moveCaretVertically(
            state.layoutResult,
            state.text,
            state.selection.caretByte,
            key == flux::keys::UpArrow ? -1 : 1
        );
        state.selection = flux::detail::moveSelectionToByte(state.text, state.selection, caret, shift);
        markSelectableMarkdownDirty();
        return true;
    }
    return false;
}

} // namespace detail

struct MarkdownText : flux::ViewModifiers<MarkdownText> {
    std::string text;
    std::uint64_t cacheKey = 0;
    std::uint64_t textRevision = 0;
    flux::Font baseFont {};
    flux::Font codeFont {};
    flux::Font h1Font {};
    flux::Font h2Font {};
    flux::Font h3Font {};
    flux::Color baseColor {};
    flux::Color codeBackground {};
    flux::Color selectionColor = flux::Color::theme();
    flux::HorizontalAlignment horizontalAlignment = flux::HorizontalAlignment::Leading;
    flux::VerticalAlignment verticalAlignment = flux::VerticalAlignment::Top;
    flux::TextWrapping wrapping = flux::TextWrapping::Wrap;
    int maxLines = 0;
    float firstBaselineOffset = 0.f;
    bool selectable = false;

    detail::MarkdownLayoutCacheKey makeCacheKey(float maxWidth) const {
        return detail::MarkdownLayoutCacheKey {
            .cacheKey = cacheKey,
            .textRevision = textRevision,
            .baseFont = baseFont,
            .codeFont = codeFont,
            .h1Font = h1Font,
            .h2Font = h2Font,
            .h3Font = h3Font,
            .baseColor = baseColor,
            .codeBackground = codeBackground,
            .maxWidth = maxWidth,
            .horizontalAlignment = horizontalAlignment,
            .verticalAlignment = verticalAlignment,
            .wrapping = wrapping,
            .maxLines = maxLines,
            .firstBaselineOffset = firstBaselineOffset,
        };
    }

    float effectiveMaxWidth(float width) const {
        if (wrapping == flux::TextWrapping::NoWrap || !std::isfinite(width) || width <= 0.f) {
            return 0.f;
        }
        return width;
    }

    std::shared_ptr<flux::TextLayout const> layoutForWidth(float width, flux::TextSystem &textSystem) const {
        return detail::resolveMarkdownLayout(makeCacheKey(effectiveMaxWidth(width)), text, textSystem);
    }

    auto body() const {
        auto theme = flux::useEnvironment<flux::Theme>();
        auto selectableStateHandle =
            flux::useState<std::shared_ptr<detail::SelectableMarkdownState>>(
                std::make_shared<detail::SelectableMarkdownState>()
            );
        std::shared_ptr<detail::SelectableMarkdownState> selectableState =
            selectable ? *selectableStateHandle : nullptr;
        MarkdownText markdown = *this;

        auto widthForConstraints = [markdown](flux::LayoutConstraints const &constraints) {
            return std::isfinite(constraints.maxWidth) && constraints.maxWidth > 0.f ? constraints.maxWidth : 0.f;
        };
        auto layoutForFrame = [markdown](float width) {
            return markdown.layoutForWidth(width, flux::Application::instance().textSystem());
        };
        auto updateSelectionLayout =
            [markdown](detail::SelectableMarkdownState &state, std::shared_ptr<flux::TextLayout const> const &layout) {
                MarkdownResolvedStyle const selectionStyle {
                    .baseFont = markdown.baseFont,
                    .boldFont = detail::markdownBoldFont(markdown.baseFont),
                    .h1Font = markdown.h1Font,
                    .h2Font = markdown.h2Font,
                    .h3Font = markdown.h3Font,
                    .codeFont = markdown.codeFont,
                    .baseColor = markdown.baseColor,
                    .codeBackground = markdown.codeBackground,
                };
                flux::AttributedString renderedText =
                    detail::makeMarkdownAttributedString(markdown.text, selectionStyle);
                detail::updateSelectableMarkdownLayout(
                    state,
                    layout,
                    std::move(renderedText.utf8),
                    layout ? layout->measuredSize.width : 0.f
                );
            };

        flux::Element content = flux::Element{flux::Render{
            .measureFn = [layoutForFrame, widthForConstraints](
                             flux::LayoutConstraints const &constraints,
                             flux::LayoutHints const &) {
                std::shared_ptr<flux::TextLayout const> textLayout =
                    layoutForFrame(widthForConstraints(constraints));
                return textLayout ? textLayout->measuredSize : flux::Size {};
            },
            .draw = [layoutForFrame, selectableState, updateSelectionLayout, theme, selectionColor = selectionColor](
                        flux::Canvas &canvas,
                        flux::Rect frame) {
                std::shared_ptr<flux::TextLayout const> textLayout = layoutForFrame(frame.width);
                if (!textLayout) {
                    return;
                }

                if (selectableState) {
                    updateSelectionLayout(*selectableState, textLayout);
                    if (selectableState->selection.hasSelection()) {
                        flux::Color const resolvedSelectionColor =
                            flux::resolveColor(selectionColor, theme().selectedContentBackgroundColor, theme());
                        for (flux::Rect const &rect : flux::detail::selectionRects(
                                 selectableState->layoutResult,
                                 selectableState->selection,
                                 &selectableState->text,
                                 0.f,
                                 0.f
                             )) {
                            canvas.drawRect(
                                rect,
                                flux::CornerRadius {},
                                flux::FillStyle::solid(resolvedSelectionColor),
                                flux::StrokeStyle::none()
                            );
                        }
                    }
                }

                canvas.drawTextLayout(*textLayout, flux::Point {});
            },
            .pure = !selectable,
        }};

        if (selectable && selectableState) {
            content = std::move(content)
                          .focusable(true)
                          .cursor(flux::Cursor::IBeam)
                          .onPointerDown([state = selectableState](flux::Point local) {
                              detail::handleSelectableMarkdownPointerDown(*state, local);
                          })
                          .onPointerMove([state = selectableState](flux::Point local) {
                              detail::handleSelectableMarkdownPointerDrag(*state, local);
                          })
                          .onPointerUp([state = selectableState](flux::Point) {
                              detail::handleSelectableMarkdownPointerUp(*state);
                          })
                          .onKeyDown([state = selectableState](flux::KeyCode key, flux::Modifiers modifiers) {
                              detail::handleSelectableMarkdownKey(*state, key, modifiers);
                          });
        }
        return content;
    }
};

} // namespace lambda
