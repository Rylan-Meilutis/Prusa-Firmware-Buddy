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
    // Register data senders
    data_ch0_sender.add_to_task();
    data_ch1_sender.add_to_task();
    port_list.add(data_ch0_sender);
    port_list.add(data_ch1_sender);
    registers.add_port_name_set("pub.data_ch0",
        prusa3d_tool_offset_sensor_Data_1_0_Traits::full_name_and_version,
        data_ch0_sender.get_port_id());
    registers.add_port_name_set("pub.data_ch1",
        prusa3d_tool_offset_sensor_Data_1_0_Traits::full_name_and_version,
        data_ch1_sender.get_port_id());

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

void ToolOffsetSensorNode::write_config(const ConfigTraits::Request::Type &cfg) {
    config_mutex.lock();
    config.ch0_enabled = cfg.ch0_enabled;
    config.ch1_enabled = cfg.ch1_enabled;
    config_mutex.unlock();
}

ToolOffsetSensorNode::ChannelConfig ToolOffsetSensorNode::get_config() {
    config_mutex.lock();
    auto snapshot = config;
    config_mutex.unlock();
    return snapshot;
}

void ToolOffsetSensorNode::update_status(StatusTraits::Type &) {
    // TODO BFW-8360 implement real status update
}

void ToolOffsetSensorNode::publish_data_ch0(const prusa3d_tool_offset_sensor_Data_1_0 &data) {
    data_ch0_sender.set_data(data);
}

void ToolOffsetSensorNode::publish_data_ch1(const prusa3d_tool_offset_sensor_Data_1_0 &data) {
    data_ch1_sender.set_data(data);
}

} // namespace tool_offset_sensor::cyphal
