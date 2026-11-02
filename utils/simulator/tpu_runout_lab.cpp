// Characterization lab, not a hardware emulator or a corrected detector.
// Links the production pressure monitor; only load and executed E are modeled.
#include <feature/extrusion_calibration.hpp>
#include <cmath>
#include <cstdio>

using namespace buddy::extrusion_calibration;

int main() {
    constexpr int count =
#ifdef LAB_PURGE_CONTEXT
        7;
#else
        5;
#endif
    for (int scenario = 0; scenario < count; ++scenario) {
        reset_job_results();
        Score reference { .noise = .2f, .low_load = 5, .high_load = 30, .valid = true };
        set_job_result(4, { .08f, 4.f, .9f, true, reference, true });
        select_job_result(4);
        set_pressure_monitor_detection(true, true);
        uint32_t time = 1000;
        float e = 0;
        const char *names[] = { "normal_pressure_buildup", "true_missing_pressure", "primed_at_enable", "pressure_retained_across_travel", "travel_with_relaxed_pressure", "pre_print_bin_purge", "true_runout_after_bin_exit" };
#ifdef LAB_PURGE_CONTEXT
        if (scenario >= 5) {
            set_pressure_monitor_purge_context(true);
        }
#endif
        record_loadcell_sample(time, scenario == 2 ? 15.f : 0.f, e);
        ExtrusionFault fault = ExtrusionFault::none;
        float fault_seconds = 0;
        // 200 Hz loadcell samples and quantized 100 Hz executed E steps,
        // averaging 0.5 mm/s. Constant pressure is synthetic healthy flow;
        // it is not a measurement of this user's particular nozzle/material.
        for (unsigned i = 1; i <= 3600; ++i) {
            e = std::floor(i / 2.f) * .005f;
            record_loadcell_sample(time += 5000, scenario == 1 || scenario >= 5 ? 0.f : 15.f, e);
            fault = consume_extrusion_fault();
            if (fault != ExtrusionFault::none) {
                fault_seconds = i * .005f;
                break;
            }
        }
#ifdef LAB_PURGE_CONTEXT
        if (scenario >= 5) {
            set_pressure_monitor_purge_context(false);
            const float base_e = e;
            for (unsigned i = 1; i <= 3600; ++i) {
                e = base_e + std::floor(i / 2.f) * .005f;
                record_loadcell_sample(time += 5000, scenario == 6 ? 0.f : 15.f, e);
                fault = consume_extrusion_fault();
                if (fault != ExtrusionFault::none) {
                    fault_seconds = i * .005f;
                    break;
                }
            }
        }
#endif
        if ((scenario == 3 || scenario == 4) && fault == ExtrusionFault::none) {
            for (unsigned i = 0; i < 200; ++i) {
                record_loadcell_sample(time += 5000, scenario == 3 ? 15.f : 0.f, e);
            }
            const float base_e = e;
            for (unsigned i = 1; i <= 3600; ++i) {
                e = base_e + std::floor(i / 2.f) * .005f;
                record_loadcell_sample(time += 5000, 15.f, e);
                fault = consume_extrusion_fault();
                if (fault != ExtrusionFault::none) {
                    fault_seconds = i * .005f;
                    break;
                }
            }
        }
        const char *fault_name = fault == ExtrusionFault::no_pressure_rise ? "no_pressure_rise" : fault == ExtrusionFault::none ? "none"
                                                                                                                                : "other";
        std::printf("{\"scenario\":\"%s\",\"fault\":\"%s\",\"fault_after_segment_seconds\":%.3f,\"executed_e_mm\":%.3f}\n",
            names[scenario], fault_name, fault_seconds, e);
    }
}
