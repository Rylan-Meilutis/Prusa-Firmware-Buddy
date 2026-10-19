#include <catch2/catch.hpp>
#include <m976_search_policy.hpp>
#include <m976_indx_pellet_policy.hpp>
#include <pa_calibration_cache.hpp>

using namespace buddy;
using Catch::Detail::Approx;

TEST_CASE("Warm PA hints require an independently repeated bracketed minimum", "[m976][fast]") {
    using m976_search::verified_minimum;
    CHECK(verified_minimum(.04f, .5f, .1f, .2f, .3f, .101f, true));
    CHECK_FALSE(verified_minimum(.04f, .5f, .1f, .2f, .3f, .15f, true));
    CHECK_FALSE(verified_minimum(.04f, .5f, .1f, .1f, .3f, .1f, true));
    CHECK_FALSE(verified_minimum(.04f, .5f, .1f, .2f, .3f, .1f, false));
    for (float invalid : { NAN, INFINITY, -INFINITY }) {
        CHECK_FALSE(verified_minimum(invalid, .5f, .1f, .2f, .3f, .1f, true));
        CHECK_FALSE(verified_minimum(.04f, .5f, invalid, .2f, .3f, .1f, true));
        CHECK_FALSE(verified_minimum(.04f, .5f, .1f, invalid, .3f, .1f, true));
        CHECK_FALSE(verified_minimum(.04f, .5f, .1f, .2f, invalid, .1f, true));
        CHECK_FALSE(verified_minimum(.04f, .5f, .1f, .2f, .3f, invalid, true));
    }
    for (float edge : { 0.f, .001f, .499f, .5f }) {
        CHECK_FALSE(verified_minimum(edge, .5f, .1f, .2f, .3f, .1f, true));
    }
}

TEST_CASE("PA signal failure is bounded and transient weak captures recover", "[m976][fast]") {
    m976_search::SignalGuard signal;
    for (int i = 0; i < 2; ++i) {
        signal.observe(900, false, 1, 1, 6);
        CHECK(signal.failure == nullptr);
    }
    signal.observe(900, false, 10, 1, 6);
    CHECK(signal.weak == 0);
    for (int i = 0; i < 3; ++i) {
        signal.observe(900, false, 1, 1, 6);
    }
    CHECK(std::string_view(signal.failure) == "repeated_low_signal");
    signal = {};
    signal.observe(0, false, 10, 1, 6);
    CHECK(std::string_view(signal.failure) == "no_capture_data");
    signal = {};
    signal.observe(900, true, 10, 1, 6);
    CHECK(std::string_view(signal.failure) == "capture_overflow");
}

TEST_CASE("PA temperature seed never becomes an extrapolated reusable value", "[m976][fast]") {
    pa_temperature::Model model;
    CHECK_FALSE(std::isfinite(model.search_seed(240)));
    REQUIRE(model.add({ 240, .05f, 12, .9f }));
    CHECK(model.search_seed(250) == Approx(.05f));
    CHECK_FALSE(model.evaluate(250).valid);
    CHECK_FALSE(std::isfinite(model.search_seed(256)));
    REQUIRE(model.add({ 250, .04f, 12, .9f }));
    CHECK(model.search_seed(245) == Approx(.045f));
    CHECK(model.search_seed(260) == Approx(.03f));
    CHECK_FALSE(model.evaluate(260).valid);
    CHECK_FALSE(std::isfinite(model.search_seed(169)));
    CHECK_FALSE(std::isfinite(model.search_seed(301)));
}

TEST_CASE("PA cache seed preserves every identity and quality gate", "[m976][cache][fast]") {
    pa_cache::Record r;
    r.version = pa_cache::record_version;
    r.key.temperature = 240;
    r.key.profile_temperature = 215;
    r.key.confidence_floor = 75;
    r.key.minimum_snr = 6;
    r.key.nozzle = .4f;
    r.pa = .046f;
    r.max_flow = 12;
    r.confidence = .81f;
    pa_cache::Bank bank;
    REQUIRE(bank.insert(r));
    auto request = r.key;
    request.temperature = 250;
    CHECK(bank.search_seed(request) == Approx(.046f));
    const char *reason = nullptr;
    CHECK(bank.find(request, reason) == nullptr); // Hint is NOT a cache hit.
    CHECK(std::string_view(reason) == "calibration_temperature_changed");
    for (int field = 0; field < 11; ++field) {
        auto changed = request;
        switch (field) {
        case 0:
            changed.profile[0] = 'X';
            break;
        case 1:
            changed.material[0] = 'X';
            break;
        case 2:
            ++changed.color;
            break;
        case 3:
            ++changed.color_valid;
            break;
        case 4:
            ++changed.manufacturer;
            break;
        case 5:
            ++changed.physical_tool;
            break;
        case 6:
            changed.nozzle = .6f;
            break;
        case 7:
            ++changed.profile_temperature;
            break;
        case 8:
            ++changed.confidence_floor;
            break;
        case 9:
            ++changed.minimum_snr;
            break;
        case 10:
            changed.flexible = true;
            break;
        }
        CHECK_FALSE(std::isfinite(bank.search_seed(changed)));
    }
    bank.records[0].confidence = .5f;
    CHECK_FALSE(std::isfinite(bank.search_seed(request)));
    bank.records[0] = r;
    bank.records[0].version = 0;
    CHECK_FALSE(std::isfinite(bank.search_seed(request)));
}

TEST_CASE("PA pellet budget never increases the established cycle or volume limit", "[m976][fast][pellet]") {
    using namespace m976_indx_pellet_policy;
    for (float cycle_mm : { .35f, 2.4f, 3.f, 12.f }) {
        Budget budget;
        for (int cycle = 0; cycle < 1000; ++cycle) {
            if (budget.would_overfill(cycle_mm)) {
                budget = {}; // wipe, cool, eject
            }
            REQUIRE_FALSE(budget.would_overfill(cycle_mm));
            const bool eject = budget.record(cycle_mm);
            CHECK(budget.cycles <= 5);
            CHECK(budget.filament_mm <= maximum_pending_filament_mm + .0001f);
            if (eject) {
                budget = {};
            }
        }
    }
    Budget budget;
    CHECK(budget.would_overfill(NAN));
    CHECK(budget.would_overfill(INFINITY));
    CHECK(budget.would_overfill(-1));
    CHECK(budget.would_overfill(12.1f));
    CHECK(cooling_delay_for("HIPS", 250) == 15000);
}
