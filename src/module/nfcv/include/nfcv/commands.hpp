#pragma once

#include "types.hpp"

#include <cstdint>
#include <span>
#include <variant>

namespace nfcv {

namespace command {
    struct Inventory {
        struct Request {
        } request;
        struct Response {
            nfcv::UIDView uid;
        } response;
    };
    struct SystemInfo {
        struct Request {
            nfcv::UIDConstView uid;
        } request;
        using Response = TagInfo &;
        Response response;
    };

    struct ReadSingleBlock {
        struct Request {
            nfcv::UIDConstView uid;
            uint8_t block_address;
        } request;
        struct Response {
            std::span<std::byte> block_buffer;
        } response;
    };

    struct WriteSingleBlock {
        struct Request {
            nfcv::UIDConstView uid;
            uint8_t block_address;
            std::span<const std::byte> block_buffer;
        } request;
        struct Response {
        } response;
    };

    struct StayQuiet {
        struct Request {
            nfcv::UIDConstView uid;
        } request;
    };

} // namespace command

using Command = std::variant<command::Inventory, command::SystemInfo, command::ReadSingleBlock, command::WriteSingleBlock, command::StayQuiet>;

/// Utility function that checks if current command expects response
bool is_response_expected(const Command &command);
/// Utility function that checks if current command is "write-like" command
bool is_write_like_command(const Command &command);
} // namespace nfcv
