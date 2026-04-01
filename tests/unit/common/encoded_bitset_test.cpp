#include <catch2/catch_test_macros.hpp>
#include <utils/storage/encoded_bitset.hpp>
#include <cstring>

TEST_CASE("EncodedBitset: default construction", "[encoded_bitset]") {
    constexpr EncodedBitset<16> bs;
    STATIC_REQUIRE(bs.data() == 0);
    STATIC_REQUIRE(!bs.test(0));
    STATIC_REQUIRE(!bs[0]);
}

TEST_CASE("EncodedBitset: integer construction", "[encoded_bitset]") {
    constexpr EncodedBitset<16> bs(0x8001);
    STATIC_REQUIRE(bs.test(0));
    STATIC_REQUIRE(!bs.test(1));
    STATIC_REQUIRE(bs.test(15));
    STATIC_REQUIRE(bs.data() == 0x8001);
}

TEST_CASE("EncodedBitset: set and test individual bits", "[encoded_bitset]") {
    EncodedBitset<32> bs;
    REQUIRE(!bs.test(5));

    bs.set(5);
    REQUIRE(bs.test(5));
    REQUIRE(bs.data() == (1u << 5));

    bs.set(0);
    REQUIRE(bs.test(0));
    REQUIRE(bs.test(5));

    bs.set(31);
    REQUIRE(bs.test(31));
}

TEST_CASE("EncodedBitset: set to false and reset", "[encoded_bitset]") {
    EncodedBitset<16> bs(0xFFFF);

    bs.set(3, false);
    REQUIRE(!bs.test(3));
    REQUIRE(bs.test(2));
    REQUIRE(bs.test(4));

    bs.reset(0);
    REQUIRE(!bs.test(0));
}

TEST_CASE("EncodedBitset: operator[]", "[encoded_bitset]") {
    constexpr EncodedBitset<16> bs(0b1010);
    STATIC_REQUIRE(!bs[0]);
    STATIC_REQUIRE(bs[1]);
    STATIC_REQUIRE(!bs[2]);
    STATIC_REQUIRE(bs[3]);
}

TEST_CASE("EncodedBitset: equality", "[encoded_bitset]") {
    EncodedBitset<16> a(0x1234);
    EncodedBitset<16> b(0x1234);
    EncodedBitset<16> c(0x5678);

    REQUIRE(a == b);
    REQUIRE(a != c);

    // Default-constructed are equal
    REQUIRE(EncodedBitset<32>() == EncodedBitset<32>());
}

TEST_CASE("EncodedBitset: constexpr operations", "[encoded_bitset]") {
    // Verify everything works at compile time
    constexpr auto bs = [] {
        EncodedBitset<32> b;
        b.set(0);
        b.set(7);
        b.set(31);
        return b;
    }();

    STATIC_REQUIRE(bs.test(0));
    STATIC_REQUIRE(bs.test(7));
    STATIC_REQUIRE(bs.test(31));
    STATIC_REQUIRE(!bs.test(1));
    STATIC_REQUIRE(bs.data() == ((1u << 0) | (1u << 7) | (1u << 31)));
}

// These tests verify binary compatibility with the ARM EEPROM representation.
// Both x86_64 (test host) and ARM (target) are little-endian, so memcpy of
// these byte patterns into uint32_t produces the same result on both platforms.
// The byte patterns match what ARM GCC's std::bitset<N> writes to EEPROM.

TEST_CASE("EncodedBitset: memcpy from hardcoded bytes - bits 0 and 2", "[encoded_bitset]") {
    const uint8_t bytes[] = { 0x05, 0x00, 0x00, 0x00 };
    EncodedBitset<16> bs;
    std::memcpy(&bs, bytes, sizeof(bs));

    REQUIRE(bs.test(0));
    REQUIRE(!bs.test(1));
    REQUIRE(bs.test(2));
    REQUIRE(!bs.test(3));
    REQUIRE(bs.data() == 0x05);
}

TEST_CASE("EncodedBitset: memcpy from hardcoded bytes - bit 15", "[encoded_bitset]") {
    const uint8_t bytes[] = { 0x00, 0x80, 0x00, 0x00 };
    EncodedBitset<16> bs;
    std::memcpy(&bs, bytes, sizeof(bs));

    REQUIRE(!bs.test(0));
    REQUIRE(bs.test(15));
    REQUIRE(bs.data() == 0x8000);
}

TEST_CASE("EncodedBitset: memcpy from hardcoded bytes - all bits set", "[encoded_bitset]") {
    const uint8_t bytes[] = { 0xFF, 0xFF, 0xFF, 0xFF };
    EncodedBitset<32> bs;
    std::memcpy(&bs, bytes, sizeof(bs));

    for (size_t i = 0; i < 32; i++) {
        REQUIRE(bs.test(i));
    }
    REQUIRE(bs.data() == 0xFFFFFFFF);
}

TEST_CASE("EncodedBitset: memcpy from hardcoded bytes - all zeros", "[encoded_bitset]") {
    const uint8_t bytes[] = { 0x00, 0x00, 0x00, 0x00 };
    EncodedBitset<16> bs(0xFFFF); // Start non-zero to verify overwrite
    std::memcpy(&bs, bytes, sizeof(bs));

    for (size_t i = 0; i < 16; i++) {
        REQUIRE(!bs.test(i));
    }
    REQUIRE(bs.data() == 0);
}

TEST_CASE("EncodedBitset: memcpy from hardcoded bytes - mixed pattern", "[encoded_bitset]") {
    // 0xA5 = 10100101, 0x01 = 00000001 → bits: 0,2,5,7,8
    const uint8_t bytes[] = { 0xA5, 0x01, 0x00, 0x00 };
    EncodedBitset<16> bs;
    std::memcpy(&bs, bytes, sizeof(bs));

    REQUIRE(bs.test(0));
    REQUIRE(!bs.test(1));
    REQUIRE(bs.test(2));
    REQUIRE(!bs.test(3));
    REQUIRE(!bs.test(4));
    REQUIRE(bs.test(5));
    REQUIRE(!bs.test(6));
    REQUIRE(bs.test(7));
    REQUIRE(bs.test(8));
    REQUIRE(!bs.test(9));
    REQUIRE(bs.data() == 0x01A5);
}

TEST_CASE("EncodedBitset: memcpy roundtrip", "[encoded_bitset]") {
    EncodedBitset<16> original;
    original.set(0);
    original.set(3);
    original.set(15);

    uint8_t bytes[4];
    std::memcpy(bytes, &original, sizeof(bytes));

    EncodedBitset<16> restored;
    std::memcpy(&restored, bytes, sizeof(restored));

    REQUIRE(original == restored);
    REQUIRE(restored.test(0));
    REQUIRE(restored.test(3));
    REQUIRE(restored.test(15));
    REQUIRE(!restored.test(1));
}

TEST_CASE("EncodedBitset: constructor masks out-of-range bits", "[encoded_bitset]") {
    // N=8: only bits 0-7 should be kept
    constexpr EncodedBitset<8> bs(0xFFFFFFFF);
    STATIC_REQUIRE(bs.data() == 0xFF);

    // N=1: only bit 0
    constexpr EncodedBitset<1> bs1(0xFFFFFFFF);
    STATIC_REQUIRE(bs1.data() == 1);

    // N=32: all bits kept
    constexpr EncodedBitset<32> bs32(0xFFFFFFFF);
    STATIC_REQUIRE(bs32.data() == 0xFFFFFFFF);

    // Out-of-range bits don't affect equality
    REQUIRE(EncodedBitset<8>(0xFF) == EncodedBitset<8>(0x1FF));
}

TEST_CASE("EncodedBitset: operator~", "[encoded_bitset]") {
    // Negation of all-zero is all-ones (within N bits)
    constexpr EncodedBitset<8> zero;
    constexpr auto neg_zero = ~zero;
    STATIC_REQUIRE(neg_zero.data() == 0xFF);

    // Negation of all-ones is zero
    constexpr EncodedBitset<8> ones(0xFF);
    constexpr auto neg_ones = ~ones;
    STATIC_REQUIRE(neg_ones.data() == 0);

    // Negation only affects bits within N
    constexpr EncodedBitset<4> small(0b0101);
    constexpr auto neg_small = ~small;
    STATIC_REQUIRE(neg_small.data() == 0b1010);

    // Double negation is identity
    constexpr EncodedBitset<16> val(0x1234);
    STATIC_REQUIRE(~~val == val);
}

TEST_CASE("EncodedBitset: all() factory", "[encoded_bitset]") {
    // N=32: all bits set
    constexpr auto all32 = EncodedBitset<32>::all();
    STATIC_REQUIRE(all32.data() == 0xFFFFFFFF);

    // N=16: only lower 16 bits
    constexpr auto all16 = EncodedBitset<16>::all();
    STATIC_REQUIRE(all16.data() == 0xFFFF);

    // N=8
    constexpr auto all8 = EncodedBitset<8>::all();
    STATIC_REQUIRE(all8.data() == 0xFF);

    // N=1: single bit
    constexpr auto all1 = EncodedBitset<1>::all();
    STATIC_REQUIRE(all1.data() == 1);

    // all() is equivalent to ~EncodedBitset()
    STATIC_REQUIRE(EncodedBitset<16>::all() == ~EncodedBitset<16>());
}
