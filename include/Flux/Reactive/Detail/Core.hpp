#pragma once

#include <Flux/Reactive/SmallFn.hpp>
#include <Flux/Reactive/Transition.hpp>

#include <atomic>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace flux::Reactive {

namespace detail {

enum Flag : std::uint16_t {
  Mutable = 1u << 0u,
  Watching = 1u << 1u,
  Recursed = 1u << 2u,
  Dirty = 1u << 3u,
  Pending = 1u << 4u,
  Disposed = 1u << 5u,
};

inline bool hasFlag(std::uint16_t flags, Flag flag) {
  return (flags & static_cast<std::uint16_t>(flag)) != 0u;
}

inline void setFlag(std::uint16_t& flags, Flag flag) {
  flags |= static_cast<std::uint16_t>(flag);
}

inline void clearFlag(std::uint16_t& flags, Flag flag) {
  flags &= static_cast<std::uint16_t>(~static_cast<std::uint16_t>(flag));
}

struct Disposable {
  virtual ~Disposable() = default;
  virtual void dispose() = 0;
  virtual bool disposed() const = 0;
};

struct Observable;
struct Computation;

struct Link {
  Observable* source = nullptr;
  Computation* observer = nullptr;
  std::uint64_t sourceVersion = 0;
  Link* nextSource = nullptr;
  Link* prevSource = nullptr;
  Link* nextSubscriber = nullptr;
  Link* prevSubscriber = nullptr;
};

inline std::atomic_size_t gLiveLinks = 0;
inline std::atomic_size_t gTotalLinks = 0;

inline Link* allocateLink() {
  gLiveLinks.fetch_add(1, std::memory_order_relaxed);
  gTotalLinks.fetch_add(1, std::memory_order_relaxed);
  return new Link();
}

inline void freeLink(Link* link) {
  if (!link) {
    return;
  }
  gLiveLinks.fetch_sub(1, std::memory_order_relaxed);
  delete link;
}

inline std::size_t debugLiveLinkCount() {
  return gLiveLinks.load(std::memory_order_relaxed);
}

inline std::size_t debugTotalLinkAllocations() {
  return gTotalLinks.load(std::memory_order_relaxed);
}

inline void debugResetLinkAllocationCount() {
  gTotalLinks.store(0, std::memory_order_relaxed);
}

struct ScopeState {
  bool disposed = false;
  std::vector<std::shared_ptr<Disposable>> owned;
  std::vector<SmallFn<void()>> cleanups;

  ~ScopeState() {
    dispose();
  }

  void own(std::shared_ptr<Disposable> disposable) {
    assert(!disposed && "cannot add reactive owner to a disposed scope");
    owned.push_back(std::move(disposable));
  }

  void onCleanup(SmallFn<void()> cleanup) {
    assert(!disposed && "cannot add cleanup to a disposed scope");
    cleanups.push_back(std::move(cleanup));
  }

  void dispose() {
    if (disposed) {
      return;
    }
    disposed = true;

    for (auto it = cleanups.rbegin(); it != cleanups.rend(); ++it) {
      (*it)();
    }
    cleanups.clear();

    for (auto it = owned.rbegin(); it != owned.rend(); ++it) {
      if (*it) {
        (*it)->dispose();
      }
    }
    owned.clear();
  }
};

inline thread_local Computation* sCurrentObserver = nullptr;
inline thread_local ScopeState* sCurrentOwner = nullptr;
inline thread_local int sBatchDepth = 0;
inline thread_local std::vector<Computation*> sEffectQueue;

inline void ownNode(std::shared_ptr<Disposable> disposable) {
  if (sCurrentOwner) {
    sCurrentOwner->own(std::move(disposable));
  }
}

struct ObserverContext {
  Computation* previous = nullptr;

  explicit ObserverContext(Computation* next)
      : previous(sCurrentObserver) {
    sCurrentObserver = next;
  }

  ~ObserverContext() {
    sCurrentObserver = previous;
  }
};

struct OwnerContext {
  ScopeState* previous = nullptr;

  explicit OwnerContext(ScopeState* next)
      : previous(sCurrentOwner) {
    sCurrentOwner = next;
  }

  ~OwnerContext() {
    sCurrentOwner = previous;
  }
};

struct Observable : Disposable {
  Link* subscribers = nullptr;
  std::uint64_t version = 0;
  std::uint16_t flags = 0;

  ~Observable() override {
    dispose();
  }

  bool disposed() const override {
    return hasFlag(flags, Disposed);
  }

  void dispose() override;
  virtual bool updateIfNeeded();
  void reportRead();
  void subscribe(Computation& observer);
  void propagatePending();
  void propagateDirty();
  void detachSubscribers();
};

struct Computation : Observable {
  Link* sources = nullptr;
  Link* spareLinks = nullptr;
  bool scheduled = false;

  ~Computation() override {
    dispose();
    deleteSpareLinks();
  }

  void dispose() override;
  bool pollSourcesChanged();
  bool updateIfNeeded() override = 0;
  void markDirty();
  void markPending();
  void clearSourcesForReuse();
  void deleteSpareLinks();
  Link* findSource(Observable const* source) const;
  Link* acquireLink();
  void retireLink(Link* link);
  virtual void run() = 0;
  virtual void onDirty() = 0;
  virtual void onPending() = 0;
};

inline void unlinkFromSourceList(Link* link) {
  auto* observer = link->observer;
  if (!observer) {
    return;
  }
  if (link->prevSource) {
    link->prevSource->nextSource = link->nextSource;
  } else {
    observer->sources = link->nextSource;
  }
  if (link->nextSource) {
    link->nextSource->prevSource = link->prevSource;
  }
  link->nextSource = nullptr;
  link->prevSource = nullptr;
}

inline void unlinkFromSubscriberList(Link* link) {
  auto* source = link->source;
  if (!source) {
    return;
  }
  if (link->prevSubscriber) {
    link->prevSubscriber->nextSubscriber = link->nextSubscriber;
  } else {
    source->subscribers = link->nextSubscriber;
  }
  if (link->nextSubscriber) {
    link->nextSubscriber->prevSubscriber = link->prevSubscriber;
  }
  link->nextSubscriber = nullptr;
  link->prevSubscriber = nullptr;
}

inline void Observable::dispose() {
  if (disposed()) {
    return;
  }
  setFlag(flags, Disposed);
  detachSubscribers();
}

inline bool Observable::updateIfNeeded() {
  return false;
}

inline void Observable::reportRead() {
  if (!sCurrentObserver || disposed()) {
    return;
  }
  subscribe(*sCurrentObserver);
}

inline void Observable::subscribe(Computation& observer) {
  if (auto* existing = observer.findSource(this)) {
    existing->sourceVersion = version;
    return;
  }

  auto* link = observer.acquireLink();
  link->source = this;
  link->observer = &observer;
  link->sourceVersion = version;

  link->nextSubscriber = subscribers;
  if (subscribers) {
    subscribers->prevSubscriber = link;
  }
  subscribers = link;

  link->nextSource = observer.sources;
  if (observer.sources) {
    observer.sources->prevSource = link;
  }
  observer.sources = link;
}

inline void Observable::propagatePending() {
  auto* link = subscribers;
  while (link) {
    auto* next = link->nextSubscriber;
    if (link->observer) {
      link->observer->markPending();
    }
    link = next;
  }
}

inline void Observable::propagateDirty() {
  auto* link = subscribers;
  while (link) {
    auto* next = link->nextSubscriber;
    if (link->observer) {
      link->observer->markDirty();
    }
    link = next;
  }
}

inline void Observable::detachSubscribers() {
  while (subscribers) {
    auto* link = subscribers;
    auto* observer = link->observer;
    unlinkFromSubscriberList(link);
    unlinkFromSourceList(link);
    link->source = nullptr;
    link->observer = nullptr;
    if (observer) {
      observer->retireLink(link);
    } else {
      freeLink(link);
    }
  }
}

inline void Computation::dispose() {
  if (disposed()) {
    return;
  }
  setFlag(flags, Disposed);
  scheduled = false;
  detachSubscribers();
  clearSourcesForReuse();
}

inline bool Computation::pollSourcesChanged() {
  bool changed = false;
  auto* link = sources;
  while (link) {
    auto* next = link->nextSource;
    Observable* source = link->source;
    if (source) {
      bool const sourceChanged = source->updateIfNeeded();
      if (sourceChanged || link->sourceVersion != source->version) {
        changed = true;
      }
      link->sourceVersion = source->version;
    }
    link = next;
  }
  return changed;
}

inline void Computation::markDirty() {
  if (disposed() || hasFlag(flags, Dirty)) {
    return;
  }
  clearFlag(flags, Pending);
  setFlag(flags, Dirty);
  onDirty();
}

inline void Computation::markPending() {
  if (disposed() || hasFlag(flags, Dirty) || hasFlag(flags, Pending)) {
    return;
  }
  setFlag(flags, Pending);
  onPending();
}

inline void Computation::clearSourcesForReuse() {
  while (sources) {
    auto* link = sources;
    unlinkFromSubscriberList(link);
    unlinkFromSourceList(link);
    link->source = nullptr;
    link->observer = nullptr;
    retireLink(link);
  }
}

inline void Computation::deleteSpareLinks() {
  while (spareLinks) {
    auto* link = spareLinks;
    spareLinks = spareLinks->nextSource;
    link->nextSource = nullptr;
    freeLink(link);
  }
}

inline Link* Computation::findSource(Observable const* source) const {
  auto* link = sources;
  while (link) {
    if (link->source == source) {
      return link;
    }
    link = link->nextSource;
  }
  return nullptr;
}

inline Link* Computation::acquireLink() {
  if (!spareLinks) {
    return allocateLink();
  }
  auto* link = spareLinks;
  spareLinks = spareLinks->nextSource;
  *link = Link{};
  return link;
}

inline void Computation::retireLink(Link* link) {
  *link = Link{};
  link->nextSource = spareLinks;
  spareLinks = link;
}

inline void flushEffects();

struct BatchGuard {
  BatchGuard() {
    ++sBatchDepth;
  }

  ~BatchGuard() {
    --sBatchDepth;
    if (sBatchDepth == 0) {
      flushEffects();
    }
  }
};

inline void scheduleEffect(Computation* effect) {
  if (effect->scheduled || effect->disposed()) {
    return;
  }
  effect->scheduled = true;
  sEffectQueue.push_back(effect);
}

inline void flushEffects() {
  while (!sEffectQueue.empty()) {
    auto pending = std::move(sEffectQueue);
    sEffectQueue.clear();
    for (auto* effect : pending) {
      effect->scheduled = false;
      if (!effect->disposed() &&
          (hasFlag(effect->flags, Dirty) ||
           hasFlag(effect->flags, Pending))) {
        (void)effect->updateIfNeeded();
      }
    }
  }
}

template <typename T>
concept EqualityComparable = requires(T const& a, T const& b) {
  { a == b } -> std::convertible_to<bool>;
};

} // namespace detail

class Scope {
public:
  Scope()
      : state_(std::make_shared<detail::ScopeState>()) {}

  Scope(Scope const&) = delete;
  Scope& operator=(Scope const&) = delete;

  Scope(Scope&&) noexcept = default;
  Scope& operator=(Scope&&) noexcept = default;

  ~Scope() {
    dispose();
  }

  void dispose() {
    if (state_) {
      state_->dispose();
    }
  }

  bool disposed() const {
    return !state_ || state_->disposed;
  }

  void onCleanup(SmallFn<void()> cleanup) {
    state_->onCleanup(std::move(cleanup));
  }

  detail::ScopeState* state() const {
    return state_.get();
  }

private:
  std::shared_ptr<detail::ScopeState> state_;
};

template <typename Fn>
decltype(auto) withOwner(Scope& scope, Fn&& fn) {
  detail::OwnerContext context(scope.state());
  return std::forward<Fn>(fn)();
}

template <typename Fn>
void onCleanup(Fn&& fn) {
  assert(detail::sCurrentOwner && "onCleanup called without an active owner");
  detail::sCurrentOwner->onCleanup(SmallFn<void()>(std::forward<Fn>(fn)));
}

template <typename Fn>
decltype(auto) untrack(Fn&& fn) {
  detail::ObserverContext context(nullptr);
  return std::forward<Fn>(fn)();
}

template <typename T>
struct SignalState final : detail::Observable {
  explicit SignalState(T initial)
      : value(std::move(initial)) {
    detail::setFlag(flags, detail::Mutable);
  }

  void set(T next) {
    assert(!disposed() && "writing to a disposed Signal");
    if constexpr (detail::EqualityComparable<T>) {
      if (value == next) {
        return;
      }
    }
    detail::BatchGuard batch;
    value = std::move(next);
    ++version;
    propagateDirty();
  }

  T value;
};

template <typename T>
class Signal {
public:
  using Value = T;

  Signal() = default;

  explicit Signal(T initial)
      : state_(std::make_shared<SignalState<T>>(std::move(initial))) {
    detail::ownNode(state_);
  }

  T const& get() const {
    assert(state_ && "reading an empty Signal handle");
    assert(!state_->disposed() && "reading a disposed Signal");
    state_->reportRead();
    return state_->value;
  }

  T const& operator()() const {
    return get();
  }

  T const& peek() const {
    assert(state_ && "peeking an empty Signal handle");
    assert(!state_->disposed() && "peeking a disposed Signal");
    return state_->value;
  }

  void set(T next) const {
    assert(state_ && "writing an empty Signal handle");
    state_->set(std::move(next));
  }

  bool disposed() const {
    return !state_ || state_->disposed();
  }

private:
  std::shared_ptr<SignalState<T>> state_;
};

template <typename T>
struct ComputedState final : detail::Computation {
  template <typename Fn>
  explicit ComputedState(Fn&& compute)
      : fn(std::forward<Fn>(compute)) {
    recompute();
  }

  void run() override {
    (void)recompute();
  }

  bool updateIfNeeded() override {
    if (disposed()) {
      return false;
    }
    if (!value) {
      return recompute();
    }
    if (detail::hasFlag(flags, detail::Dirty)) {
      return recompute();
    }
    if (detail::hasFlag(flags, detail::Pending)) {
      if (!pollSourcesChanged()) {
        detail::clearFlag(flags, detail::Pending);
        return false;
      }
      return recompute();
    }
    return false;
  }

  void onDirty() override {
    propagatePending();
  }

  void onPending() override {
    propagatePending();
  }

  bool recompute() {
    assert(!disposed() && "recomputing a disposed Computed");
    clearSourcesForReuse();
    detail::ObserverContext context(this);
    auto next = fn();
    bool changed = !value.has_value();
    if (!changed) {
      if constexpr (detail::EqualityComparable<T>) {
        changed = !(*value == next);
      } else {
        changed = true;
      }
    }
    value = std::move(next);
    if (changed) {
      ++version;
    }
    detail::clearFlag(flags, detail::Pending);
    detail::clearFlag(flags, detail::Dirty);
    return changed;
  }

  SmallFn<T()> fn;
  std::optional<T> value;
};

template <typename T>
class Computed {
public:
  using Value = T;

  Computed() = default;

  template <typename Fn>
    requires(!std::is_same_v<std::decay_t<Fn>, Computed>)
  explicit Computed(Fn&& fn)
      : state_(std::make_shared<ComputedState<T>>(std::forward<Fn>(fn))) {
    detail::ownNode(state_);
  }

  T const& get() const {
    assert(state_ && "reading an empty Computed handle");
    assert(!state_->disposed() && "reading a disposed Computed");
    if (detail::hasFlag(state_->flags, detail::Dirty) ||
        detail::hasFlag(state_->flags, detail::Pending) || !state_->value) {
      (void)state_->updateIfNeeded();
    }
    state_->reportRead();
    return *state_->value;
  }

  T const& operator()() const {
    return get();
  }

  T const& peek() const {
    assert(state_ && "peeking an empty Computed handle");
    assert(!state_->disposed() && "peeking a disposed Computed");
    if (detail::hasFlag(state_->flags, detail::Dirty) ||
        detail::hasFlag(state_->flags, detail::Pending) || !state_->value) {
      (void)state_->updateIfNeeded();
    }
    return *state_->value;
  }

  bool disposed() const {
    return !state_ || state_->disposed();
  }

private:
  std::shared_ptr<ComputedState<T>> state_;
};

template <typename Fn>
Computed(Fn) -> Computed<std::invoke_result_t<Fn&>>;

struct EffectState final : detail::Computation {
  template <typename Fn>
  explicit EffectState(Fn&& body)
      : fn(std::forward<Fn>(body)) {}

  void run() override {
    if (disposed()) {
      return;
    }
    clearSourcesForReuse();
    detail::clearFlag(flags, detail::Pending);
    detail::clearFlag(flags, detail::Dirty);
    ::flux::detail::TransitionScopeSuspension transitionScope;
    detail::ObserverContext context(this);
    fn();
  }

  bool updateIfNeeded() override {
    if (disposed()) {
      return false;
    }
    if (detail::hasFlag(flags, detail::Dirty)) {
      run();
      return true;
    }
    if (detail::hasFlag(flags, detail::Pending)) {
      if (pollSourcesChanged()) {
        run();
        return true;
      }
      detail::clearFlag(flags, detail::Pending);
    }
    return false;
  }

  void onDirty() override {
    detail::scheduleEffect(this);
  }

  void onPending() override {
    detail::scheduleEffect(this);
  }

  SmallFn<void()> fn;
};

class Effect {
public:
  Effect() = default;

  template <typename Fn>
  explicit Effect(Fn&& fn)
      : state_(std::make_shared<EffectState>(std::forward<Fn>(fn))) {
    detail::ownNode(state_);
    state_->run();
  }

  void dispose() {
    if (state_) {
      state_->dispose();
    }
  }

  bool disposed() const {
    return !state_ || state_->disposed();
  }

private:
  std::shared_ptr<EffectState> state_;
};

template <typename Fn>
auto makeComputed(Fn&& fn) {
  using Value = std::invoke_result_t<Fn&>;
  return Computed<Value>(std::forward<Fn>(fn));
}

} // namespace flux::Reactive
