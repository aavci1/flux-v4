#include <Flux/UI/ActionRegistry.hpp>

namespace flux {

void ActionRegistry::beginRebuild() {
  viewClaims_.clear();
  windowActions_.clear();
}

void ActionRegistry::registerViewClaim(ComponentKey const& key, std::string const& actionName,
                                      std::function<void()> handler, std::function<bool()> isEnabled) {
  ActionHandler h;
  h.name = actionName;
  h.trigger = std::move(handler);
  h.isEnabled = std::move(isEnabled);
  viewClaims_[key].push_back(std::move(h));
}

void ActionRegistry::registerWindowAction(std::string const& actionName, std::function<void()> handler,
                                         std::function<bool()> isEnabled) {
  ActionHandler h;
  h.name = actionName;
  h.trigger = std::move(handler);
  h.isEnabled = std::move(isEnabled);
  windowActions_.push_back(std::move(h));
}

ActionHandler const* ActionRegistry::findViewClaim(ComponentKey const& focusedKey,
                                                   std::string const& actionName) const {
  auto cit = viewClaims_.find(focusedKey);
  if (cit == viewClaims_.end()) {
    return nullptr;
  }
  for (ActionHandler const& h : cit->second) {
    if (h.name == actionName) {
      return &h;
    }
  }
  return nullptr;
}

ActionHandler const* ActionRegistry::findWindowAction(std::string const& actionName) const {
  for (auto it = windowActions_.rbegin(); it != windowActions_.rend(); ++it) {
    if (it->name == actionName) {
      return &*it;
    }
  }
  return nullptr;
}

bool ActionRegistry::dispatchShortcut(ComponentKey const& focusedKey, KeyCode key, Modifiers modifiers,
                                    std::unordered_map<std::string, ActionDescriptor> const& descriptors) const {
  // Step 1: view-claims from focused component (first matching shortcut in list order).
  if (!focusedKey.empty()) {
    auto cit = viewClaims_.find(focusedKey);
    if (cit != viewClaims_.end()) {
      for (ActionHandler const& claim : cit->second) {
        auto dit = descriptors.find(claim.name);
        if (dit == descriptors.end()) {
          continue;
        }
        if (!dit->second.shortcut.matches(key, modifiers)) {
          continue;
        }
        if (dit->second.isEnabled && !dit->second.isEnabled()) {
          continue;
        }
        if (claim.isEnabled && !claim.isEnabled()) {
          continue;
        }
        claim.trigger();
        return true;
      }
    }
  }

  // Step 2: window-actions — last registration wins (scan from end).
  for (auto it = windowActions_.rbegin(); it != windowActions_.rend(); ++it) {
    ActionHandler const& handler = *it;
    auto dit = descriptors.find(handler.name);
    if (dit == descriptors.end()) {
      continue;
    }
    if (!dit->second.shortcut.matches(key, modifiers)) {
      continue;
    }
    if (dit->second.isEnabled && !dit->second.isEnabled()) {
      continue;
    }
    if (handler.isEnabled && !handler.isEnabled()) {
      continue;
    }
    handler.trigger();
    return true;
  }

  return false;
}

bool ActionRegistry::isHandlerEnabled(ComponentKey const& focusedKey, std::string const& name,
                                    std::unordered_map<std::string, ActionDescriptor> const& descriptors) const {
  auto dit = descriptors.find(name);
  if (dit == descriptors.end()) {
    return false;
  }
  if (dit->second.isEnabled && !dit->second.isEnabled()) {
    return false;
  }

  if (ActionHandler const* claim = findViewClaim(focusedKey, name)) {
    if (claim->isEnabled && !claim->isEnabled()) {
      return false;
    }
    return true;
  }

  if (ActionHandler const* win = findWindowAction(name)) {
    if (win->isEnabled && !win->isEnabled()) {
      return false;
    }
    return true;
  }

  return false;
}

} // namespace flux
