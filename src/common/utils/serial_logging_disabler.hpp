/// @file
#pragma once

#include <utils/uncopyable.hpp>

/// RAII guard that disables sending serial messages to the logging subsystem
/// Only to be used on the marlin task
class SerialLoggingDisabler : public Uncopyable {

public:
    SerialLoggingDisabler() {
        instances_++;
    }
    ~SerialLoggingDisabler() {
        instances_--;
    }

    static bool is_logging_disabled() {
        return instances_ > 0;
    }

private:
    static inline uint8_t instances_ = 0;
};
