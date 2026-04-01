#pragma once

/// \file Flux/Core/Cursor.hpp
///
/// Part of the Flux public API.


#include <cstdint>

namespace flux {

/// Mouse pointer shape. Values map to standard platform cursors.
enum class Cursor : std::uint8_t {
    Inherit,       ///< Unset — does not constrain the cursor; the hit behind wins (see Runtime cursor hit-test).
    Arrow,         ///< Platform arrow pointer
    IBeam,         ///< Text insertion cursor (I-beam)
    Hand,          ///< Pointing hand — clickable items, links
    ResizeEW,      ///< Horizontal resize (↔) — vertical dividers
    ResizeNS,      ///< Vertical resize (↕) — horizontal dividers
    ResizeNESW,    ///< Diagonal resize (⤢) — bottom-left / top-right corners
    ResizeNWSE,    ///< Diagonal resize (⤡) — top-left / bottom-right corners
    ResizeAll,     ///< Move / pan (⊹) — free drag
    Crosshair,     ///< Precision crosshair — drawing tools, colour pickers
    NotAllowed,    ///< Prohibited (🚫) — disabled actions
};

} // namespace flux
