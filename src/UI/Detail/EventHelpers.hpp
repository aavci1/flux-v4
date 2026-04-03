#pragma once

/// \file UI/Detail/EventHelpers.hpp
///
/// Internal helpers shared between Element.cpp (renderFromLayout) and RenderLayoutTree.cpp.

#include <Flux/Core/Cursor.hpp>
#include <Flux/UI/ComponentKey.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/EventMap.hpp>

namespace flux::detail {

inline EventHandlers eventHandlersFromModifiers(ElementModifiers const& m, ComponentKey stableKey) {
  bool const effFocusable =
      m.focusable || static_cast<bool>(m.onKeyDown) || static_cast<bool>(m.onKeyUp) ||
      static_cast<bool>(m.onTextInput);
  return EventHandlers{
      .stableTargetKey = stableKey,
      .onTap = m.onTap,
      .onPointerDown = m.onPointerDown,
      .onPointerUp = m.onPointerUp,
      .onPointerMove = m.onPointerMove,
      .onScroll = m.onScroll,
      .onKeyDown = m.onKeyDown,
      .onKeyUp = m.onKeyUp,
      .onTextInput = m.onTextInput,
      .focusable = effFocusable,
      .cursor = m.cursor,
  };
}

inline bool shouldInsertHandlers(EventHandlers const& h) {
  return static_cast<bool>(h.onTap) || static_cast<bool>(h.onPointerDown) ||
         static_cast<bool>(h.onPointerUp) || static_cast<bool>(h.onPointerMove) ||
         static_cast<bool>(h.onScroll) || static_cast<bool>(h.onKeyDown) ||
         static_cast<bool>(h.onKeyUp) || static_cast<bool>(h.onTextInput) || h.focusable ||
         h.cursor != Cursor::Inherit;
}

} // namespace flux::detail
