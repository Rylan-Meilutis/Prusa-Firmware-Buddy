#include <catch2/catch_test_macros.hpp>
#include <filters/debouncer.hpp>

TEST_CASE("Debouncer: initial state is not stable", "[debouncer]") {
    Debouncer<int> d(42, 3);
    CHECK(d.value() == 42);
    CHECK_FALSE(d.is_stable());
}

TEST_CASE("Debouncer: single push doesn't change value", "[debouncer]") {
    Debouncer<int> d(0, 3);
    d.push(1);
    CHECK(d.value() == 0);
    CHECK_FALSE(d.is_stable());
}

TEST_CASE("Debouncer: stabilizes after required_stability pushes", "[debouncer]") {
    Debouncer<int> d(0, 3);
    d.push(1);
    CHECK(d.value() == 0);
    CHECK_FALSE(d.is_stable());
    d.push(1);
    CHECK(d.value() == 0);
    CHECK_FALSE(d.is_stable());
    d.push(1);
    CHECK(d.value() == 1);
    CHECK(d.is_stable());
}

TEST_CASE("Debouncer: interrupted sequence resets counter", "[debouncer]") {
    Debouncer<int> d(0, 3);
    d.push(1);
    d.push(1);
    // Interrupt before stabilizing
    d.push(2);
    CHECK_FALSE(d.is_stable());
    d.push(1);
    d.push(1);
    CHECK(d.value() == 0);
    CHECK_FALSE(d.is_stable());
    // Now finish the sequence for 1
    d.push(1);
    CHECK(d.value() == 1);
    CHECK(d.is_stable());
}

TEST_CASE("Debouncer: stable value persists through noise", "[debouncer]") {
    Debouncer<int> d(0, 3);
    // Stabilize on 5
    d.push(5);
    d.push(5);
    d.push(5);
    CHECK(d.value() == 5);

    CHECK(d.is_stable());

    // Noise that never reaches stability
    d.push(9);
    CHECK_FALSE(d.is_stable());
    d.push(7);
    CHECK_FALSE(d.is_stable());
    d.push(9);
    CHECK_FALSE(d.is_stable());
    d.push(8);
    CHECK(d.value() == 5);
    CHECK_FALSE(d.is_stable());
}

TEST_CASE("Debouncer: required_stability = 1 stabilizes immediately", "[debouncer]") {
    Debouncer<int> d(0, 1);
    d.push(42);
    CHECK(d.value() == 42);
    CHECK(d.is_stable());
    d.push(7);
    CHECK(d.value() == 7);
    CHECK(d.is_stable());
}

TEST_CASE("Debouncer: multiple transitions", "[debouncer]") {
    Debouncer<int> d(0, 2);
    d.push(1);
    d.push(1);
    CHECK(d.value() == 1);
    d.push(2);
    d.push(2);
    CHECK(d.value() == 2);
    d.push(3);
    d.push(3);
    CHECK(d.value() == 3);
}

TEST_CASE("Debouncer: extra pushes after stabilization are harmless", "[debouncer]") {
    Debouncer<int> d(0, 2);
    for (int i = 0; i < 300; i++) {
        d.push(1);
    }
    CHECK(d.value() == 1);
    CHECK(d.is_stable());
}

TEST_CASE("Debouncer: works with bool", "[debouncer]") {
    Debouncer<bool> d(false, 3);
    d.push(true);
    d.push(true);
    CHECK(d.value() == false);
    d.push(true);
    CHECK(d.value() == true);
}

TEST_CASE("Debouncer: works with enum", "[debouncer]") {
    enum class State { off,
        on,
        error };
    Debouncer<State> d(State::off, 2);
    d.push(State::on);
    CHECK(d.value() == State::off);
    d.push(State::on);
    CHECK(d.value() == State::on);
    d.push(State::error);
    d.push(State::error);
    CHECK(d.value() == State::error);
}

TEST_CASE("Debouncer: uint16_t counter for large intervals", "[debouncer]") {
    Debouncer<int, uint16_t> d(0, 500);
    for (int i = 0; i < 499; i++) {
        d.push(1);
    }
    CHECK(d.value() == 0);
    d.push(1);
    CHECK(d.value() == 1);
}
