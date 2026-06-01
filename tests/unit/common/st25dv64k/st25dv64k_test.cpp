// Tests the real st25dv64k.cpp driver against a mocked I2C layer + an in-memory EEPROM.

#include <catch2/catch_test_macros.hpp>

#include <common/st25dv64k.h>
#include <i2c.hpp>
#include <bsod.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

struct MockEeprom {
    std::array<uint8_t, 8192> mem;
    uint16_t read_pointer = 0;
    int mem_writes = 0; // counts Mem_Write calls, so tests can assert page-coalescing
    void reset() {
        mem.fill(0xff);
        read_pointer = 0;
        mem_writes = 0;
    }
};
MockEeprom g_eeprom;

constexpr uint16_t PAGE = 4;
constexpr uint16_t MAX_FRAME = 256;

} // namespace

namespace i2c {

Result Transmit(I2C_HandleTypeDef &, uint16_t, uint8_t *pData, uint16_t Size, uint32_t) {
    if (Size == 2) { // sets the read pointer before a Receive
        g_eeprom.read_pointer = (static_cast<uint16_t>(pData[0]) << 8) | pData[1];
    }
    return Result::ok;
}

Result Receive(I2C_HandleTypeDef &, uint16_t, uint8_t *pData, uint16_t Size, uint32_t) {
    std::memcpy(pData, g_eeprom.mem.data() + g_eeprom.read_pointer, Size);
    return Result::ok;
}

Result Mem_Write_16bit_Addr(I2C_HandleTypeDef &, uint16_t, uint16_t MemAddress, uint8_t *pData, uint16_t Size, uint32_t) {
    // Guard against crossing a page boundary: either page-aligned whole pages, or a sub-page write
    // wholly within one page.
    REQUIRE(Size >= 1);
    REQUIRE(Size <= MAX_FRAME);
    const bool aligned_whole_pages = (MemAddress % PAGE == 0) && (Size % PAGE == 0);
    const bool within_one_page = (MemAddress % PAGE) + Size <= PAGE;
    REQUIRE((aligned_whole_pages || within_one_page));
    REQUIRE(MemAddress + Size <= g_eeprom.mem.size());
    std::memcpy(g_eeprom.mem.data() + MemAddress, pData, Size);
    ++g_eeprom.mem_writes;
    return Result::ok;
}

Result IsDeviceReady(I2C_HandleTypeDef &, uint16_t, uint32_t, uint32_t) {
    return Result::ok;
}

Result Mem_Read_16bit_Addr(I2C_HandleTypeDef &, uint16_t, uint16_t MemAddress, uint8_t *pData, uint16_t Size, uint32_t) {
    std::memcpy(pData, g_eeprom.mem.data() + MemAddress, Size);
    return Result::ok;
}

} // namespace i2c

// Happy-path tests never hit this; throw so any unexpected error path fails loudly.
[[noreturn]] void fatal_error(const ErrCode, ...) {
    throw std::runtime_error("fatal_error(ErrCode) called");
}

extern "C" uint32_t ticks_ms() {
    static uint32_t tick = 0;
    return tick++;
}

namespace {

struct Driver {
    std::array<uint8_t, MockEeprom().mem.size()> reference;
    Driver() {
        st25dv64k_user_write_bytes_flush(); // drain the file-global write_buffer from a prior SECTION
        g_eeprom.reset();
        reference.fill(0xff);
    }

    void write(uint16_t address, const uint8_t *data, uint16_t size) {
        st25dv64k_user_write_bytes_buffered(address, data, size);
        std::memcpy(reference.data() + address, data, size);
    }

    void flush() {
        st25dv64k_user_write_bytes_flush();
    }

    void check_committed() const {
        REQUIRE(std::memcmp(g_eeprom.mem.data(), reference.data(), reference.size()) == 0);
    }
};

} // namespace

TEST_CASE("st25dv64k buffered writes are byte-correct and coalesce pages") {
    SECTION("aligned whole-page run goes through as one bulk frame") {
        Driver d;
        const uint8_t data[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
        d.write(0x500, data, sizeof(data));
        d.flush();
        d.check_committed();
        REQUIRE(g_eeprom.mem_writes == 1);
    }

    SECTION("a sub-page write stays buffered until flush") {
        Driver d;
        const uint8_t data[] = { 0x10, 0x20 };
        d.write(0x500, data, sizeof(data));
        REQUIRE(g_eeprom.mem_writes == 0);
        d.flush();
        REQUIRE(g_eeprom.mem_writes == 1);
        d.check_committed();
    }

    SECTION("contiguous sub-page writes coalesce into one page program") {
        Driver d;
        const uint8_t a = 0x11, b = 0x22, c = 0x33;
        d.write(0x500, &a, 1);
        d.write(0x501, &b, 1);
        d.write(0x502, &c, 1);
        d.flush();
        d.check_committed();
        REQUIRE(g_eeprom.mem_writes == 1);
    }

    SECTION("header+data sharing a page programs that page only once (the migration pattern)") {
        Driver d;
        const uint8_t header[] = { 0x01, 0x02, 0x03 };
        const uint8_t data[] = { 0x04, 0x05, 0x06, 0x07, 0x08 };
        d.write(0x500, header, sizeof(header));
        d.write(0x503, data, sizeof(data)); // fills 0x500, spills to 0x504
        d.flush();
        d.check_committed();
        REQUIRE(g_eeprom.mem_writes == 2);
    }

    SECTION("misaligned start is split by the head") {
        Driver d;
        const uint8_t data[] = { 0x40, 0x41, 0x42, 0x43, 0x44 };
        d.write(0x503, data, sizeof(data)); // off 3: head 1B then aligned 4B
        d.flush();
        d.check_committed();
    }

    SECTION("a non-contiguous write commits the stale buffer first") {
        Driver d;
        const uint8_t a = 0x11, b = 0x22;
        d.write(0x500, &a, 1);
        d.write(0x600, &b, 1);
        REQUIRE(g_eeprom.mem_writes == 1);
        d.flush();
        REQUIRE(g_eeprom.mem_writes == 2);
        d.check_committed();
    }

    SECTION("a write larger than MaxFrame is chunked into <=MaxFrame frames") {
        Driver d;
        const std::vector<uint8_t> data(600, 0xAB);
        d.write(0x500, data.data(), data.size()); // 256 + 256 + 88
        d.flush();
        d.check_committed();
        REQUIRE(g_eeprom.mem_writes == 3);
    }

    SECTION("a single write spanning head + bulk + tail") {
        Driver d;
        const uint8_t data[] = { 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC };
        d.write(0x503, data, sizeof(data)); // head(1) + bulk(8) + tail(4)
        d.flush();
        d.check_committed();
    }

    SECTION("st25dv64k_user_read_bytes flushes buffered writes so reads see them") {
        Driver d;
        const uint8_t data[] = { 0xDE, 0xDF, 0xE0 };
        d.write(0x500, data, sizeof(data));
        REQUIRE(g_eeprom.mem_writes == 0);
        uint8_t out[3] {};
        st25dv64k_user_read_bytes(0x500, out, sizeof(out));
        REQUIRE(g_eeprom.mem_writes == 1);
        REQUIRE(std::memcmp(out, data, sizeof(out)) == 0);
    }

    SECTION("fuzz: random contiguous/jumping writes match the naive reference") {
        Driver d;
        std::mt19937 rng(0xC0FFEE);
        uint16_t address = 0x500;
        for (int i = 0; i < 2000; ++i) {
            if (rng() % 4 == 0) {
                address = static_cast<uint16_t>(0x500 + rng() % 0x1000);
            }
            const uint16_t size = 1 + rng() % 40;
            if (address + size > 0x1F00) {
                address = 0x500;
            }
            std::vector<uint8_t> data(size);
            for (auto &byte : data) {
                byte = static_cast<uint8_t>(rng());
            }
            d.write(address, data.data(), size);
            address += size;
            if (rng() % 8 == 0) {
                d.flush();
            }
        }
        d.flush();
        d.check_committed();
    }
}
