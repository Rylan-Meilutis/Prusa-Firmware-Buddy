#include "serial_remote_control.hpp"

#include "ScreenHandler.hpp"
#include <knob_event.hpp>
#include <printer_lock.hpp>
#include <config_store/store_instance.hpp>
#include <leds/light_state.hpp>
#include <option/has_leds.h>
#include <option/has_side_leds.h>
#include <timing.h>
#if HAS_LEDS()
    #include <leds/led_manager.hpp>
    #include <leds/status_leds_handler.hpp>
#endif
#if HAS_SIDE_LEDS()
    #include <leds/side_strip_handler.hpp>
#endif

#include <array>
#include <algorithm>
#include <atomic>

namespace serial_remote_control {

namespace {
std::atomic<TransferKind> remote_transfer_kind { TransferKind::none };
std::atomic<uint8_t> remote_transfer_progress { 0 };
std::atomic<uint32_t> remote_transfer_expires_ms { 0 };
}

void set_transfer(const TransferKind kind, const uint32_t completed, const uint32_t total) {
    remote_transfer_progress.store(total ? static_cast<uint8_t>(std::min<uint32_t>(100, completed * 100ULL / total)) : 0, std::memory_order_release);
    remote_transfer_expires_ms.store(kind != TransferKind::none && total == 0 ? ticks_ms() + 1000 : 0, std::memory_order_release);
    remote_transfer_kind.store(kind, std::memory_order_release);
}

TransferStatus transfer_status() {
    const uint32_t expires = remote_transfer_expires_ms.load(std::memory_order_acquire);
    if (expires && ticks_diff(ticks_ms(), expires) >= 0) {
        set_transfer(TransferKind::none);
    }
    return { remote_transfer_kind.load(std::memory_order_acquire), remote_transfer_progress.load(std::memory_order_acquire) };
}
namespace {

struct Command {
    Action action;
    int16_t value;
};

constexpr uint8_t queue_size = 8;
std::array<Command, queue_size> commands {};
std::atomic<uint8_t> read_index { 0 };
std::atomic<uint8_t> write_index { 0 };
std::atomic<bool> control_enabled { false };
std::atomic<bool> refresh_requested { false };
std::atomic<bool> protocol_session_active { false };
std::atomic<uint8_t> protocol_subscriptions { 0 };
std::atomic<bool> protocol_legacy_notifications { true };
std::atomic<uint32_t> protocol_event_sequence { 0 };
std::atomic<uint32_t> protocol_session_activity_ms { 0 };
constexpr uint32_t protocol_session_timeout_ms = 30'000;

uint8_t advance(uint8_t index) {
    return static_cast<uint8_t>((index + 1) % queue_size);
}

void clear_queue() {
    const auto write = write_index.load(std::memory_order_acquire);
    read_index.store(write, std::memory_order_release);
}

} // namespace

void set_enabled(const bool enabled) {
    control_enabled.store(enabled, std::memory_order_release);
    if (!enabled) {
        clear_queue();
    }
}

bool enqueue(const Action action, const int16_t value) {
    if (!control_enabled.load(std::memory_order_acquire) || printer_lock::locked()) {
        return false;
    }

    const auto write = write_index.load(std::memory_order_relaxed);
    const auto next = advance(write);
    if (next == read_index.load(std::memory_order_acquire)) {
        return false;
    }

    commands[write] = Command { action, value };
    write_index.store(next, std::memory_order_release);
    return true;
}

void request_refresh() {
    refresh_requested.store(true, std::memory_order_release);
}

void reload_theme() {
#if HAS_LEDS()
    leds::StatusLedsHandler::instance().reload_colors();
#endif
    request_refresh();
}

LightStatus light_status() {
    LightStatus result { -1, -1, -1 };
#if HAS_SIDE_LEDS()
    auto &side = leds::SideStripHandler::instance();
    result.print_screen = side.get_print_screen_brightness();
    result.print_chamber = static_cast<uint16_t>(side.get_print_light_brightness()) * 100 / 255;
#elif HAS_LEDS()
    result.print_screen = leds::LEDManager::instance().get_print_screen_brightness();
#endif
#if HAS_LEDS()
    result.print_status = leds::StatusLedsHandler::instance().get_print_status_brightness();
#endif
    return result;
}

void set_temporary_lights(const int16_t screen, const int16_t chamber, const int16_t status) {
    if (screen >= 0 && screen <= 100) {
#if HAS_SIDE_LEDS()
        leds::SideStripHandler::instance().set_print_screen_brightness(screen);
#elif HAS_LEDS()
        leds::LEDManager::instance().set_print_screen_brightness(screen);
#endif
    }
#if HAS_SIDE_LEDS()
    if (chamber >= 0 && chamber <= 100)
        leds::SideStripHandler::instance().set_print_light_brightness(chamber == 100 ? 255 : chamber * 255 / 100);
#endif
#if HAS_LEDS()
    if (status >= 0 && status <= 100)
        leds::StatusLedsHandler::instance().set_print_status_brightness(status);
#endif
}

void set_persistent_lights(const uint32_t screen, const uint32_t chamber, const uint32_t status) {
    constexpr std::array<leds::LightState, 4> states {
        leds::LightState::deep_idle, leds::LightState::idle,
        leds::LightState::active, leds::LightState::printing,
    };
    for (size_t i = 0; i < states.size(); ++i) {
        const uint8_t shift = leds::light_state_shift(states[i]);
        const uint8_t chamber_value = chamber >> shift;
        const uint8_t status_value = status >> shift;
#if HAS_SIDE_LEDS()
        if (chamber_value <= 100)
            leds::SideStripHandler::instance().set_brightness(states[i], chamber_value == 100 ? 255 : chamber_value * 255 / 100);
#endif
#if HAS_LEDS()
        if (status_value <= 100)
            leds::StatusLedsHandler::instance().set_brightness(states[i], status_value);
#endif
    }
    config_store().screen_brightness_by_state.set(screen);
}

void open_session(const uint8_t subscriptions, const bool legacy_notifications) {
    protocol_subscriptions.store(subscriptions, std::memory_order_release);
    protocol_legacy_notifications.store(legacy_notifications, std::memory_order_release);
    protocol_event_sequence.store(0, std::memory_order_release);
    protocol_session_activity_ms.store(ticks_ms(), std::memory_order_release);
    protocol_session_active.store(true, std::memory_order_release);
}

void keepalive_session() {
    if (session_active()) {
        protocol_session_activity_ms.store(ticks_ms(), std::memory_order_release);
    }
}

void close_session() {
    protocol_session_active.store(false, std::memory_order_release);
    protocol_subscriptions.store(0, std::memory_order_release);
    protocol_legacy_notifications.store(true, std::memory_order_release);
    set_enabled(false);
}

bool session_active() {
    if (!protocol_session_active.load(std::memory_order_acquire)) {
        return false;
    }
    const auto last_activity = protocol_session_activity_ms.load(std::memory_order_acquire);
    if (ticks_diff(ticks_ms(), last_activity) <= static_cast<int32_t>(protocol_session_timeout_ms)) {
        return true;
    }
    close_session();
    return false;
}

bool subscribed(const EventSubscription subscription) {
    return session_active() && (protocol_subscriptions.load(std::memory_order_acquire) & static_cast<uint8_t>(subscription));
}

bool legacy_notifications_enabled() {
    return !session_active() || protocol_legacy_notifications.load(std::memory_order_acquire);
}

uint32_t next_event_sequence() {
    return protocol_event_sequence.fetch_add(1, std::memory_order_acq_rel) + 1;
}

void process_gui() {
    if (refresh_requested.exchange(false, std::memory_order_acq_rel)) {
        if (auto *screen = Screens::Access()->Get()) {
            screen->Invalidate();
        }
    }
    if (printer_lock::locked()) {
        set_enabled(false);
        return;
    }
    if (!control_enabled.load(std::memory_order_acquire)) {
        return;
    }

    auto read = read_index.load(std::memory_order_relaxed);
    const auto write = write_index.load(std::memory_order_acquire);
    while (read != write) {
        const Command command = commands[read];
        switch (command.action) {
        case Action::encoder:
            gui::knob::EventEncoder(command.value);
            break;
        case Action::click:
            gui::knob::EventClick(BtnState_t::Pressed);
            gui::knob::EventClick(BtnState_t::Released);
            break;
        case Action::back:
            gui::knob::EventClick(BtnState_t::HeldAndReleased);
            break;
        case Action::home:
            Screens::Access()->CloseAll();
            Screens::Access()->ResetTimeout();
            break;
        }
        read = advance(read);
        read_index.store(read, std::memory_order_release);
    }
}

} // namespace serial_remote_control
