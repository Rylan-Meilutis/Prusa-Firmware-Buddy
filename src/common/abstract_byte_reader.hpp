/// @file
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <utils/byte_utils.hpp>

/// Generic interface for reading bytes into buffer.
class AbstractByteReader {
public:
    /// Read bytes into provided buffer, return valid subspan of that buffer.
    virtual WritableBytes read(WritableBytes buffer) = 0;

    /// Wrapper for clients who haven't embraced beauty of std::byte yet.
    std::span<uint8_t> read(std::span<uint8_t> buffer) {
        WritableBytes in { (std::byte *)buffer.data(), buffer.size() };
        WritableBytes out { read(in) };
        return { (uint8_t *)out.data(), out.size() };
    }
};
