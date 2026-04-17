/// @file
#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

/// This is a lightweight RTT subsystem used for logging during development.
/// It is not a full-featured implementation of RTT or logging, but it is
/// useful in constrained environment such as INDX_HEAD board.
/// All this handles is a creation of RTT control block, exactly one RTT
/// buffer and a bunch of print functions.
///
/// To read the logs:
/// 1. start openocd session
/// 2. echo 'rtt setup 0x20000000 0x7800 "SEGGER RTT";rtt polling_interval 5;rtt start;rtt server start 5555 0' | nc localhost 4444
/// 3. telnet localhost 5555

namespace rtt {

/// Initialize RTT subsystem. Call this once.
void init();

namespace detail {

    /// Print single character via RTT subsystem.
    void print(char);

    /// Print byte as hexadecimal via RTT subsystem.
    void print(std::byte);

    /// Print 0-terminated string via RTT subsystem.
    inline void print(const char *str) {
        while (char c = *str++) {
            print(c);
        }
    }

    /// Print unsigned integer as decimal via RTT subsystem.
    void print(uint32_t);
    inline void print(uint16_t number) { print(static_cast<uint32_t>(number)); }
    inline void print(uint8_t number) { print(static_cast<uint32_t>(number)); }

    /// Print signed integer as decimal via RTT subsystem.
    void print(int32_t);
    inline void print(int16_t number) { print(static_cast<int32_t>(number)); }
    inline void print(int8_t number) { print(static_cast<int32_t>(number)); }

} // namespace detail

/// Print multiple entities sequentially via RTT subsystem.
template <class... Ts>
inline void print(Ts &&...ts) {
    (detail::print(std::forward<Ts>(ts)), ...);
}

} // namespace rtt
