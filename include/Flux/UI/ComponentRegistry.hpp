#pragma once

#include <Flux/UI/ComponentKey.hpp>

#include <memory>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>

namespace flux {

/// Heap-allocated type-erased component instance.
struct ComponentEntry {
  std::unique_ptr<void, void (*)(void*)> instance{nullptr, nullptr};
  std::type_index type{typeid(void)};
};

/// Stores one heap-allocated instance per composite component key.
/// Owned by Runtime. A single registry persists for the window lifetime.
class ComponentRegistry {
public:
  /// Call before each build pass. Resets the visited-key tracking.
  void beginRebuild();

  /// Call after each build pass. Destroys entries not visited this pass.
  void endRebuild();

  /// Get or create a component instance at the given key.
  ///
  /// If an existing entry matches key + type:
  ///   - Calls `instance->updateProps(description)` if the method exists.
  ///   - Returns a pointer to the existing instance (signals intact).
  ///
  /// Otherwise:
  ///   - Move-constructs a new C from `description`.
  ///   - Stores it in the registry under `key`.
  ///   - Returns a pointer to the new instance.
  ///
  /// The returned pointer is stable until `endRebuild()` removes the entry.
  template<typename C>
  C* getOrCreate(ComponentKey const& key, C&& description);

  /// Default-constructs `C` on first use, then `updateProps(desc)` when `Desc` is supplied.
  /// For reuse, calls `existing->updateProps(desc)` when that method exists.
  template<typename C, typename Desc>
  C* getOrCreate(ComponentKey const& key, Desc const& desc);

private:
  std::unordered_map<ComponentKey, ComponentEntry, ComponentKeyHash> entries_;
  std::unordered_set<ComponentKey, ComponentKeyHash> visited_;
};

} // namespace flux

// --- implementation (header-only template) ---

namespace flux {

template<typename C>
C* ComponentRegistry::getOrCreate(ComponentKey const& key, C&& description) {
  visited_.insert(key);

  auto it = entries_.find(key);
  if (it != entries_.end() && it->second.type == std::type_index(typeid(C))) {
    C* existing = static_cast<C*>(it->second.instance.get());
    if constexpr (requires { existing->updateProps(description); }) {
      existing->updateProps(description);
    }
    return existing;
  }

  C* raw = new C(std::move(description));
  entries_[key] = ComponentEntry{
      std::unique_ptr<void, void (*)(void*)>(raw, [](void* p) {
        delete static_cast<C*>(p);
      }),
      std::type_index(typeid(C))};
  return raw;
}

inline void ComponentRegistry::beginRebuild() { visited_.clear(); }

inline void ComponentRegistry::endRebuild() {
  for (auto it = entries_.begin(); it != entries_.end();) {
    if (visited_.find(it->first) == visited_.end()) {
      it = entries_.erase(it);
    } else {
      ++it;
    }
  }
}

template<typename C, typename Desc>
C* ComponentRegistry::getOrCreate(ComponentKey const& key, Desc const& desc) {
  visited_.insert(key);

  auto it = entries_.find(key);
  if (it != entries_.end() && it->second.type == std::type_index(typeid(C))) {
    C* existing = static_cast<C*>(it->second.instance.get());
    if constexpr (requires { existing->updateProps(desc); }) {
      existing->updateProps(desc);
    }
    return existing;
  }

  C* raw = new C();
  if constexpr (requires { raw->updateProps(desc); }) {
    raw->updateProps(desc);
  }
  entries_[key] = ComponentEntry{
      std::unique_ptr<void, void (*)(void*)>(raw, [](void* p) {
        delete static_cast<C*>(p);
      }),
      std::type_index(typeid(C))};
  return raw;
}

} // namespace flux
