#pragma once

#include <cstddef>
#include <cstdint>
#include <utils/byte_utils.hpp>
#include <utility>

namespace i2c {
using Address = uint8_t;

template <typename HWImpl>
concept Device = requires(HWImpl impl, Address address, size_t offset, Bytes tx_buf, WritableBytes rx_buf) {
    { impl.write_memory(address, offset, tx_buf) } -> std::same_as<bool>;
    { impl.read_memory(address, offset, rx_buf) } -> std::same_as<bool>;
    { impl.raw_transmit(address, offset, tx_buf) } -> std::same_as<bool>;
    { impl.raw_receive(address, offset, rx_buf) } -> std::same_as<bool>;
};
} // namespace i2c
