#pragma once

/// \file Flux/UI/Views/TextEditBehavior.hpp
///
/// Stateful editing controller shared by text-entry components.

#include <Flux/Core/Types.hpp>
#include <Flux/Reactive/Observer.hpp>
#include <Flux/Reactive/Signal.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Views/TextEditUtils.hpp>

#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

namespace flux {

struct KeyEvent {
    KeyCode key = 0;
    Modifiers modifiers = Modifiers::None;
};

struct TextEditBehaviorOptions {
    bool multiline = false;
    int maxLength = 0;
    bool acceptsTab = false;
    bool submitsOnEnter = false;
    std::function<void(std::string const &)> onChange;
    std::function<void(std::string const &)> onSubmit;
    std::function<void(std::string const &)> onEscape;
    /// +1 = down, -1 = up. Ignored when null or single-line.
    std::function<int(int currentCaretByte, int direction)> verticalResolver;
};

class TextEditBehavior {
  public:
    TextEditBehavior(Signal<std::string> &value, TextEditBehaviorOptions const &opts);
    ~TextEditBehavior();

    void syncOptions(TextEditBehaviorOptions const &opts);

    std::string const &value() const;
    int caretByte() const { return state_.selection.caretByte; }
    int selectionAnchorByte() const { return state_.selection.anchorByte; }
    std::pair<int, int> orderedSelection() const;
    bool hasSelection() const { return state_.selection.hasSelection(); }
    bool focused() const { return state_.focused; }
    bool disabled() const { return state_.disabled; }

    float caretBlinkPhase() const;

    void setFocused(bool f);
    void setDisabled(bool d);

    void moveCaretTo(int byte, bool extendSelection);
    void selectAll();
    void selectWordAt(int byte);
    void clearSelection();

    bool handleKey(KeyEvent const &e);
    bool handleTextInput(std::string_view utf8);
    bool handlePointerDown(int byte, bool shift);
    void handlePointerDrag(int byte);
    void handlePointerUp();

    bool canUndo() const;
    bool canRedo() const;
    void undo();
    void redo();

    bool consumeEnsureCaretVisibleRequest();

  private:
    struct UndoEntry {
        std::string valueBefore;
        int caretBefore = 0;
        int anchorBefore = 0;
        std::string valueAfter;
        int caretAfter = 0;
        int anchorAfter = 0;
        std::chrono::steady_clock::time_point timestamp {};
        bool coalescable = false;
    };

    void mutateValue(std::string next, int newCaret, int newAnchor, bool recordUndo, bool coalesceTyping);
    void insertAtCaret(std::string_view text, bool fromTyping);
    void deleteSelectionOrChar(bool forward);
    void deleteWord(bool forward);
    void deleteToLineBoundary(bool forward);
    void copySelection(bool cut);
    void paste();
    void moveLineBoundary(bool end, bool extend);
    void moveDocumentBoundary(bool end, bool extend);
    void moveWord(int dir, bool extend);
    void moveChar(int dir, bool extend);
    void resetBlinkEpoch();
    bool blinkVisibleAt(std::chrono::nanoseconds now) const;
    void syncBlinkSubscription();
    void pushUndo(UndoEntry entry, bool coalesceTyping);

    Signal<std::string> *value_ {};
    TextEditBehaviorOptions opts_ {};

    detail::TextEditState state_ {};

    bool ensureCaretVisible_ = false;

    std::deque<UndoEntry> undo_;
    std::deque<UndoEntry> redo_;

    std::chrono::nanoseconds blinkEpoch_ {};
    bool blinkVisible_ = true;
    bool blinkSubscribed_ = false;
    ObserverHandle blinkHandle_ {};

    bool multilineLocked_ = false;

    static constexpr int kMaxUndoDepth = 200;
    static constexpr std::chrono::milliseconds kCoalesceWindow {500};
};

TextEditBehavior &useTextEditBehavior(State<std::string> value, TextEditBehaviorOptions const &opts = {});

} // namespace flux
