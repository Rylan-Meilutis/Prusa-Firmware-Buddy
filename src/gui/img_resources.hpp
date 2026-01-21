#pragma once
#include "guitypes.hpp" //img::Resource

namespace img {

#include "qoi_resources.gen"

#ifndef UNITTESTS
inline constexpr std::array spinner_16x16_stages { &img::spinner0_16x16, &img::spinner1_16x16, &img::spinner2_16x16, &img::spinner3_16x16 };
#endif

/// @returns different spinnerX_16x16 based on current time
const Resource *spinner_16x16_animated();

} // namespace img
