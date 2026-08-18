#include "gui_time.hpp"
#include "timing.h"

static uint32_t current_tick = 0;

void gui::TickLoop() {
    current_tick = ticks_ms();
}

void gui::StartLoop() {
    gui::TickLoop();
}

void gui::EndLoop() {
}

uint32_t gui::GetTick() {
    return current_tick;
}
