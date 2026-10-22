#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace buddy::serial_collision {

// No heap allocation. Completed prefixes retire only after their physical
// planner blocks are discarded. State-changing commands are drain barriers.
template <size_t CommandSize, size_t Capacity = 8>
class Journal {
public:
    static constexpr uint32_t invalid = UINT32_MAX;
    struct Record {
        std::array<char, CommandSize> command {};
        float feedrate = 0;
        uint32_t token = invalid;
        uint8_t relative = 0;
    };

    bool full() const { return count_ == Capacity; }
    bool replaying() const { return replay_ < count_; }
    bool frozen() const { return frozen_; }
    void clear() {
        count_ = 0;
        replay_ = Capacity;
        frozen_ = false;
    }

    template <class InFlight>
    void retire_completed(InFlight in_flight) {
        if (frozen_) {
            return;
        }
        size_t retired = 0;
        while (retired < count_ && !in_flight(records_[retired].token)) {
            ++retired;
        }
        for (size_t i = retired; i < count_; ++i) {
            records_[i - retired] = records_[i];
        }
        count_ -= retired;
    }

    const Record *append(const char *command, float feedrate, uint8_t relative) {
        if (full() || frozen_ || std::strlen(command) >= CommandSize) {
            return nullptr;
        }
        auto &record = records_[count_++];
        std::strcpy(record.command.data(), command);
        record.feedrate = feedrate;
        record.relative = relative;
        record.token = next_++;
        if (next_ == invalid) {
            next_ = 0;
        }
        return &record;
    }

    bool freeze(uint32_t token) {
        frozen_ = true;
        replay_ = Capacity;
        for (size_t i = 0; i < count_; ++i) {
            if (records_[i].token == token) {
                replay_ = i;
                return true;
            }
        }
        return false;
    }

    const Record *next() const { return replaying() ? &records_[replay_] : nullptr; }
    void advance() {
        if (replaying()) {
            ++replay_;
        }
        if (!replaying()) {
            frozen_ = false;
        }
    }

private:
    std::array<Record, Capacity> records_ {};
    size_t count_ = 0;
    size_t replay_ = Capacity;
    uint32_t next_ = 1;
    bool frozen_ = false;
};

} // namespace buddy::serial_collision
