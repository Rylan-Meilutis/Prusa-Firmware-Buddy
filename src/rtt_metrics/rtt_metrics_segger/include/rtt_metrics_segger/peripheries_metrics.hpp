/// @file
#pragma once

#include <accelerometer/common_structs.hpp>
#include <rtt_metrics_structs/rtt_metrics_structs.hpp>

namespace rtt_metrics {
void log_accelerometer(const accelerometer::RawAcceleration &raw_acceleration);
void log_loadcell_tared_z(const LoadcellTaredZ &tared_z);
} // namespace rtt_metrics
