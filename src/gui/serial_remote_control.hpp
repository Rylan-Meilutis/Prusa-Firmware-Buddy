#pragma once

#include <cstdint>
#include <array>

namespace serial_remote_control {

enum class Action : uint8_t {
    encoder,
    click,
    back,
    home,
};

struct Status {
    bool enabled;
    uint8_t pending;
    uint32_t accepted_sequence;
    uint32_t applied_sequence;
};

struct LightStatus {
    int16_t print_screen;
    int16_t print_chamber;
    int16_t print_status;
};

void set_enabled(bool enabled);
bool enqueue(Action action, int16_t value = 0);
void request_refresh();
void reload_theme();
LightStatus light_status();
void set_temporary_lights(int16_t screen, int16_t chamber, int16_t status);
void set_persistent_lights(const std::array<int16_t, 4> &screen,
    const std::array<int16_t, 4> &chamber, const std::array<int16_t, 4> &status);
Status status();

/// Called only by the GUI task.
void process_gui();

} // namespace serial_remote_control
