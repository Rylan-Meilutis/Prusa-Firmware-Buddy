#pragma once

#include "error.hpp"
#include "types.hpp"
#include "commands.hpp"

#include <cstddef>
#include <expected>

#include <utils/uncopyable.hpp>

namespace nfcv {

class ReaderWriterInterface {
public:
    using AntennaData = uintptr_t;

    virtual Result<void> field_up(AntennaData antenna_data) = 0;
    virtual void field_down() = 0;

    virtual AntennaData switch_to_next_discovery_atenna() = 0;

    [[nodiscard]] virtual nfcv::Result<void> nfcv_command(const Command &command) = 0;

public: //* Utility wrappers for nfcv commands
    Result<UID> inventory();
    Result<void> stay_quiet(const UID &uid);
    Result<TagInfo> get_system_info(const UID &uid);
    Result<void> read_single_block(const UID &uid, BlockID block_id, const std::span<std::byte> &buffer);
    Result<void> write_single_block(const UID &uid, BlockID block_id, const std::span<const std::byte> &buffer);
};

class FieldGuard : Uncopyable {

public:
    FieldGuard(ReaderWriterInterface &reader, ReaderWriterInterface::AntennaData antenna)
        : reader(reader)
        , result(reader.field_up(antenna)) {
    }

    ~FieldGuard() {
        if (result) {
            reader.field_down();
        }
    }

    ReaderWriterInterface &reader;
    const Result<void> result;
};

} // namespace nfcv
