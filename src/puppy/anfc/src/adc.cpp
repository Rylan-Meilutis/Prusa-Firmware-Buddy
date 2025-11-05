#include "adc.hpp"

namespace adc {
alignas(uint32_t) std::array<Raw, 1> impl::buffer;
} // namespace adc
