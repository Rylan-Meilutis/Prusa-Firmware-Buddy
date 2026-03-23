// Generic queue for bridging raw Cyphal messages over Modbus.
// Wire format: [1B payload_length | 2B port_id LE | payload].
#pragma once

#include "yet_another_circular_buffer.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

template <size_t buffer_size>
class CyphalBridgeQueue {
public:
    static constexpr size_t header_size = 3; // 1B length + 2B port_id
    static constexpr size_t max_payload = 64;

    void enqueue(uint16_t port_id, const void *data, uint8_t size) {
        const std::byte header[header_size] = {
            static_cast<std::byte>(size),
            static_cast<std::byte>(port_id & 0xFF),
            static_cast<std::byte>(port_id >> 8),
        };

        // Intentional abort: the buffer must always have room. The Modbus
        // master drains faster than Cyphal produces, so overflow here
        // indicates a logic error (e.g. missing drain), not normal backpressure.
        if (!buffer_.try_write(header, header_size)
            || !buffer_.try_write(static_cast<const std::byte *>(data), size)) {
            abort();
        }
        ++message_count_;
    }

    // Drain complete messages into out[0..max_bytes). Returns bytes written.
    size_t read_into(std::byte *out, size_t max_bytes) {
        size_t written = 0;

        // Flush the stashed overflow message from a previous call first
        if (overflow_size_ > 0) {
            if (overflow_size_ > max_bytes) {
                return 0;
            }
            memcpy(out, overflow_, overflow_size_);
            written = overflow_size_;
            overflow_size_ = 0;
        }

        while (message_count_ > 0) {
            // Read header + payload from the ring buffer
            std::byte header[header_size];
            if (!buffer_.try_read(header, header_size)) {
                break;
            }

            const uint8_t payload_len = static_cast<uint8_t>(header[0]);
            const size_t msg_total = header_size + payload_len;

            // Must succeed -- enqueue writes header+payload atomically
            if (payload_len > max_payload) {
                abort();
            }
            std::byte payload[max_payload];
            if (payload_len > 0 && !buffer_.try_read(payload, payload_len)) {
                abort();
            }

            --message_count_;

            if (written + msg_total > max_bytes) {
                // Won't fit -- stash for next call
                memcpy(overflow_, header, header_size);
                memcpy(overflow_ + header_size, payload, payload_len);
                overflow_size_ = msg_total;
                break;
            }

            memcpy(out + written, header, header_size);
            memcpy(out + written + header_size, payload, payload_len);
            written += msg_total;
        }

        return written;
    }

    size_t bytes_available() const {
        return buffer_.size() + overflow_size_;
    }

    size_t message_count() const {
        return message_count_ + (overflow_size_ > 0 ? 1 : 0);
    }

    void clear() {
        buffer_.clear();
        message_count_ = 0;
        overflow_size_ = 0;
    }

private:
    YetAnotherCircularBuffer<buffer_size> buffer_;
    size_t message_count_ = 0;

    // Staging area for one message dequeued from buffer_ that didn't fit in output
    static constexpr size_t max_overflow = header_size + max_payload;
    std::byte overflow_[max_overflow] = {};
    size_t overflow_size_ = 0;
};
