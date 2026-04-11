#include <Flux/UI/Views/TextEditBehavior.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/Core/Clipboard.hpp>
#include <Flux/Core/KeyCodes.hpp>
#include <Flux/Reactive/AnimationClock.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/Views/TextEditUtils.hpp>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace flux {

namespace {

/// Full blink cycle duration (seconds). Lower = faster caret blink.
constexpr double kCaretBlinkPeriodSec = 1.f;

/// Caret/selection live in TextEditBehavior, not in the bound `Signal<std::string>`, so moving the caret does
/// not mark reactive state dirty. Pointer paths call `markReactiveDirty()` in InputDispatcher; keyboard
/// navigation must request a redraw the same way.
void markTextUiDirty() {
    if (Application::hasInstance()) {
        Application::instance().markReactiveDirty();
    }
}

bool hasMod(Modifiers m, Modifiers bit) noexcept {
    return (static_cast<std::uint32_t>(m) & static_cast<std::uint32_t>(bit)) != 0;
}

bool cmdLike(Modifiers m) noexcept {
    return hasMod(m, Modifiers::Meta) || hasMod(m, Modifiers::Ctrl);
}

bool textInputDebugEnabled() {
    static bool const enabled = [] {
        char const *env = std::getenv("FLUX_DEBUG_TEXT_INPUT");
        return env && env[0] != '\0' && env[0] != '0';
    }();
    return enabled;
}

char const *keyName(KeyCode key) noexcept {
    switch (key) {
    case keys::A:
        return "A";
    case keys::C:
        return "C";
    case keys::V:
        return "V";
    case keys::X:
        return "X";
    case keys::Z:
        return "Z";
    case keys::Return:
        return "Return";
    case keys::Tab:
        return "Tab";
    case keys::Delete:
        return "Delete";
    case keys::ForwardDelete:
        return "ForwardDelete";
    case keys::Escape:
        return "Escape";
    case keys::LeftArrow:
        return "LeftArrow";
    case keys::RightArrow:
        return "RightArrow";
    case keys::UpArrow:
        return "UpArrow";
    case keys::DownArrow:
        return "DownArrow";
    case keys::Home:
        return "Home";
    case keys::End:
        return "End";
    default:
        return "Unknown";
    }
}

void formatModifiers(char *buffer, std::size_t size, Modifiers m) {
    if (size == 0) {
        return;
    }
    buffer[0] = '\0';
    if (m == Modifiers::None) {
        std::snprintf(buffer, size, "None");
        return;
    }

    std::size_t used = 0;
    bool first = true;
    auto append = [&](char const *label, Modifiers flag) {
        if (!hasMod(m, flag)) {
            return;
        }
        int const written =
            std::snprintf(buffer + used, size > used ? size - used : 0, "%s%s", first ? "" : "+", label);
        if (written > 0) {
            used += static_cast<std::size_t>(written);
        }
        first = false;
    };
    append("Shift", Modifiers::Shift);
    append("Ctrl", Modifiers::Ctrl);
    append("Alt", Modifiers::Alt);
    append("Meta", Modifiers::Meta);
}

void logTextInputDebug(char const *phase, char const *event, detail::TextEditState const &state, std::string const &value,
                       char const *extra = nullptr) {
    if (!textInputDebugEnabled()) {
        return;
    }
    auto [a, b] = state.selection.ordered();
    std::fprintf(stderr,
                 "[TextInput] %s event=%s caret=%d anchor=%d selection=[%d,%d] len=%zu focused=%d disabled=%d dragging=%d%s%s\n",
                 phase, event, state.selection.caretByte, state.selection.anchorByte, a, b, value.size(),
                 state.focused ? 1 : 0, state.disabled ? 1 : 0, state.draggingSelection ? 1 : 0,
                 extra ? " " : "", extra ? extra : "");
    std::fflush(stderr);
}

} // namespace

TextEditBehavior::TextEditBehavior(Signal<std::string> &value, TextEditBehaviorOptions const &opts)
    : value_(&value), opts_(opts) {
    multilineLocked_ = opts.multiline;
    state_.multiline = opts.multiline;
    std::string const &s = value_->get();
    state_.selection.caretByte = detail::utf8Clamp(s, static_cast<int>(s.size()));
    state_.selection.anchorByte = state_.selection.caretByte;
    blinkEpoch_ = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    );
}

TextEditBehavior::~TextEditBehavior() {
    if (blinkSubscribed_) {
        AnimationClock::instance().unsubscribe(blinkHandle_);
    }
}

void TextEditBehavior::syncOptions(TextEditBehaviorOptions const &opts) {
    assert(opts.multiline == multilineLocked_ && "TextEditBehaviorOptions::multiline cannot change");
    opts_ = opts;
}

std::string const &TextEditBehavior::value() const {
    return value_->get();
}

std::pair<int, int> TextEditBehavior::orderedSelection() const {
    return state_.selection.ordered();
}

void TextEditBehavior::resetBlinkEpoch() {
    blinkEpoch_ = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    );
    blinkVisible_ = true;
}

bool TextEditBehavior::blinkVisibleAt(std::chrono::nanoseconds now) const {
    if (!state_.focused || state_.disabled) {
        return true;
    }
    double const elapsed = std::chrono::duration<double>(now - blinkEpoch_).count();
    double const period = kCaretBlinkPeriodSec;
    double const phase = std::fmod(elapsed, period) / period;
    return phase <= 0.5;
}

void TextEditBehavior::syncBlinkSubscription() {
    bool const want = state_.focused && !state_.disabled;
    if (want && !blinkSubscribed_) {
        blinkVisible_ = true;
        blinkHandle_ = AnimationClock::instance().subscribe([this](AnimationTick const &tick) {
            bool const visible = blinkVisibleAt(std::chrono::nanoseconds {tick.deadlineNanos});
            if (visible == blinkVisible_) {
                return;
            }
            blinkVisible_ = visible;
            if (Application::hasInstance()) {
                Application::instance().requestRedraw();
            }
        });
        blinkSubscribed_ = true;
    } else if (!want && blinkSubscribed_) {
        AnimationClock::instance().unsubscribe(blinkHandle_);
        blinkHandle_ = {};
        blinkSubscribed_ = false;
    }
}

float TextEditBehavior::caretBlinkPhase() const {
    if (!state_.focused || state_.disabled) {
        return 1.f;
    }
    auto const t0 = std::chrono::steady_clock::time_point(
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(blinkEpoch_)
    );
    double const elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    double const period = kCaretBlinkPeriodSec;
    return static_cast<float>(std::fmod(elapsed, period) / period);
}

void TextEditBehavior::setFocused(bool f) {
    if (state_.focused == f) {
        return;
    }
    state_.focused = f;
    resetBlinkEpoch();
    syncBlinkSubscription();
    markTextUiDirty();
    logTextInputDebug("post", "focus", state_, value_->get(), f ? "focused=1" : "focused=0");
}

void TextEditBehavior::setDisabled(bool d) {
    if (state_.disabled == d) {
        return;
    }
    state_.disabled = d;
    resetBlinkEpoch();
    syncBlinkSubscription();
    markTextUiDirty();
    logTextInputDebug("post", "disabled", state_, value_->get(), d ? "disabled=1" : "disabled=0");
}

void TextEditBehavior::moveCaretTo(int byte, bool extendSelection) {
    std::string const &s = value_->get();
    state_.selection = detail::moveSelectionToByte(s, state_.selection, byte, extendSelection);
    ensureCaretVisible_ = true;
    resetBlinkEpoch();
    markTextUiDirty();
    logTextInputDebug("post", "moveCaretTo", state_, s);
}

void TextEditBehavior::selectAll() {
    std::string const &s = value_->get();
    state_.selection = detail::selectAllSelection(s);
    ensureCaretVisible_ = true;
    resetBlinkEpoch();
    markTextUiDirty();
    logTextInputDebug("post", "selectAll", state_, s);
}

void TextEditBehavior::clearSelection() {
    if (!state_.selection.hasSelection()) {
        return;
    }
    state_.selection = detail::clearSelection(state_.selection);
    markTextUiDirty();
    logTextInputDebug("post", "clearSelection", state_, value_->get());
}

bool TextEditBehavior::consumeEnsureCaretVisibleRequest() {
    bool const r = ensureCaretVisible_;
    ensureCaretVisible_ = false;
    return r;
}

void TextEditBehavior::pushUndo(UndoEntry entry, bool coalesceTyping) {
    if (coalesceTyping && !undo_.empty() && undo_.back().coalescable) {
        auto &top = undo_.back();
        auto const now = std::chrono::steady_clock::now();
        if (top.valueAfter == entry.valueBefore && (now - top.timestamp) <= kCoalesceWindow) {
            top.valueAfter = std::move(entry.valueAfter);
            top.caretAfter = entry.caretAfter;
            top.anchorAfter = entry.anchorAfter;
            top.timestamp = now;
            return;
        }
    }
    undo_.push_back(std::move(entry));
    while (static_cast<int>(undo_.size()) > kMaxUndoDepth) {
        undo_.pop_front();
    }
}

void TextEditBehavior::mutateValue(std::string next, int newCaret, int newAnchor, bool recordUndo,
                                   bool coalesceTyping) {
    std::string const before = value_->get();
    bool const valueChanged = before != next;
    if (!valueChanged) {
        int const nc = detail::utf8Clamp(next, newCaret);
        int const na = detail::utf8Clamp(next, newAnchor);
        if (nc == state_.selection.caretByte && na == state_.selection.anchorByte) {
            return;
        }
        state_.selection.caretByte = nc;
        state_.selection.anchorByte = na;
        ensureCaretVisible_ = true;
        resetBlinkEpoch();
        markTextUiDirty();
        logTextInputDebug("post", "mutate(no-value-change)", state_, next);
        return;
    }

    if (recordUndo) {
        UndoEntry e {};
        e.valueBefore = before;
        e.caretBefore = state_.selection.caretByte;
        e.anchorBefore = state_.selection.anchorByte;
        e.valueAfter = next;
        e.caretAfter = newCaret;
        e.anchorAfter = newAnchor;
        e.timestamp = std::chrono::steady_clock::now();
        e.coalescable = coalesceTyping;
        pushUndo(std::move(e), coalesceTyping);
    }

    state_.selection.caretByte = detail::utf8Clamp(next, newCaret);
    state_.selection.anchorByte = detail::utf8Clamp(next, newAnchor);
    value_->set(std::move(next));
    redo_.clear();

    if (opts_.onChange) {
        opts_.onChange(value_->get());
    }
    ensureCaretVisible_ = true;
    resetBlinkEpoch();
    logTextInputDebug("post", "mutate", state_, value_->get());
}

void TextEditBehavior::insertAtCaret(std::string_view insert, bool fromTyping) {
    detail::TextEditMutation mutation = detail::insertText(value_->get(), state_.selection, insert, opts_.maxLength);
    mutateValue(std::move(mutation.text), mutation.selection.caretByte, mutation.selection.anchorByte, true,
                fromTyping && mutation.coalescableTyping);
}

void TextEditBehavior::deleteSelectionOrChar(bool forward) {
    detail::TextEditMutation mutation = detail::eraseSelectionOrChar(value_->get(), state_.selection, forward);
    mutateValue(std::move(mutation.text), mutation.selection.caretByte, mutation.selection.anchorByte, true, false);
}

void TextEditBehavior::deleteWord(bool forward) {
    detail::TextEditMutation mutation = detail::eraseWord(value_->get(), state_.selection, forward);
    mutateValue(std::move(mutation.text), mutation.selection.caretByte, mutation.selection.anchorByte, true, false);
}

void TextEditBehavior::deleteToLineBoundary(bool forward) {
    detail::TextEditMutation mutation = detail::eraseToLineBoundary(value_->get(), state_.selection, forward);
    mutateValue(std::move(mutation.text), mutation.selection.caretByte, mutation.selection.anchorByte, true, false);
}

void TextEditBehavior::copySelection(bool cut) {
    if (!Application::hasInstance()) {
        return;
    }
    std::string const &buf = value_->get();
    auto [a, b] = state_.selection.ordered();
    a = detail::utf8Clamp(buf, a);
    b = detail::utf8Clamp(buf, b);
    if (a >= b) {
        return;
    }
    std::string const chunk = buf.substr(static_cast<std::size_t>(a), static_cast<std::size_t>(b - a));
    Application::instance().clipboard().writeText(chunk);
    if (cut) {
        mutateValue(buf.substr(0, static_cast<std::size_t>(a)) + buf.substr(static_cast<std::size_t>(b)), a, a, true,
                    false);
    }
}

void TextEditBehavior::paste() {
    if (!Application::hasInstance()) {
        return;
    }
    auto t = Application::instance().clipboard().readText();
    if (!t || t->empty()) {
        return;
    }
    insertAtCaret(*t, false);
}

void TextEditBehavior::moveLineBoundary(bool end, bool extend) {
    state_.selection = detail::moveSelectionToLineBoundary(value_->get(), state_.selection, end, extend);
    ensureCaretVisible_ = true;
    resetBlinkEpoch();
    markTextUiDirty();
}

void TextEditBehavior::moveDocumentBoundary(bool end, bool extend) {
    state_.selection = detail::moveSelectionToDocumentBoundary(value_->get(), state_.selection, end, extend);
    ensureCaretVisible_ = true;
    resetBlinkEpoch();
    markTextUiDirty();
}

void TextEditBehavior::moveWord(int dir, bool extend) {
    state_.selection = detail::moveSelectionByWord(value_->get(), state_.selection, dir, extend);
    ensureCaretVisible_ = true;
    resetBlinkEpoch();
    markTextUiDirty();
}

void TextEditBehavior::moveChar(int dir, bool extend) {
    state_.selection = detail::moveSelectionByChar(value_->get(), state_.selection, dir, extend);
    ensureCaretVisible_ = true;
    resetBlinkEpoch();
    markTextUiDirty();
}

bool TextEditBehavior::handleKey(KeyEvent const &e) {
    if (state_.disabled) {
        logTextInputDebug("pre", "key(ignored-disabled)", state_, value_->get());
        return false;
    }

    Modifiers const m = e.modifiers;
    bool const shift = hasMod(m, Modifiers::Shift);
    bool const alt = hasMod(m, Modifiers::Alt);
    bool const cmd = cmdLike(m);
    char mods[32];
    formatModifiers(mods, sizeof(mods), m);
    char extra[96];
    std::snprintf(extra, sizeof(extra), "key=%s(0x%X) mods=%s", keyName(e.key), static_cast<unsigned>(e.key), mods);
    logTextInputDebug("pre", "key", state_, value_->get(), extra);
    auto finish = [&](bool handled) {
        logTextInputDebug("post", handled ? "key" : "key(unhandled)", state_, value_->get(), extra);
        return handled;
    };

    if (e.key == keys::Escape) {
        if (opts_.onEscape) {
            opts_.onEscape(value_->get());
            return finish(true);
        }
        return finish(false);
    }

    if (cmd && !shift && e.key == keys::A) {
        selectAll();
        return finish(true);
    }
    if (cmd && !shift && e.key == keys::C) {
        copySelection(false);
        return finish(true);
    }
    if (cmd && !shift && e.key == keys::X) {
        copySelection(true);
        return finish(true);
    }
    if (cmd && !shift && e.key == keys::V) {
        paste();
        return finish(true);
    }
    if (cmd && e.key == keys::Z) {
        if (shift) {
            redo();
        } else {
            undo();
        }
        return finish(true);
    }

    if (e.key == keys::Tab) {
        if (opts_.acceptsTab && opts_.multiline) {
            insertAtCaret("\t", false);
            return finish(true);
        }
        return finish(false);
    }

    if (e.key == keys::Return) {
        if (opts_.submitsOnEnter) {
            if (opts_.onChange) {
                opts_.onChange(value_->get());
            }
            if (opts_.onSubmit) {
                opts_.onSubmit(value_->get());
            }
            return finish(true);
        }
        if (opts_.multiline) {
            insertAtCaret("\n", false);
            return finish(true);
        }
        return finish(false);
    }

    if (e.key == keys::Delete) {
        if (cmd) {
            deleteToLineBoundary(false);
        } else if (alt) {
            deleteWord(false);
        } else {
            deleteSelectionOrChar(false);
        }
        return finish(true);
    }
    if (e.key == keys::ForwardDelete) {
        if (cmd) {
            deleteToLineBoundary(true);
        } else if (alt) {
            deleteWord(true);
        } else {
            deleteSelectionOrChar(true);
        }
        return finish(true);
    }

    if (e.key == keys::LeftArrow) {
        if (cmd && !opts_.multiline) {
            moveCaretTo(0, shift);
            return finish(true);
        }
        if (cmd && opts_.multiline) {
            moveLineBoundary(false, shift);
            return finish(true);
        }
        if (alt) {
            moveWord(-1, shift);
            return finish(true);
        }
        moveChar(-1, shift);
        return finish(true);
    }
    if (e.key == keys::RightArrow) {
        if (cmd && !opts_.multiline) {
            moveCaretTo(static_cast<int>(value_->get().size()), shift);
            return finish(true);
        }
        if (cmd && opts_.multiline) {
            moveLineBoundary(true, shift);
            return finish(true);
        }
        if (alt) {
            moveWord(1, shift);
            return finish(true);
        }
        moveChar(1, shift);
        return finish(true);
    }

    if (e.key == keys::Home) {
        moveLineBoundary(false, shift);
        return finish(true);
    }
    if (e.key == keys::End) {
        moveLineBoundary(true, shift);
        return finish(true);
    }

    if (e.key == keys::UpArrow) {
        if (cmd && opts_.multiline) {
            moveDocumentBoundary(false, shift);
            return finish(true);
        }
        if (opts_.multiline && opts_.verticalResolver) {
            int const n = opts_.verticalResolver(state_.selection.caretByte, -1);
            moveCaretTo(n, shift);
            return finish(true);
        }
        return finish(false);
    }
    if (e.key == keys::DownArrow) {
        if (cmd && opts_.multiline) {
            moveDocumentBoundary(true, shift);
            return finish(true);
        }
        if (opts_.multiline && opts_.verticalResolver) {
            int const n = opts_.verticalResolver(state_.selection.caretByte, 1);
            moveCaretTo(n, shift);
            return finish(true);
        }
        return finish(false);
    }

    return finish(false);
}

bool TextEditBehavior::canUndo() const {
    return !undo_.empty();
}

bool TextEditBehavior::canRedo() const {
    return !redo_.empty();
}

void TextEditBehavior::undo() {
    if (undo_.empty()) {
        return;
    }
    UndoEntry eEntry = std::move(undo_.back());
    undo_.pop_back();
    value_->set(eEntry.valueBefore);
    state_.selection.caretByte = detail::utf8Clamp(value_->get(), eEntry.caretBefore);
    state_.selection.anchorByte = detail::utf8Clamp(value_->get(), eEntry.anchorBefore);
    redo_.push_back(std::move(eEntry));
    if (opts_.onChange) {
        opts_.onChange(value_->get());
    }
    ensureCaretVisible_ = true;
    resetBlinkEpoch();
}

void TextEditBehavior::redo() {
    if (redo_.empty()) {
        return;
    }
    UndoEntry eEntry = std::move(redo_.back());
    redo_.pop_back();
    value_->set(eEntry.valueAfter);
    state_.selection.caretByte = detail::utf8Clamp(value_->get(), eEntry.caretAfter);
    state_.selection.anchorByte = detail::utf8Clamp(value_->get(), eEntry.anchorAfter);
    undo_.push_back(std::move(eEntry));
    if (opts_.onChange) {
        opts_.onChange(value_->get());
    }
    ensureCaretVisible_ = true;
    resetBlinkEpoch();
}

bool TextEditBehavior::handleTextInput(std::string_view utf8) {
    if (state_.disabled || utf8.empty()) {
        char extra[96];
        std::snprintf(extra, sizeof(extra), "bytes=%zu ignored=%d", static_cast<std::size_t>(utf8.size()),
                      (state_.disabled || utf8.empty()) ? 1 : 0);
        logTextInputDebug("pre", "text", state_, value_->get(), extra);
        return false;
    }
    char extra[96];
    std::snprintf(extra, sizeof(extra), "bytes=%zu", static_cast<std::size_t>(utf8.size()));
    logTextInputDebug("pre", "text", state_, value_->get(), extra);
    insertAtCaret(utf8, true);
    return true;
}

bool TextEditBehavior::handlePointerDown(int byte, bool shift) {
    if (state_.disabled) {
        char extra[96];
        std::snprintf(extra, sizeof(extra), "byte=%d shift=%d ignored=1", byte, shift ? 1 : 0);
        logTextInputDebug("pre", "pointerDown", state_, value_->get(), extra);
        return false;
    }
    char extra[96];
    std::snprintf(extra, sizeof(extra), "byte=%d shift=%d", byte, shift ? 1 : 0);
    logTextInputDebug("pre", "pointerDown", state_, value_->get(), extra);
    state_.draggingSelection = true;
    std::string const &s = value_->get();
    int const b = detail::utf8Clamp(s, byte);
    state_.selection.caretByte = b;
    if (!shift) {
        state_.selection.anchorByte = b;
    }
    ensureCaretVisible_ = true;
    resetBlinkEpoch();
    markTextUiDirty();
    logTextInputDebug("post", "pointerDown", state_, value_->get(), extra);
    return true;
}

void TextEditBehavior::handlePointerDrag(int byte) {
    if (!state_.draggingSelection || state_.disabled) {
        char extra[96];
        std::snprintf(extra, sizeof(extra), "byte=%d ignored=1", byte);
        logTextInputDebug("pre", "pointerDrag", state_, value_->get(), extra);
        return;
    }
    char extra[96];
    std::snprintf(extra, sizeof(extra), "byte=%d", byte);
    logTextInputDebug("pre", "pointerDrag", state_, value_->get(), extra);
    std::string const &s = value_->get();
    state_.selection.caretByte = detail::utf8Clamp(s, byte);
    ensureCaretVisible_ = true;
    resetBlinkEpoch();
    markTextUiDirty();
    logTextInputDebug("post", "pointerDrag", state_, value_->get(), extra);
}

void TextEditBehavior::handlePointerUp() {
    state_.draggingSelection = false;
    logTextInputDebug("post", "pointerUp", state_, value_->get());
}

TextEditBehavior &useTextEditBehavior(State<std::string> value, TextEditBehaviorOptions const &opts) {
    StateStore *store = StateStore::current();
    assert(store && value.signal);
    TextEditBehavior &b = store->claimSlot<TextEditBehavior>(*value.signal, opts);
    b.syncOptions(opts);
    return b;
}

} // namespace flux
