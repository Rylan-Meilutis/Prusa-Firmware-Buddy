#include "print_status_message_mgr.hpp"

#include <timing.h>
#include <serial_printing.hpp>

PrintStatusMessageManager print_status_message_instance;

PrintStatusMessageManager &print_status_message() {
    return print_status_message_instance;
}

static int serial_progress_percent(float current, float target) {
    if (target <= 0.0f) {
        return -1;
    }
    const float percent = current * 100.0f / target;
    if (percent <= 0.0f) {
        return 0;
    }
    if (percent >= 100.0f) {
        return 100;
    }
    return static_cast<int>(percent + 0.5f);
}

void report_print_status_to_serial_host(const PrintStatusMessage &message) {
#if PRINTER_IS_PRUSA_MINI()
    // MINI release builds are at the physical flash limit. Its serial UI still
    // observes these records, but richer outbound operation notifications are
    // reserved for the loadcell/MMU/toolchanger platforms.
    (void)message;
#else
    const char *label = nullptr;
    switch (message.type) {
    case PrintStatusMessage::homing: label = "Homing"; break;
    case PrintStatusMessage::homing_retrying: label = "Retrying homing"; break;
    case PrintStatusMessage::homing_refining: label = "Refining homing"; break;
    case PrintStatusMessage::recalibrating_home: label = "Recalibrating home"; break;
    case PrintStatusMessage::calibrating_axis: label = "Calibrating axis"; break;
    case PrintStatusMessage::probing_bed: label = "Probing bed"; break;
    case PrintStatusMessage::additional_probing: label = "Additional probing"; break;
    case PrintStatusMessage::dwelling: label = "Waiting"; break;
    case PrintStatusMessage::absorbing_heat: label = "Heat soaking"; break;
    case PrintStatusMessage::waiting_for_hotend_temp: label = "Heating hotend"; break;
    case PrintStatusMessage::waiting_for_bed_temp: label = "Heating bed"; break;
#if HAS_CHAMBER_API()
    case PrintStatusMessage::waiting_for_chamber_temp: label = "Heating chamber"; break;
#endif
#if ENABLED(PROBE_CLEANUP_SUPPORT)
    case PrintStatusMessage::nozzle_cleaning: label = "Cleaning nozzle"; break;
#endif
#if HAS_AUTO_RETRACT()
    case PrintStatusMessage::auto_retracting: label = "Retracting filament"; break;
#endif
#if HAS_TOOL_OFFSET_SENSOR()
    case PrintStatusMessage::tool_offset_calibrating: label = "Calibrating tool offset"; break;
#endif
    default: break;
    }
    if (!label) return;

    int progress = -1;
    if (const auto data = std::get_if<PrintStatusMessageDataProgress>(&message.data)) {
        progress = serial_progress_percent(data->current, message.type == PrintStatusMessage::absorbing_heat ? 100.0f : data->target);
    } else if (const auto data = std::get_if<PrintStatusMessageDataToolProgress>(&message.data)) {
        progress = serial_progress_percent(data->progress.current, data->progress.target);
    } else if (const auto data = std::get_if<PrintStatusMessageDataAxisProgress>(&message.data)) {
        progress = serial_progress_percent(data->progress.current, data->progress.target);
    }
    SerialPrinting::notify_status(label, progress);
#endif
}

PrintStatusMessageManager::Record PrintStatusMessageManager::current_message() const {
    std::scoped_lock mutex_guard(mutex_);

    if (temporary_message_.data) {
        return temporary_message_.data;
    }

    auto guard = active_guard_;
    while (guard) {
        const auto &data = guard->record();
        if (data.message.type != PrintStatusMessage::none) {
            return data;
        }

        guard = guard->parent_guard_;
    }

    return {};
}

uint32_t PrintStatusMessageManager::latest_id() const {
    std::scoped_lock mutex_guard(mutex_);
    return id_counter_ - 1;
}

void PrintStatusMessageManager::show_temporary(const Message &msg, uint32_t duration_ms) {
    {
        std::scoped_lock guard(mutex_);
        temporary_message_ = TemporaryMessage {
            .data {
                .message = msg,
                .id = id_counter_++,
            },
            .end_time_ms = ticks_ms() + duration_ms,
        };
        add_history_item_nolock(temporary_message_.data);
    }
    report_print_status_to_serial_host(msg);
}

void PrintStatusMessageManager::clear_timed_out_temporary() {
    std::scoped_lock guard(mutex_);
    if (temporary_message_.data && ticks_diff(ticks_ms(), temporary_message_.end_time_ms) >= 0) {
        temporary_message_.data = {};
    }
}

void PrintStatusMessageManager::clear_temporary() {
    std::scoped_lock guard(mutex_);
    temporary_message_ = {};
}

void PrintStatusMessageManager::walk_history(const stdext::inplace_function<bool(const Record &)> &callback) const {
    std::scoped_lock guard(mutex_);
    const auto end = history_pos_;

    auto pos = history_pos_;
    do {
        pos = (pos + 1) % history_buffer_size;
        const Record &msg = history_[pos];
        if (msg) {
            callback(msg);
        }
    } while (pos != end);
}

void PrintStatusMessageManager::add_history_item_nolock(const Record &msg) {
    // If the message ID is the same, update history, otherwise advance it
    if (history_[history_pos_].id != msg.id) {
        history_pos_ = (history_pos_ + 1) % history_buffer_size;
    }

    history_[history_pos_] = msg;
}
