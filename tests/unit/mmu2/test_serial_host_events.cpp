#include <mmu2/mmu2_reporting.hpp>

#include <catch2/catch.hpp>
#include <cstdint>

namespace {
uint8_t event_bits(MMU2::SerialHostMmuEvent event) {
    return static_cast<uint8_t>(event);
}
} // namespace

TEST_CASE("MMU serial host events are deferred until consumed") {
    // Keep the process-global event latch clean even if another test failed earlier.
    MMU2::consume_serial_host_mmu_events();

    REQUIRE(MMU2::consume_serial_host_mmu_events() == MMU2::SerialHostMmuEvent::none);

    MMU2::defer_serial_host_mmu_paused();
    REQUIRE(event_bits(MMU2::consume_serial_host_mmu_events()) == event_bits(MMU2::SerialHostMmuEvent::paused));
    REQUIRE(MMU2::consume_serial_host_mmu_events() == MMU2::SerialHostMmuEvent::none);

    MMU2::defer_serial_host_mmu_resume();
    REQUIRE(event_bits(MMU2::consume_serial_host_mmu_events()) == event_bits(MMU2::SerialHostMmuEvent::resume));
    REQUIRE(MMU2::consume_serial_host_mmu_events() == MMU2::SerialHostMmuEvent::none);
}

TEST_CASE("MMU serial host event deferral coalesces repeated reports") {
    MMU2::consume_serial_host_mmu_events();

    MMU2::defer_serial_host_mmu_paused();
    MMU2::defer_serial_host_mmu_paused();
    MMU2::defer_serial_host_mmu_resume();

    const uint8_t events = event_bits(MMU2::consume_serial_host_mmu_events());
    REQUIRE((events & event_bits(MMU2::SerialHostMmuEvent::paused)) != 0);
    REQUIRE((events & event_bits(MMU2::SerialHostMmuEvent::resume)) != 0);
    REQUIRE(MMU2::consume_serial_host_mmu_events() == MMU2::SerialHostMmuEvent::none);
}
