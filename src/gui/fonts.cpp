/**
 * @file fonts.cpp
 */

#include "fonts.hpp"
#include "config.h"
#include <bsod/bsod.h>
#include <guiconfig/guiconfig.h>
#include <printers.h>
#include <option/enable_translation_ja.h>
#include <option/enable_translation_uk.h>
#include <array>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#if PRINTER_IS_PRUSA_MINI()
    #if ENABLE_TRANSLATION_JA()
constexpr FontCharacterSet mini_charset = FontCharacterSet::latin_and_katakana;
    #elif ENABLE_TRANSLATION_UK()
constexpr FontCharacterSet mini_charset = FontCharacterSet::latin_and_cyrillic;
    #else
constexpr FontCharacterSet mini_charset = FontCharacterSet::latin;
    #endif
constexpr font_t font_regular_7x13 { 7, 13, mini_charset, nullptr };
constexpr font_t font_regular_11x18 { 11, 18, mini_charset, nullptr };
constexpr font_t font_regular_9x16 { 9, 16, mini_charset, nullptr };
#else
    #include "res/cc/font_regular_9x16_full.hpp" //Font::small
    #include "res/cc/font_bold_11x19_full.hpp" //Font::normal
    #include "res/cc/font_bold_13x22_full.hpp" //Font::big
    #include "res/cc/font_bold_30x53_digits.hpp" //Font::large
#endif

#if PRINTER_IS_PRUSA_MINI()
bool load_external_font_glyph(const font_t *font, const uint32_t glyph, uint8_t *destination, const size_t size) {
    struct ExternalFont {
        uint8_t width;
        uint8_t height;
        const char *path;
        int descriptor;
        uint32_t cached_glyph;
        std::array<uint8_t, (11 * 18 + 1) / 2> cache;
    };
    static ExternalFont fonts[] {
        { 7, 13, "/internal/res/fonts/7x13.bin", -1, UINT32_MAX, {} },
        { 9, 16, "/internal/res/fonts/9x16.bin", -1, UINT32_MAX, {} },
        { 11, 18, "/internal/res/fonts/11x18.bin", -1, UINT32_MAX, {} },
    };
    for (auto &external : fonts) {
        if (font->w != external.width || font->h != external.height) continue;
        if (external.cached_glyph == glyph) {
            memcpy(destination, external.cache.data(), size);
            return true;
        }
        if (external.descriptor < 0) external.descriptor = open(external.path, O_RDONLY);
        if (external.descriptor < 0) return false;
        const off_t offset = static_cast<off_t>(glyph * size);
        if (lseek(external.descriptor, offset, SEEK_SET) != offset
            || read(external.descriptor, external.cache.data(), size) != static_cast<ssize_t>(size)) return false;
        external.cached_glyph = glyph;
        memcpy(destination, external.cache.data(), size);
        return true;
    }
    return false;
}
#endif

#if PRINTER_IS_PRUSA_MINI()
static_assert(resource_font_size(Font::small) == font_size_t { font_regular_7x13.w, font_regular_7x13.h }, "Font size doesn't match");
static_assert(resource_font_size(Font::normal) == font_size_t { font_regular_11x18.w, font_regular_11x18.h }, "Font size doesn't match");
static_assert(resource_font_size(Font::big) == font_size_t { font_regular_11x18.w, font_regular_11x18.h }, "Font size doesn't match");
static_assert(resource_font_size(Font::special) == font_size_t { font_regular_9x16.w, font_regular_9x16.h }, "Font size doesn't match");
#else
static_assert(resource_font_size(Font::small) == font_size_t { font_regular_9x16.w, font_regular_9x16.h }, "Font size doesn't match");
static_assert(resource_font_size(Font::normal) == font_size_t { font_bold_11x19.w, font_bold_11x19.h }, "Font size doesn't match");
static_assert(resource_font_size(Font::big) == font_size_t { font_bold_13x22.w, font_bold_13x22.h }, "Font size doesn't match");
static_assert(resource_font_size(Font::special) == font_size_t { font_regular_9x16.w, font_regular_9x16.h }, "Font size doesn't match");
static_assert(resource_font_size(Font::large) == font_size_t { font_bold_30x53.w, font_bold_30x53.h }, "Font size doesn't match");
#endif

const font_t *resource_font(Font font) {
    switch (font) {
#if PRINTER_IS_PRUSA_MINI()
    case Font::small:
        return &font_regular_7x13;
    case Font::normal:
        return &font_regular_11x18;
    case Font::big:
        return &font_regular_11x18;
    case Font::special:
        return &font_regular_9x16;
#else
    case Font::small:
        return &font_regular_9x16;
    case Font::normal:
        return &font_bold_11x19;
    case Font::big:
        return &font_bold_13x22;
    case Font::special:
        return &font_regular_9x16;
    case Font::large:
        return &font_bold_30x53;
#endif
    }
    bsod_unreachable();
}
