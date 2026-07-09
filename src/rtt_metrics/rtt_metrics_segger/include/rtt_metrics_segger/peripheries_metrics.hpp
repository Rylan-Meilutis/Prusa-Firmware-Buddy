/// @file
#pragma once

#include <accelerometer/common_structs.hpp>

namespace rtt_metrics {
void log_accelerometer(const accelerometer::RawAcceleration &raw_acceleration);
} // namespace rtt_metrics
