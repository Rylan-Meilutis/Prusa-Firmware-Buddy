#define DO_NOT_CHECK_ATOMIC_LOCK_FREE

#include "cyphal_tool_offset_sensor_node.hpp"

#include <cyphal_pnp.hpp>
#include <hal.hpp>
#include <freertos/timing.hpp>
#include <prusa3d/common/CustomExecuteCommand_1_0.h>
#include <prusa3d/common/SharedFault_1_0.h>

namespace tool_offset_sensor::cyphal {

using namespace can::cyphal;

ToolOffsetSensorNode::ToolOffsetSensorNode(const UID &uid)
    : ToolOffsetSensorNodeBase(uid.data(),
        "cz.prusa3d.honeybee.tool_offset_sensor") {
}

void ToolOffsetSensorNode::app_init() {
    assert(registers.get_max_registers() == registers.get_register_count());
}

void ToolOffsetSensorNode::app_tick(int64_t now_us) {
    // blink status LED slowly to indicate address assigned
    hal::set_status_led((now_us / (1024 * 1024)) % 2);
}

void ToolOffsetSensorNode::app_tick_pnp(int64_t now_us) {
    // blink status LED quickly to indicate PnP discovery mode
    hal::set_status_led((now_us / (128 * 1024)) % 2);
}

void ToolOffsetSensorNode::write_config([[maybe_unused]] const ConfigTraits::Request::Type &config) {
    // TODO BFW-8360 implement config writing if needed
}

void ToolOffsetSensorNode::update_status(StatusTraits::Type &) {
    // TODO BFW-8360 implement real status update
}

} // namespace tool_offset_sensor::cyphal
