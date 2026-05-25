#pragma once

#include <atomic>
#include <cstdint>

#include "storage.hpp"

class EEPROMStorage : public configuration_store::Storage {
    // TODO detect errors and return them
public:
    void read_bytes(uint16_t address, std::span<uint8_t> buffer) override;
    void write_bytes(uint16_t address, std::span<const uint8_t> data) override;
};

inline configuration_store::Storage &EEPROMInstance() {
    static EEPROMStorage storage {};
    return storage;
}
