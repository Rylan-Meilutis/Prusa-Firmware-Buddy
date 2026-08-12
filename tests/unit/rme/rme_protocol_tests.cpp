#include <rme_protocol_parser.hpp>
#include <rme_file_transfer.hpp>

#if __has_include(<catch2/catch_test_macros.hpp>)
    #include <catch2/catch_test_macros.hpp>
#else
    #include <catch2/catch.hpp>
#endif

#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>

using namespace std::string_view_literals;

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
}

TEST_CASE("RME SHA-256 parsing is exact", "[rme][file]") {
    std::array<uint8_t, 32> digest {};
    REQUIRE(rme_protocol::parse_sha256("000102030405060708090a0b0c0d0e0f101112131415161718191A1B1C1D1E1F"sv, digest));
    for (size_t i = 0; i < digest.size(); ++i) CHECK(digest[i] == i);
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
