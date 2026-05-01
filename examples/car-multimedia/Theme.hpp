#pragma once

#include "Common.hpp"
#include "Constants.hpp"

namespace car {

inline Theme makeCarTheme() {
    Theme t = Theme::dark().withDensity(1.15f);
    t.windowBackgroundColor = Color::hex(0x0A0C11);
    t.controlBackgroundColor = Color::hex(0x161A22);
    t.elevatedBackgroundColor = Color::hex(0x0E1015);
    t.textBackgroundColor = Color::hex(0x161A22);
    t.hoveredControlBackgroundColor = Color::hex(0x1E2330);
    t.rowHoverBackgroundColor = Color::hex(0x232838);
    t.separatorColor = Color::hex(0x252A36);
    t.opaqueSeparatorColor = Color::hex(0x353B4A);
    t.labelColor = Color::hex(0xF0F2F7);
    t.secondaryLabelColor = Color::hex(0x9BA1B0);
    t.tertiaryLabelColor = Color::hex(0x6B7180);
    t.quaternaryLabelColor = Color::hex(0x4A5060);
    t.accentColor = Color::hex(kSignatureBlue);
    t.selectedContentBackgroundColor = Color{0.04f, 0.52f, 1.f, 0.16f};
    return t;
}

} // namespace car
