#pragma once

#include <cstdint>

namespace serial_remote_control {

enum class Action : uint8_t {
    encoder,
    click,
    back,
    home,
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
void set_persistent_lights(uint32_t screen, uint32_t chamber, uint32_t status);

enum class EventSubscription : uint8_t {
    none = 0,
    progress = 1 << 0,
    error = 1 << 1,
    workflow = 1 << 2,
    notification = 1 << 3,
    all = 0x0f,
};

void open_session(uint8_t subscriptions, bool legacy_notifications);
void close_session();
bool session_active();
bool subscribed(EventSubscription subscription);
bool legacy_notifications_enabled();
uint32_t next_event_sequence();

/// Called only by the GUI task.
void process_gui();

} // namespace serial_remote_control
