#pragma once

#include <display.hpp>

namespace filament_color_gui {

inline constexpr Rect16::Width_t swatch_extension_width { 32 };
inline constexpr Rect16::Width_t swatch_and_arrow_extension_width { 48 };
inline constexpr Rect16::Width_t swatch_size { 20 };

inline void draw_swatch(Rect16 rect, const Color color, const Color background) {
    rect = Rect16::Left_t { static_cast<int16_t>(rect.Left() + (rect.Width() - swatch_size) / 2) };
    rect = Rect16::Top_t { static_cast<int16_t>(rect.Top() + (rect.Height() - swatch_size) / 2) };
    rect = swatch_size;
    rect = Rect16::Height_t { swatch_size };

    const bool light_color = color.to_grayscale() > 127;
    const bool light_background = background.to_grayscale() > 127;
    display::draw_rect(rect, light_color == light_background ? (light_color ? COLOR_BLACK : COLOR_WHITE) : background);
    rect += Rect16::Left_t { 1 };
    rect += Rect16::Top_t { 1 };
    rect -= Rect16::Width_t { 2 };
    rect -= Rect16::Height_t { 2 };
    display::fill_rect(rect, color);
}

} // namespace filament_color_gui
