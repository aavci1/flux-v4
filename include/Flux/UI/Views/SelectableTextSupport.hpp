#pragma once

#include <Flux/Core/Application.hpp>
#include <Flux/Core/Clipboard.hpp>
#include <Flux/Core/KeyCodes.hpp>
#include <Flux/Graphics/TextLayout.hpp>
#include <Flux/UI/ComponentKey.hpp>
#include <Flux/UI/Views/TextEditUtils.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <unordered_map>

namespace flux::detail {

struct SelectableTextClickTracker {
    Point lastPoint {};
    std::chrono::steady_clock::time_point lastClickAt {};
    int clickCount = 0;
};

struct SelectableTextState {
    TextEditSelection selection {};
    bool dragging = false;
    SelectableTextClickTracker clickTracker {};
    TextEditLayoutResult layoutResult {};
    std::string text;
};

inline bool selectableTextHasMod(Modifiers m, Modifiers bit) noexcept {
    return (static_cast<std::uint32_t>(m) & static_cast<std::uint32_t>(bit)) != 0;
}

inline bool selectableTextCmdLike(Modifiers m) noexcept {
    return selectableTextHasMod(m, Modifiers::Meta) || selectableTextHasMod(m, Modifiers::Ctrl);
}

inline void markSelectableTextDirty() {
    if (!Application::hasInstance()) {
        return;
    }
    Application::instance().markReactiveDirty();
    Application::instance().requestRedraw();
}

inline std::shared_ptr<SelectableTextState> selectableTextState(ComponentKey const &key) {
    static std::unordered_map<ComponentKey, std::weak_ptr<SelectableTextState>, ComponentKeyHash> cache;

    if (cache.size() > 1024) {
        for (auto it = cache.begin(); it != cache.end();) {
            if (it->second.expired()) {
                it = cache.erase(it);
            } else {
                ++it;
            }
        }
        if (cache.size() > 1024) {
            cache.clear();
        }
    }

    if (auto it = cache.find(key); it != cache.end()) {
        if (std::shared_ptr<SelectableTextState> existing = it->second.lock()) {
            return existing;
        }
    }

    std::shared_ptr<SelectableTextState> state = std::make_shared<SelectableTextState>();
    cache[key] = state;
    return state;
}

inline void updateSelectableTextLayout(
    SelectableTextState &state,
    std::shared_ptr<TextLayout const> layout,
    std::string text,
    float contentWidth
) {
    state.text = std::move(text);
    state.layoutResult = makeTextEditLayoutResult(
        std::move(layout),
        static_cast<int>(state.text.size()),
        contentWidth
    );
    state.selection = clampSelection(state.text, state.selection);
}

inline void copySelectableTextSelection(SelectableTextState const &state) {
    if (!Application::hasInstance()) {
        return;
    }
    auto const [start, end] = state.selection.ordered();
    if (start >= end || start < 0 || end > static_cast<int>(state.text.size())) {
        return;
    }
    Application::instance().clipboard().writeText(
        state.text.substr(static_cast<std::size_t>(start), static_cast<std::size_t>(end - start))
    );
}

inline int registerSelectableTextClick(SelectableTextClickTracker &tracker, Point local) {
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

inline void handleSelectableTextPointerDown(SelectableTextState &state, Point local) {
    int const byte = caretByteAtPoint(state.layoutResult, local, state.text);
    int const clickCount = registerSelectableTextClick(state.clickTracker, local);
    state.dragging = (clickCount == 1);
    if (clickCount >= 3) {
        state.selection = selectAllSelection(state.text);
    } else if (clickCount == 2) {
        state.selection = wordSelectionAtByte(state.text, byte);
    } else {
        state.selection = moveSelectionToByte(state.text, state.selection, byte, false);
    }
    markSelectableTextDirty();
}

inline void handleSelectableTextPointerDrag(SelectableTextState &state, Point local) {
    if (!state.dragging) {
        return;
    }
    int const byte = caretByteAtPoint(state.layoutResult, local, state.text);
    state.selection = moveSelectionToByte(state.text, state.selection, byte, true);
    markSelectableTextDirty();
}

inline void handleSelectableTextPointerUp(SelectableTextState &state) {
    state.dragging = false;
}

inline bool handleSelectableTextKey(SelectableTextState &state, KeyCode key, Modifiers modifiers) {
    bool const shift = selectableTextHasMod(modifiers, Modifiers::Shift);
    bool const alt = selectableTextHasMod(modifiers, Modifiers::Alt);
    bool const cmd = selectableTextCmdLike(modifiers);

    if (cmd && !shift && key == keys::A) {
        state.selection = selectAllSelection(state.text);
        markSelectableTextDirty();
        return true;
    }
    if (cmd && !shift && key == keys::C) {
        copySelectableTextSelection(state);
        return true;
    }
    if (key == keys::LeftArrow) {
        state.selection = cmd ? moveSelectionToDocumentBoundary(state.text, state.selection, false, shift) :
                         alt ? moveSelectionByWord(state.text, state.selection, -1, shift) :
                               moveSelectionByChar(state.text, state.selection, -1, shift);
        markSelectableTextDirty();
        return true;
    }
    if (key == keys::RightArrow) {
        state.selection = cmd ? moveSelectionToDocumentBoundary(state.text, state.selection, true, shift) :
                         alt ? moveSelectionByWord(state.text, state.selection, 1, shift) :
                               moveSelectionByChar(state.text, state.selection, 1, shift);
        markSelectableTextDirty();
        return true;
    }
    if (key == keys::Home) {
        state.selection = moveSelectionToDocumentBoundary(state.text, state.selection, false, shift);
        markSelectableTextDirty();
        return true;
    }
    if (key == keys::End) {
        state.selection = moveSelectionToDocumentBoundary(state.text, state.selection, true, shift);
        markSelectableTextDirty();
        return true;
    }
    if ((key == keys::UpArrow || key == keys::DownArrow) && state.layoutResult.lines.size() > 1) {
        int const caret = moveCaretVertically(
            state.layoutResult,
            state.text,
            state.selection.caretByte,
            key == keys::UpArrow ? -1 : 1
        );
        state.selection = moveSelectionToByte(state.text, state.selection, caret, shift);
        markSelectableTextDirty();
        return true;
    }
    return false;
}

} // namespace flux::detail
