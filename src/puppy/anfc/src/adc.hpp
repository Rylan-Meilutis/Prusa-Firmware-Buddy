#pragma once

#include <array>

namespace adc {

using Raw = uint16_t;

namespace impl {
    alignas(uint32_t) extern std::array<Raw, 1> buffer;
} // namespace impl

} // namespace adc
