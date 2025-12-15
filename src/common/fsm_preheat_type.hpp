/**
 * @file fsm_preheat_type.hpp
 */

#pragma once

#include "client_fsm_types.h"
#include <common/fsm_base_types.hpp>
#include <tool_index.hpp>

#include <utility>

/**
 * @brief object to pass preheat data between threads
 */
struct PreheatData {
    using ToolIndex = std::variant<VirtualToolIndex, AllTools>;

    ToolIndex tool;

    PreheatMode mode : 3;
    static_assert(std::to_underlying(PreheatMode::_last) < (1 << 3));

    bool has_return_option : 1;
    bool has_cooldown_option : 1;

    static constexpr PreheatData make(PreheatMode mode, ToolIndex tool, RetAndCool_t ret_cool = RetAndCool_t::Neither) {
        return PreheatData {
            .tool = tool,
            .mode = mode,
            .has_return_option = bool(std::to_underlying(ret_cool) & std::to_underlying(RetAndCool_t::Return)),
            .has_cooldown_option = bool(std::to_underlying(ret_cool) & std::to_underlying(RetAndCool_t::Cooldown)),
        };
    }

    static constexpr PreheatData deserialize(fsm::PhaseData data) {
        return fsm::deserialize_data<PreheatData>(data);
    }

    constexpr fsm::PhaseData serialize() const {
        return fsm::serialize_data(*this);
    }
};
