#include <Flux/Core/ComponentKey.hpp>

#include "Debug/PerfCounters.hpp"

#include <algorithm>
#include <cassert>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace flux {

namespace {

using ComponentKeyHandle = std::uint32_t;

constexpr ComponentKeyHandle kRootHandle = 0;
constexpr std::uint64_t kHandleHashMultiplier = 0x9e3779b97f4a7c15ull;

struct InternedKeyNode {
  ComponentKeyHandle parent = kRootHandle;
  LocalId tail{};
  std::uint32_t depth = 0;
};

struct InternedEdge {
  ComponentKeyHandle parent = kRootHandle;
  LocalId tail{};

  bool operator==(InternedEdge const&) const = default;
};

struct InternedEdgeHash {
  std::size_t operator()(InternedEdge const& edge) const noexcept {
    std::size_t seed = static_cast<std::size_t>(edge.parent);
    std::size_t const tailHash = LocalIdHash{}(edge.tail);
    seed ^= tailHash + 0x9e3779b9u + (seed << 6) + (seed >> 2);
    return seed;
  }
};

std::size_t mixHandle(ComponentKeyHandle handle) noexcept {
  return static_cast<std::size_t>(static_cast<std::uint64_t>(handle) * kHandleHashMultiplier);
}

void recordMaterializedGrowth(std::size_t oldCapacity, std::size_t newCapacity) {
  if (newCapacity > oldCapacity) {
    debug::perf::recordComponentKeyHeapGrowth(newCapacity);
  }
}

class ComponentKeyTable {
public:
  ComponentKeyTable() {
    nodes_.push_back(InternedKeyNode{});
  }

  ComponentKeyHandle intern(ComponentKeyHandle parent, LocalId tail) {
    std::unique_lock lock(mutex_);
    return internLocked(parent, tail);
  }

  ComponentKeyHandle intern(LocalId const* values, std::size_t count) {
    if (count == 0 || !values) {
      return kRootHandle;
    }
    std::unique_lock lock(mutex_);
    ComponentKeyHandle handle = kRootHandle;
    for (std::size_t index = 0; index < count; ++index) {
      handle = internLocked(handle, values[index]);
    }
    return handle;
  }

  ComponentKeyHandle parent(ComponentKeyHandle handle) const noexcept {
    std::shared_lock lock(mutex_);
    return nodes_[handle].parent;
  }

  ComponentKeyHandle ancestorAtDepth(ComponentKeyHandle handle, std::uint32_t depth) const noexcept {
    std::shared_lock lock(mutex_);
    while (nodes_[handle].depth > depth) {
      handle = nodes_[handle].parent;
    }
    return handle;
  }

  bool hasPrefix(ComponentKeyHandle key, std::uint32_t keyDepth, ComponentKeyHandle prefix,
                 std::uint32_t prefixDepth) const noexcept {
    if (prefixDepth > keyDepth) {
      return false;
    }
    std::shared_lock lock(mutex_);
    while (nodes_[key].depth > prefixDepth) {
      key = nodes_[key].parent;
    }
    return key == prefix;
  }

  bool sharesPrefix(ComponentKeyHandle lhs, ComponentKeyHandle rhs) const noexcept {
    if (lhs == kRootHandle || rhs == kRootHandle) {
      return false;
    }
    std::shared_lock lock(mutex_);
    while (nodes_[lhs].depth > nodes_[rhs].depth) {
      lhs = nodes_[lhs].parent;
    }
    while (nodes_[rhs].depth > nodes_[lhs].depth) {
      rhs = nodes_[rhs].parent;
    }
    while (lhs != rhs) {
      if (lhs == kRootHandle || rhs == kRootHandle) {
        return false;
      }
      lhs = nodes_[lhs].parent;
      rhs = nodes_[rhs].parent;
    }
    return lhs != kRootHandle;
  }

  void materialize(ComponentKeyHandle handle, std::uint32_t depth, std::vector<LocalId>& out) const {
    std::size_t const oldCapacity = out.capacity();
    out.resize(depth);
    recordMaterializedGrowth(oldCapacity, out.capacity());
    if (depth == 0) {
      return;
    }

    std::shared_lock lock(mutex_);
    for (std::uint32_t index = depth; index > 0; --index) {
      InternedKeyNode const& node = nodes_[handle];
      out[index - 1U] = node.tail;
      handle = node.parent;
    }
  }

private:
  ComponentKeyHandle internLocked(ComponentKeyHandle parent, LocalId tail) {
    InternedEdge const edge{.parent = parent, .tail = tail};
    if (auto it = edges_.find(edge); it != edges_.end()) {
      return it->second;
    }

    assert(parent < nodes_.size());
    ComponentKeyHandle const handle = static_cast<ComponentKeyHandle>(nodes_.size());
    nodes_.push_back(InternedKeyNode{
        .parent = parent,
        .tail = tail,
        .depth = static_cast<std::uint32_t>(nodes_[parent].depth + 1U),
    });
    edges_.emplace(edge, handle);
    return handle;
  }

  mutable std::shared_mutex mutex_{};
  std::vector<InternedKeyNode> nodes_{};
  std::unordered_map<InternedEdge, ComponentKeyHandle, InternedEdgeHash> edges_{};
};

ComponentKeyTable& componentKeyTable() {
  static ComponentKeyTable table{};
  return table;
}

} // namespace

ComponentKey::ComponentKey(std::initializer_list<value_type> init) {
  debug::perf::recordComponentKeyCopy(init.size());
  assignFromValues(init.begin(), init.size());
}

ComponentKey::ComponentKey(std::vector<value_type> const& values) {
  debug::perf::recordComponentKeyCopy(values.size());
  assignFromValues(values.data(), values.size());
}

ComponentKey::ComponentKey(std::vector<value_type> const& prefix, value_type tail) {
  debug::perf::recordComponentKeyAppend(prefix.size() + 1U);
  handle_ = componentKeyTable().intern(prefix.data(), prefix.size());
  handle_ = componentKeyTable().intern(handle_, tail);
  size_ = static_cast<std::uint32_t>(prefix.size() + 1U);
}

ComponentKey::ComponentKey(ComponentKey const& other) {
  debug::perf::recordComponentKeyCopy(other.size());
  handle_ = other.handle_;
  size_ = other.size_;
}

ComponentKey::ComponentKey(ComponentKey const& prefix, value_type tail) {
  debug::perf::recordComponentKeyAppend(prefix.size() + 1U);
  handle_ = componentKeyTable().intern(prefix.handle_, tail);
  size_ = prefix.size_ + 1U;
}

ComponentKey::ComponentKey(ComponentKey&& other) noexcept
    : handle_(other.handle_)
    , size_(other.size_)
    , cacheValid_(other.cacheValid_)
    , materialized_(std::move(other.materialized_)) {
  other.clear();
}

ComponentKey& ComponentKey::operator=(ComponentKey const& other) {
  if (this != &other) {
    debug::perf::recordComponentKeyCopy(other.size());
    handle_ = other.handle_;
    size_ = other.size_;
    cacheValid_ = false;
    materialized_.clear();
  }
  return *this;
}

ComponentKey& ComponentKey::operator=(ComponentKey&& other) noexcept {
  if (this != &other) {
    handle_ = other.handle_;
    size_ = other.size_;
    cacheValid_ = other.cacheValid_;
    materialized_ = std::move(other.materialized_);
    other.clear();
  }
  return *this;
}

ComponentKey::~ComponentKey() = default;

ComponentKey::value_type const* ComponentKey::data() noexcept {
  ensureMaterialized();
  return materialized_.data();
}

ComponentKey::value_type const* ComponentKey::data() const noexcept {
  ensureMaterialized();
  return materialized_.data();
}

ComponentKey::value_type const& ComponentKey::operator[](std::size_t index) const noexcept {
  return data()[index];
}

ComponentKey::value_type const& ComponentKey::back() const noexcept {
  return data()[size_ - 1U];
}

void ComponentKey::clear() noexcept {
  handle_ = kRootHandle;
  size_ = 0;
  cacheValid_ = false;
  materialized_.clear();
}

void ComponentKey::push_back(value_type value) {
  debug::perf::recordComponentKeyAppend(size_ + 1U);
  handle_ = componentKeyTable().intern(handle_, value);
  ++size_;
  cacheValid_ = false;
  materialized_.clear();
}

void ComponentKey::pop_back() noexcept {
  if (size_ == 0) {
    return;
  }

  handle_ = componentKeyTable().parent(handle_);
  --size_;
  cacheValid_ = false;
  materialized_.clear();
  if (size_ == 0) {
    clear();
  }
}

void ComponentKey::reserve(std::size_t capacity) {
  ensureMaterialized();
  std::size_t const oldCapacity = materialized_.capacity();
  materialized_.reserve(capacity);
  recordMaterializedGrowth(oldCapacity, materialized_.capacity());
}

ComponentKey ComponentKey::prefix(std::size_t length) const {
  if (length >= size_) {
    return *this;
  }
  if (length == 0) {
    return {};
  }
  ComponentKeyHandle const prefixHandle =
      componentKeyTable().ancestorAtDepth(handle_, static_cast<std::uint32_t>(length));
  return fromHandle(prefixHandle, static_cast<std::uint32_t>(length));
}

bool ComponentKey::hasPrefix(ComponentKey const& prefix) const noexcept {
  if (prefix.empty()) {
    return true;
  }
  if (size_ < prefix.size_) {
    return false;
  }
  return componentKeyTable().hasPrefix(handle_, size_, prefix.handle_, prefix.size_);
}

bool ComponentKey::sharesPrefix(ComponentKey const& other) const noexcept {
  if (empty() || other.empty()) {
    return false;
  }
  return componentKeyTable().sharesPrefix(handle_, other.handle_);
}

bool operator==(ComponentKey const& lhs, ComponentKey const& rhs) noexcept {
  std::size_t const comparedIds =
      lhs.size_ == rhs.size_ ? lhs.size_ : std::min(lhs.size_, rhs.size_);
  debug::perf::recordComponentKeyEquality(comparedIds);
  if (lhs.size_ != rhs.size_) {
    return false;
  }
  return lhs.handle_ == rhs.handle_;
}

void ComponentKey::assignFromValues(value_type const* values, std::size_t count) {
  size_ = static_cast<std::uint32_t>(count);
  handle_ = componentKeyTable().intern(values, count);
  cacheValid_ = false;
  materialized_.clear();
}

void ComponentKey::ensureMaterialized() const {
  if (cacheValid_) {
    return;
  }
  componentKeyTable().materialize(handle_, size_, materialized_);
  cacheValid_ = true;
}

ComponentKey ComponentKey::fromHandle(std::uint32_t handle, std::uint32_t size) noexcept {
  ComponentKey key;
  key.handle_ = handle;
  key.size_ = size;
  key.cacheValid_ = false;
  return key;
}

std::size_t ComponentKeyHash::operator()(ComponentKey const& k) const noexcept {
  debug::perf::recordComponentKeyHash(k.size());
  return mixHandle(k.handle_);
}

} // namespace flux
