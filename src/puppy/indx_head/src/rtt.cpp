/// @file
#include "rtt.hpp"

#include <bit>

/// Feel free to redefine to `true` when developing on `release` builds.
#define RTT_ENABLED() !defined(NDEBUG)

namespace rtt {

#if RTT_ENABLED()

    #warning "#undef RTT_ENABLED in release build"

namespace {

    /// See https://kb.segger.com/RTT for documentation
    struct ControlBlock {
        uint32_t id[4];
        uint32_t up_buffer_count;
        uint32_t down_buffer_count;

        // Technically, there should be an array of up buffers.
        // We use exactly one up buffer so we just inline it here.

        const char *up_buffer_name;
        char *up_buffer_data;
        uint32_t up_buffer_size;
        uint32_t up_buffer_write_offset;
        volatile uint32_t up_buffer_read_offset; // modified by host
        uint32_t up_buffer_flags;

        // Technically, there should be an array of down buffers.
        // We don't use any so we do not even bother.
    };

    ControlBlock control_block;
    constexpr size_t up_buffer_size = 1024; // feel free to redefine
    char up_buffer_data[up_buffer_size];

    #define memory_barrier() \
        asm volatile("" ::   \
                         : "memory")

    consteval uint32_t pack_id(char a, char b, char c, char d) {
        static_assert(std::endian::native == std::endian::little);
        return (a) | (b << 8) | (c << 16) | (d << 24);
    }

} // namespace

void init() {
    // Control block is located in .bss, save flash by initializing only
    // non-zero parts.
    control_block.up_buffer_count = 1;
    control_block.up_buffer_data = up_buffer_data;
    control_block.up_buffer_size = up_buffer_size;
    memory_barrier();
    // Reverse order and write by words; this is not using memcpy nor static
    // initialization because we want to prevent host from finding control
    // block in static data.
    control_block.id[2] = pack_id('T', 'T', '\0', '\0');
    control_block.id[1] = pack_id('E', 'R', ' ', 'R');
    control_block.id[0] = pack_id('S', 'E', 'G', 'G');
}

void detail::print(char c) {
    uint32_t write_offset = control_block.up_buffer_write_offset;
    uint32_t next_write_offset = write_offset + 1;
    if (next_write_offset == up_buffer_size) {
        next_write_offset = 0;
    }

    if (next_write_offset != control_block.up_buffer_read_offset) {
        up_buffer_data[write_offset] = c;
        memory_barrier();
        control_block.up_buffer_write_offset = next_write_offset;
    } else {
        // host can't keep up, so just discard the character
    }
}

#else

void init() {}
void detail::print(char) {}

#endif

void detail::print(uint32_t number) {
    char buffer[10]; // enough for UINT32_MAX
    size_t count = 0;
    do {
        // Derive remainder from quotient to avoid double division on M0+
        const uint32_t quotient = number / 10;
        const uint32_t remainder = number - quotient * 10;
        buffer[count++] = '0' + remainder;
        number = quotient;
    } while (number > 0);

    while (count) {
        print(buffer[--count]);
    }
}

void detail::print(int32_t number) {
    if (number < 0) {
        print('-');
        print(static_cast<uint32_t>(~number) + 1u);
    } else {
        print(static_cast<uint32_t>(number));
    }
}

void detail::print(std::byte byte) {
    static constexpr const char hex_digits[] { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
    const auto v = to_integer<uint8_t>(byte);
    print(hex_digits[(v >> 4) & 0xf]);
    print(hex_digits[(v >> 0) & 0xf]);
}

} // namespace rtt
