#pragma once

/// \file Flux/Core/Action.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Shortcut.hpp>

#include <functional>
#include <string>

namespace flux {

struct ActionDescriptor {
  /// Human-readable label, e.g. "Copy", "Save File". Used by future menu/toolbar integration.
  std::string label;

  /// Keyboard shortcut that triggers this action. Optional — an action can exist
  /// without a shortcut (triggered only programmatically or from menus).
  Shortcut shortcut{};

  /// When set, called every rebuild to determine if the action is currently available.
  /// Return false to suppress both shortcut dispatch and visual enabled state.
  /// When not set the action is always considered enabled.
  std::function<bool()> isEnabled;
};

} // namespace flux
