#pragma once
/**
 * @file gui_time.hpp
 * @author Radek Vana
 * @brief wrapped access to tick functions
 * @date 2021-04-21
 */

#include <cstdint>

namespace gui {
void TickLoop(); // call this function in loop
uint32_t GetTick(); // current loop tick value, every call in current loop returns same value
}; // namespace gui
