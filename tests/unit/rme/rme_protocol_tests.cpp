#include <rme_protocol_parser.hpp>
#include <rme_file_transfer.hpp>
#include <rme_firmware_status.hpp>
#include <m976_material.hpp>
#include <m976_temperature_policy.hpp>
#include <filament_material.hpp>
#include <filament_material_family_storage.hpp>
#include <firmware_update_handoff.hpp>
#include <task_stack_requirements.hpp>

#if __has_include(<catch2/catch_test_macros.hpp>)
    #include <catch2/catch_test_macros.hpp>
#else
    #include <catch2/catch.hpp>
#endif

#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>
#include <vector>

using namespace std::string_view_literals;

TEST_CASE("Material families preserve the deployed EEPROM1 representation", "[rme][filament][regression]") {
    using namespace buddy::filament_material_family_storage;

    CHECK_FALSE(decode(0, 10).has_value());
    for (uint8_t preset = 0; preset < 10; ++preset) {
        const auto stored = encode(preset);
        CHECK(stored == preset + 1);
        CHECK(decode(stored, 10) == preset);
    }
    CHECK_FALSE(decode(31, 10).has_value());
}

TEST_CASE("RME Marlin task stack retains crash-derived guard space", "[rme][stack][regression]") {
    using namespace buddy::task_stack_requirements;

    CHECK(marlin_words == 1664);
    CHECK(marlin_guard_bytes >= required_guard_bytes);
    CHECK(marlin_guard_bytes >= observed_rme_overrun_bytes * 4);
}

TEST_CASE("RME firmware status accepts only matching verified metadata", "[rme][firmware][regression]") {
    rme_firmware_status::VerifiedMetadata metadata {};
    metadata.size = 4'000'000;
    metadata.sha256[0] = 0x42;

    CHECK(rme_firmware_status::valid(metadata, 4'000'000));
    CHECK_FALSE(rme_firmware_status::valid(metadata, 3'999'999));
    metadata.magic = 0;
    CHECK_FALSE(rme_firmware_status::valid(metadata, 4'000'000));
    metadata.magic = rme_firmware_status::verified_metadata_magic;
    metadata.version++;
    CHECK_FALSE(rme_firmware_status::valid(metadata, 4'000'000));
}

TEST_CASE("RME firmware query deadlines are wrap-safe", "[rme][firmware][regression]") {
    CHECK_FALSE(rme_firmware_status::elapsed(999, 0, 1'000));
    CHECK(rme_firmware_status::elapsed(1'000, 0, 1'000));
    CHECK(rme_firmware_status::elapsed(5, UINT32_MAX - 5, 10));
    CHECK_FALSE(rme_firmware_status::elapsed(5, UINT32_MAX - 5, 12));
}

TEST_CASE("RME firmware validation rejects truncated and failed reads", "[rme][firmware][regression]") {
    CHECK(rme_firmware_status::complete_read_valid(4096, 4096, true, false));
    CHECK_FALSE(rme_firmware_status::complete_read_valid(2048, 4096, true, false));
    CHECK_FALSE(rme_firmware_status::complete_read_valid(4096, 4096, false, false));
    CHECK_FALSE(rme_firmware_status::complete_read_valid(4096, 4096, true, true));
}

TEST_CASE("M976 validates material family independently of custom profile name", "[rme][m976][regression]") {
    using buddy::m976_material::fallback;
    using buddy::m976_material::matches;

    CHECK(matches("PETG"sv, "PET-00L"sv, "PETG"sv));
    CHECK_FALSE(matches("PLA"sv, "PET-00L"sv, "PETG"sv));
    CHECK(matches("PETG"sv, "PETG"sv, "PETG"sv));
    CHECK(matches("PET-00L"sv, "PET-00L"sv, "PETG"sv));
    CHECK(fallback("PET-00L"sv, "PETG"sv) == 0.045f);
    CHECK(matches("PETG-CF"sv, "PETG-CF"sv, {}));
    CHECK_FALSE(matches("PETG"sv, "PETG-CF"sv, {}));
    CHECK(fallback("PETG-CF"sv, {}) == 0.045f);
}

TEST_CASE("M976 MMU service temperature stays above cold extrusion cutoff", "[rme][m976][temperature][regression]") {
    using namespace buddy::m976_temperature_policy;

    CHECK(probe_temperature(170) == 175);
    CHECK(probe_temperature(170) > 170);
    CHECK_FALSE(wait_for_restored_target);
}

TEST_CASE("External filament material never exposes the custom profile name", "[rme][filament][regression]") {
    using buddy::filament_material::authoritative_name;

    CHECK(authoritative_name("PLA-00D", "PLA") == "PLA");
    CHECK(authoritative_name("PET-00L", "PETG") == "PETG");
    CHECK(authoritative_name("PA-CF", {}) == "PA-CF");
}

TEST_CASE("RME action names are complete tokens", "[rme][regression]") {
    CHECK(rme_protocol::action_is("ABORT"sv, "ABORT"sv));
    CHECK(rme_protocol::action_is("READ offset=0"sv, "READ"sv));
    CHECK_FALSE(rme_protocol::action_is("ABORTgarbage"sv, "ABORT"sv));
    CHECK_FALSE(rme_protocol::action_is("READ_BINARY"sv, "READ"sv));
    CHECK_FALSE(rme_protocol::action_is("READ\toffset=0"sv, "READ"sv));
}

TEST_CASE("RME parameters are token bounded", "[rme]") {
    constexpr auto command = "@RME FILAMENT SET slot=7 name=NEW nozzle=215 visible=1 tx=3933926906*42"sv;
    CHECK(rme_protocol::value(command, "slot") == "7");
    CHECK(rme_protocol::value(command, "name") == "NEW");
    CHECK(rme_protocol::value(command, "visible") == "1");
    CHECK_FALSE(rme_protocol::value(command, "set"));
    CHECK_FALSE(rme_protocol::value(command, "missing"));

    // A key embedded in another token must never alias the requested key.
    CHECK_FALSE(rme_protocol::value("@RME X nottx=9"sv, "tx"));
    CHECK(rme_protocol::value("@RME X tx=9 extra=1"sv, "tx") == "9");
}

TEST_CASE("RME transaction IDs preserve the complete uint32 range", "[rme][regression]") {
    CHECK(rme_protocol::transaction("@RME X tx=0"sv) == 0);
    CHECK(rme_protocol::transaction("@RME X tx=2147483648"sv) == UINT32_C(2147483648));
    CHECK(rme_protocol::transaction("@RME X tx=3933926906"sv) == UINT32_C(3933926906));
    CHECK(rme_protocol::transaction("@RME X tx=4294967295"sv) == std::numeric_limits<uint32_t>::max());
    CHECK_FALSE(rme_protocol::transaction("@RME X tx=4294967296"sv));
    CHECK_FALSE(rme_protocol::transaction("@RME X tx=-1"sv));
    CHECK_FALSE(rme_protocol::transaction("@RME X tx=12x"sv));
    CHECK_FALSE(rme_protocol::transaction("@RME X tx="sv));
}

TEST_CASE("RME numeric parameters reject malformed and overflowing values", "[rme]") {
    CHECK(rme_protocol::signed_number("@RME UI ENCODER value=-100"sv, "value") == -100);
    CHECK(rme_protocol::signed_number("@RME THEME SET primary=#4B2AC3"sv, "primary", 16) == 0x4B2AC3);
    CHECK_FALSE(rme_protocol::signed_number("@RME X value=1garbage"sv, "value"));
    CHECK_FALSE(rme_protocol::signed_number("@RME X value=999999999999999999999"sv, "value"));
    CHECK(rme_protocol::unsigned_number("@RME FILE READ offset=1073741824"sv, "offset") == UINT32_C(1073741824));
    CHECK_FALSE(rme_protocol::unsigned_number("@RME FILE READ offset=-1"sv, "offset"));
}

TEST_CASE("RME service frames remain isolated from ordinary G-code", "[rme]") {
    CHECK(rme_protocol::is_service_frame("@RME SESSION KEEPALIVE"sv));
    CHECK(rme_protocol::is_service_frame("  @RME FILE ABORT"sv));
    CHECK(rme_protocol::is_service_frame("N123 @RME DIALOG QUERY*7"sv));
    CHECK_FALSE(rme_protocol::is_service_frame("M117 @RME SESSION KEEPALIVE"sv));
    CHECK_FALSE(rme_protocol::is_service_frame("@RME"sv));
    CHECK_FALSE(rme_protocol::is_service_frame("RME SESSION KEEPALIVE"sv));
    CHECK_FALSE(rme_protocol::is_service_frame("=60 visible=1 tx=3933926903"sv));
    CHECK_FALSE(rme_protocol::is_service_frame("N-7"sv));
}

TEST_CASE("RME percent decoding is strict and fixed capacity", "[rme]") {
    const auto decoded = rme_protocol::percent_decode<32>("Prusa%20%2F%20Prusament"sv);
    REQUIRE(decoded);
    CHECK(std::string_view(decoded->data()) == "Prusa / Prusament");

    const auto mixed_case_hex = rme_protocol::percent_decode<16>("3D%20Fuel"sv);
    REQUIRE(mixed_case_hex);
    CHECK(std::string_view(mixed_case_hex->data()) == "3D Fuel");
    CHECK_FALSE(rme_protocol::percent_decode<16>("bad%2"sv));
    CHECK_FALSE(rme_protocol::percent_decode<16>("bad%XZ"sv));
    CHECK_FALSE(rme_protocol::percent_decode<8>("12345678"sv));
    CHECK_FALSE(rme_protocol::percent_decode<16>("zero%00inside"sv));
    CHECK_FALSE(rme_protocol::percent_decode<16>(""sv));
    CHECK(rme_protocol::percent_decode<5>("a"sv).has_value());
}

TEST_CASE("RME filesystem paths are rooted and traversal safe", "[rme][file]") {
    const auto normal = rme_protocol::usb_path<64>("folder%20one/file.gcode"sv);
    REQUIRE(normal);
    CHECK(std::string_view(normal->data()) == "/usb/folder one/file.gcode");

    const auto leading_slashes = rme_protocol::usb_path<64>("///FWUPD.BBF"sv);
    REQUIRE(leading_slashes);
    CHECK(std::string_view(leading_slashes->data()) == "/usb/FWUPD.BBF");

    const auto root = rme_protocol::usb_path<64>(""sv);
    REQUIRE(root);
    CHECK(std::string_view(root->data()) == "/usb/");

    CHECK_FALSE(rme_protocol::usb_path<64>("../secret"sv));
    CHECK_FALSE(rme_protocol::usb_path<64>("safe/%2e%2e/secret"sv));
    CHECK_FALSE(rme_protocol::usb_path<64>("safe//file"sv));
    CHECK_FALSE(rme_protocol::usb_path<16>("this-name-is-too-long.gcode"sv));
    CHECK_FALSE(rme_protocol::usb_path<5>("file"sv));
    CHECK_FALSE(rme_protocol::usb_path<8>("abc"sv));
}

TEST_CASE("RME SHA-256 parsing is exact", "[rme][file]") {
    std::array<uint8_t, 32> digest {};
    REQUIRE(rme_protocol::parse_sha256("000102030405060708090a0b0c0d0e0f101112131415161718191A1B1C1D1E1F"sv, digest));
    for (size_t i = 0; i < digest.size(); ++i) {
        CHECK(digest[i] == i);
    }
    CHECK_FALSE(rme_protocol::parse_sha256("00"sv, digest));
    CHECK_FALSE(rme_protocol::parse_sha256("000102030405060708090a0b0c0d0e0f101112131415161718191A1B1C1D1E1Z"sv, digest));
}

TEST_CASE("RME parser state does not accumulate across repeated synchronization", "[rme][stress]") {
    constexpr auto command = "@RME FILAMENT SET slot=4 name=PLA-00H nozzle=215 preheat=175 bed=60 visible=1 tx=3933926903"sv;
    for (size_t iteration = 0; iteration < 100000; ++iteration) {
        REQUIRE(rme_protocol::is_service_frame(command));
        REQUIRE(rme_protocol::unsigned_number(command, "slot") == 4);
        REQUIRE(rme_protocol::value(command, "name") == "PLA-00H");
        REQUIRE(rme_protocol::transaction(command) == UINT32_C(3933926903));
    }
}

TEST_CASE("RME serial dispatch rejects recursive draining", "[rme][file][stream][regression]") {
    bool active = false;
    {
        rme_protocol::ScopedDispatchGuard outer { active };
        REQUIRE(outer);
        CHECK(active);

        // A filesystem wait may service the main loop while the outer bulk
        // frame still owns the receive buffer.  The nested reader must leave
        // all byte-stream state untouched.
        rme_protocol::ScopedDispatchGuard nested { active };
        CHECK_FALSE(nested);
        CHECK(active);
    }
    CHECK_FALSE(active);

    // The next scheduler pass can drain the already-buffered CDC bytes.
    rme_protocol::ScopedDispatchGuard next_pass { active };
    CHECK(next_pass);
}

TEST_CASE("RME binary transfer failures have stable diagnostics", "[rme][file]") {
    using rme_file_transfer::BinaryFrameError;
    using rme_file_transfer::classify_binary_frame;
    using rme_file_transfer::diagnostic_code;

    CHECK(classify_binary_frame(12288, 12288, 1024, 4096000, true) == BinaryFrameError::none);
    CHECK(classify_binary_frame(13312, 12288, 1024, 4096000, true) == BinaryFrameError::offset_mismatch);
    CHECK(classify_binary_frame(4096000, 4096000, 1, 4096000, true) == BinaryFrameError::size_exceeded);
    CHECK(classify_binary_frame(12288, 12288, 1024, 4096000, false) == BinaryFrameError::crc_mismatch);
    CHECK(classify_binary_frame(0, 1, 1, 0, true) == BinaryFrameError::offset_mismatch);

    CHECK(std::string_view(diagnostic_code(BinaryFrameError::offset_mismatch)) == "offset_mismatch");
    CHECK(std::string_view(diagnostic_code(BinaryFrameError::size_exceeded)) == "size_exceeded");
    CHECK(std::string_view(diagnostic_code(BinaryFrameError::crc_mismatch)) == "crc_mismatch");
    CHECK(diagnostic_code(BinaryFrameError::none) == nullptr);
    CHECK(std::string_view(diagnostic_code(static_cast<BinaryFrameError>(255))) == "invalid_frame");
}

TEST_CASE("RME text bulk receive backlog matches the advertised window", "[rme][file][regression]") {
    CHECK(rme_file_transfer::bulk_payload_size == 384);
    CHECK(rme_file_transfer::bulk_window_size == 4);
    CHECK(rme_file_transfer::bulk_base64_size == 512);
    CHECK(rme_file_transfer::bulk_receive_backlog <= 2048);
    CHECK(rme_file_transfer::bulk_receive_backlog > 512);
}

TEST_CASE("RME binary window fits completely in the target CDC FIFO", "[rme][file][stream][regression]") {
    CHECK(rme_file_transfer::binary_header_size == 10);
    CHECK(rme_file_transfer::binary_payload_size == 512);
    CHECK(rme_file_transfer::binary_window_size == 3);
    CHECK(rme_file_transfer::binary_receive_backlog == 1566);
    CHECK(rme_file_transfer::binary_receive_backlog <= 2048);
}

TEST_CASE("RME text abort escapes malformed binary mode", "[rme][file][regression]") {
    uint8_t matched = 0;
    constexpr std::string_view abort = "@RME FILE ABORT\n";
    for (size_t i = 0; i + 1 < abort.size(); ++i) {
        CHECK_FALSE(rme_file_transfer::consume_text_abort(abort[i], matched, i >= 9));
    }
    CHECK(rme_file_transfer::consume_text_abort(abort.back(), matched, true));
    CHECK(matched == 0);

    // Valid binary payload bytes must never activate the compatibility escape.
    for (const char byte : abort) {
        CHECK_FALSE(rme_file_transfer::consume_text_abort(byte, matched, false));
    }
}

TEST_CASE("RME binary control offsets do not overlap data", "[rme][file]") {
    CHECK(rme_file_transfer::control_frame_offset == UINT32_C(0xfffffffe));
    CHECK(rme_file_transfer::abort_frame_offset == UINT32_C(0xffffffff));
    CHECK(rme_file_transfer::control_frame_offset != rme_file_transfer::abort_frame_offset);
    CHECK(rme_file_transfer::control_frame_offset > UINT32_C(0x3fffffff));
}

TEST_CASE("RME binary header recovery rejects untrusted lengths without blind discard", "[rme][file][regression]") {
    using rme_file_transfer::plausible_binary_header;
    CHECK(plausible_binary_header(12288, 1024, 12288, 4096000, 1024));
    CHECK_FALSE(plausible_binary_header(12288, 65535, 12288, 4096000, 1024));
    CHECK_FALSE(plausible_binary_header(13312, 1024, 12288, 4096000, 1024));
    CHECK(plausible_binary_header(rme_file_transfer::abort_frame_offset, 0, 12288, 4096000, 1024));
    CHECK_FALSE(plausible_binary_header(rme_file_transfer::abort_frame_offset, 1, 12288, 4096000, 1024));
    CHECK(plausible_binary_header(rme_file_transfer::control_frame_offset, 32, 12288, 4096000, 1024));
    CHECK_FALSE(plausible_binary_header(rme_file_transfer::control_frame_offset, 0, 12288, 4096000, 1024));
    CHECK_FALSE(plausible_binary_header(rme_file_transfer::control_frame_offset, 1025, 12288, 4096000, 1024));
}

TEST_CASE("RME rolling binary header recovery finds abort after a corrupt length", "[rme][file][stream]") {
    auto append_header = [](std::vector<uint8_t> &stream, const uint32_t offset, const uint16_t length, const uint32_t crc) {
        stream.push_back(offset);
        stream.push_back(offset >> 8);
        stream.push_back(offset >> 16);
        stream.push_back(offset >> 24);
        stream.push_back(length);
        stream.push_back(length >> 8);
        stream.push_back(crc);
        stream.push_back(crc >> 8);
        stream.push_back(crc >> 16);
        stream.push_back(crc >> 24);
    };
    std::vector<uint8_t> stream;
    append_header(stream, 12288, 65535, 0x12345678); // untrusted/corrupt
    stream.insert(stream.end(), { 0xaa, 0xbb, 0xcc, 0xdd, 0xee });
    append_header(stream, rme_file_transfer::abort_frame_offset, 0, 0);

    std::array<uint8_t, rme_file_transfer::binary_header_size> window {};
    uint16_t filled = 0;
    uint32_t offset = 0;
    uint16_t length = 0;
    uint32_t crc = 0;
    bool found_abort = false;
    for (const uint8_t byte : stream) {
        const auto result = rme_file_transfer::scan_binary_header_byte(
            window, filled, byte, 12288, 4096000, 1024, offset, length, crc);
        if (result == rme_file_transfer::HeaderScanResult::ready
            && offset == rme_file_transfer::abort_frame_offset && length == 0) {
            found_abort = true;
            break;
        }
    }
    CHECK(found_abort);
}

TEST_CASE("RME production header scanner covers data and control frame states", "[rme][file][stream]") {
    auto header = [](const uint32_t offset, const uint16_t length, const uint32_t crc) {
        return std::array<uint8_t, rme_file_transfer::binary_header_size> {
            uint8_t(offset),
            uint8_t(offset >> 8),
            uint8_t(offset >> 16),
            uint8_t(offset >> 24),
            uint8_t(length),
            uint8_t(length >> 8),
            uint8_t(crc),
            uint8_t(crc >> 8),
            uint8_t(crc >> 16),
            uint8_t(crc >> 24),
        };
    };
    auto scan = [](const auto &bytes, const uint32_t committed, const uint32_t expected,
                    uint32_t &offset, uint16_t &length, uint32_t &crc) {
        std::array<uint8_t, rme_file_transfer::binary_header_size> window {};
        uint16_t received = 0;
        auto result = rme_file_transfer::HeaderScanResult::incomplete;
        for (const uint8_t byte : bytes) {
            result = rme_file_transfer::scan_binary_header_byte(
                window, received, byte, committed, expected,
                rme_file_transfer::binary_payload_size, offset, length, crc);
        }
        return result;
    };

    uint32_t offset = 0;
    uint16_t length = 0;
    uint32_t crc = 0;
    const auto data = header(1536, 512, UINT32_C(0x78563412));
    CHECK(scan(data, 1536, 4096, offset, length, crc) == rme_file_transfer::HeaderScanResult::ready);
    CHECK(offset == 1536);
    CHECK(length == 512);
    CHECK(crc == UINT32_C(0x78563412));

    std::array<uint8_t, rme_file_transfer::binary_header_size> window {};
    uint16_t received = 0;
    for (size_t index = 0; index + 1 < data.size(); ++index) {
        CHECK(rme_file_transfer::scan_binary_header_byte(
                  window, received, data[index], 1536, 4096, 512,
                  offset, length, crc)
            == rme_file_transfer::HeaderScanResult::incomplete);
    }

    const auto wrong_offset = header(2048, 512, 0);
    CHECK(scan(wrong_offset, 1536, 4096, offset, length, crc) == rme_file_transfer::HeaderScanResult::invalid);
    const auto control = header(rme_file_transfer::control_frame_offset, 16, 1);
    CHECK(scan(control, 1536, 4096, offset, length, crc) == rme_file_transfer::HeaderScanResult::ready);
    const auto abort = header(rme_file_transfer::abort_frame_offset, 0, 0);
    CHECK(scan(abort, 1536, 4096, offset, length, crc) == rme_file_transfer::HeaderScanResult::ready);
}
TEST_CASE("firmware candidate cleanup waits for bootloader handoff") {
    CHECK_FALSE(firmware_update_handoff::candidate_cleanup_allowed(true));
    CHECK(firmware_update_handoff::candidate_cleanup_allowed(false));
}
