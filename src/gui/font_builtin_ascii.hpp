#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <font_character_sets.hpp>

namespace buddy::builtin_font {
namespace detail {
    // Generated headers also declare a descriptor; only their constant bitmap
    // initializers are used here. Keep the full character tables out of the ELF:
    // the bounded prefixes below are evaluated at compile time.
    struct font_t {
        uint8_t w, h;
        FontCharacterSet charset;
        const void *pcs;
    };
#include "res/cc/font_regular_9x16_full.hpp"
#include "res/cc/font_bold_11x19_full.hpp"
#include "res/cc/font_bold_13x22_full.hpp"
#include "res/cc/font_bold_30x53_digits.hpp"

    template <size_t Bytes, size_t N>
    consteval auto prefix(const uint8_t (&source)[N]) {
        static_assert(Bytes <= N);
        std::array<uint8_t, Bytes> result {};
        for (size_t i = 0; i < Bytes; ++i) {
            result[i] = source[i];
        }
        return result;
    }

    inline constexpr auto small = prefix<95 * 72>(font_regular_9x16_data);
    inline constexpr auto normal = prefix<95 * 105>(font_bold_11x19_data);
    inline constexpr auto big = prefix<95 * 143>(font_bold_13x22_data);
    inline constexpr auto digits = prefix<16 * 795>(font_bold_30x53_data);
} // namespace detail

// These bytes are identical to the managed resource glyphs. Common status
// text and editable numbers must not depend on filesystem availability during
// printing. Extended language glyphs continue to use the external resources.
inline bool load(uint8_t width, uint8_t height, uint32_t glyph, uint8_t *destination, size_t size) {
    if (!destination || size != (size_t(width) * height + 1) / 2) {
        return false;
    }
    const uint8_t *data = nullptr;
    size_t count = 0;
    if (width == 9 && height == 16) {
        data = detail::small.data();
        count = 95;
    } else if (width == 11 && height == 19) {
        data = detail::normal.data();
        count = 95;
    } else if (width == 13 && height == 22) {
        data = detail::big.data();
        count = 95;
    } else if (width == 30 && height == 53) {
        data = detail::digits.data();
        count = 16;
    }
    if (glyph >= count) {
        return false;
    }
    memcpy(destination, data + glyph * size, size);
    return true;
}
} // namespace buddy::builtin_font
