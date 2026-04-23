#pragma once

/// \file Flux/UI/ActionRegistry.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Action.hpp>
#include <Flux/Core/ComponentKey.hpp>
#include <Flux/Core/Types.hpp>

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace flux {

struct ActionHandler {
  std::string name;
  std::function<void()> trigger;
  std::function<bool()> isEnabled; // empty = always enabled
};

class ActionRegistry {
public:
  /// Called at the start of each rebuild. Clears both tables.
  void beginRebuild();

  /// Registers a view-claim for the given component key.
  void registerViewClaim(ComponentKey const& key, std::string const& actionName,
                         std::function<void()> handler, std::function<bool()> isEnabled = {});

  /// Registers a window-action. Last call for a given name in build order wins.
  void registerWindowAction(std::string const& actionName, std::function<void()> handler,
                           std::function<bool()> isEnabled = {});

  /// Returns the view-claim handler for (focusedKey, actionName), or nullptr.
  ActionHandler const* findViewClaim(ComponentKey const& focusedKey, std::string const& actionName) const;

  /// Returns the window-action handler for actionName, or nullptr.
  ActionHandler const* findWindowAction(std::string const& actionName) const;

  /// Dispatch by shortcut: tries view-claim first, then window-action.
  /// Returns true if an enabled handler fired.
  bool dispatchShortcut(ComponentKey const& focusedKey, KeyCode key, Modifiers modifiers,
                        std::unordered_map<std::string, ActionDescriptor> const& descriptors) const;

  /// True if a handler exists for \p name and descriptor + handler enabled checks pass.
  bool isHandlerEnabled(ComponentKey const& focusedKey, std::string const& name,
                        std::unordered_map<std::string, ActionDescriptor> const& descriptors) const;

private:
  // View-claim table: componentKey → list of handlers (multiple actions per component).
  std::unordered_map<ComponentKey, std::vector<ActionHandler>, ComponentKeyHash> viewClaims_;

  // Registration order — last matching name wins (scan from end).
  std::vector<ActionHandler> windowActions_;
};

} // namespace flux
