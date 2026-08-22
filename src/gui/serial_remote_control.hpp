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
    uint32_t screen_by_state;
    uint32_t chamber_by_state;
    uint32_t status_by_state;
    int16_t current_screen;
    int16_t current_chamber;
    int8_t current_state;
    uint16_t activity_timeout_s;
    uint16_t event_timeout_s;
    uint16_t off_timeout_s;
    uint16_t status_finished_hold_s;
    bool screen_supported;
    bool chamber_supported;
    bool status_supported;
    bool door_holds_active;
    bool post_print_hold_enabled;
    bool active_hold;
};

enum class LightHoldResult : uint8_t { unchanged,
    changed,
    printer_busy,
    unsupported };
LightHoldResult set_light_hold(bool active);

enum class TransferKind : uint8_t { none,
    file,
    firmware };
struct TransferStatus {
    TransferKind kind;
    uint8_t progress;
};
void set_transfer(TransferKind kind, uint32_t completed = 0, uint32_t total = 0);
TransferStatus transfer_status();

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
    configuration = 1 << 4,
    all = 0x1f,
};

void open_session(uint8_t subscriptions, bool legacy_notifications);
void keepalive_session();
void close_session();
bool session_active();
bool subscribed(EventSubscription subscription);
bool legacy_notifications_enabled();
const char *printer_state_name();
uint32_t next_event_sequence();
uint32_t next_configuration_revision();

/// Called only by the GUI task.
void process_gui();

} // namespace serial_remote_control
