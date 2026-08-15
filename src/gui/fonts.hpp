#pragma once
#include <stdint.h>
#include <guiconfig/guiconfig.h>
#include <font_character_sets.hpp>

enum class Font : uint8_t {
    small = 0,
    normal,
    big,
    special,
#if not PRINTER_IS_PRUSA_MINI()
    large,
#endif
    largest_available =
#if not PRINTER_IS_PRUSA_MINI()
        large,
#else
        big,
#endif
};

struct font_t {
    uint8_t w;
    uint8_t h;
    FontCharacterSet charset;
    const void *pcs;
};

const font_t *resource_font(Font id);
#if PRINTER_IS_PRUSA_MINI() || BOARD_IS_XBUDDY()
bool load_external_font_glyph(const font_t *font, uint32_t glyph, uint8_t *destination, size_t size);
#endif

struct font_size_t {
    uint8_t w;
    uint8_t h;
    constexpr bool operator==(const font_size_t &rhs) const { return w == rhs.w && h == rhs.h; }
};

consteval font_size_t resource_font_size(Font id) {
    switch (id) {
#if PRINTER_IS_PRUSA_MINI()
    case Font::small:
        return { 7, 13 };
    case Font::normal:
    case Font::big:
        return { 11, 18 };
    case Font::special:
        return { 9, 16 };
#else
    case Font::small:
        return { 9, 16 };
    case Font::normal:
        return { 11, 19 };
    case Font::big:
        return { 13, 22 };
    case Font::special:
        return { 9, 16 };
    case Font::large:
        return { 30, 53 };
#endif
    default:
        return { 0, 0 };
    }
}

inline consteval auto width(Font font) { return resource_font_size(font).w; }
inline consteval auto height(Font font) { return resource_font_size(font).h; }
