/// @file
#pragma once

#include <cstdint>
#include <utils/byte_utils.hpp>

/// Modern API to crc32_calc_ex()
uint32_t crc32(uint32_t crc, Bytes data);
