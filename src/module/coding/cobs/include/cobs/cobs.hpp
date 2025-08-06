/// \brief Implements Consistent Overhead Byte Stuffing (COBS) for reliable packet framing in serial communication.
///
/// This header file provides functions for encoding and decoding data using the
/// Consistent Overhead Byte Stuffing (COBS) algorithm. COBS ensures that the
/// data bytes do not contain the packet delimiter (0x00), making packet
/// framing unambiguous.
///
/// The implementation is based on the algorithm described on https://www.wikiwand.com/en/articles/Consistent_Overhead_Byte_Stuffing

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <expected>

namespace cobs {

enum class CobsError {
    invalid_input,
    overflow
};

/** COBS encode data to buffer
        @param data Pointer to input data to encode
        @param data_len Number of bytes to encode
        @param buffer Pointer to encoded output buffer
        @param buffer_len Size of the output buffer
        @return Encoded buffer length in bytes
        @note Does not output delimiter byte
*/
inline std::expected<size_t, CobsError> encode(const void *data, size_t data_len, uint8_t *buffer, size_t buffer_len) {
    assert(data && buffer);
    if (!data || !buffer || data_len == 0 || buffer_len == 0) {
        return std::unexpected(CobsError::invalid_input);
    }

    uint8_t *encode = buffer; // Encoded byte pointer
    uint8_t *codep = encode++; // Output code pointer
    uint8_t code = 1; // Code value

    for (const uint8_t *byte = (const uint8_t *)data; data_len--; ++byte) {
        if ((size_t)(encode - buffer) >= buffer_len) {
            return std::unexpected(CobsError::overflow);
        }
        if (*byte) { // Byte not zero, write it
            *encode++ = *byte, ++code;
        }

        if (!*byte || code == 0xff) { // Input is zero or block completed, restart
            *codep = code, code = 1, codep = encode;
            if (!*byte || data_len) {
                ++encode;
            }
        }
    }
    *codep = code; // Write final code value

    return (size_t)(encode - buffer);
}

/** COBS decode data from buffer
        @param buffer Pointer to encoded input bytes
        @param buffer_len Number of bytes to decode
        @param data Pointer to decoded output data
        @return Number of bytes successfully decoded
        @note Stops decoding if delimiter byte is found
*/
inline std::expected<size_t, CobsError> decode(const uint8_t *buffer, size_t buffer_len, void *data, size_t data_len) {
    if (!data || !buffer || buffer_len == 0) {
        assert(false);
        return std::unexpected(CobsError::invalid_input);
    }

    const uint8_t *byte = buffer; // Encoded input byte pointer
    uint8_t *decode = (uint8_t *)data; // Decoded output byte pointer

    for (uint8_t code = 0xff, block = 0; byte < buffer + buffer_len; --block) {
        if ((size_t)(decode - (uint8_t *)data) >= data_len) {
            return std::unexpected(CobsError::overflow);
        }

        if (block) { // Decode block byte
            *decode++ = *byte++;
        } else {
            block = *byte++; // Fetch the next block length
            if (block && (code != 0xff)) { // Encoded zero, write it unless it's delimiter.
                *decode++ = 0;
            }
            code = block;
            if (!code) { // Delimiter code found
                break;
            }
        }
    }

    if (size_t(byte - buffer) != buffer_len) {
        return std::unexpected(CobsError::invalid_input);
    }

    return (size_t)(decode - (uint8_t *)data);
}
} // namespace cobs
