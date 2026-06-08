/**
 * @file MItem_loadcell.cpp
 */
#include "MItem_loadcell.hpp"
#include "printer_selftest.hpp"
#include "marlin_client.hpp"
#include "ScreenHandler.hpp"
#include "loadcell.hpp"
#include "WindowMenuSpin.hpp"
#include <config_store/store_instance.hpp>

/*****************************************************************************/
// MI_LOADCELL_SCALE

static constexpr NumericInputConfig loadcell_scale_spin_config = {
    .min_value = 0.005f,
    .max_value = 0.030f,
    .step = 0.001f,
    .special_value = config_store_ns::defaults::loadcell_scale,
    .max_decimal_places = 3,
};

MI_LOADCELL_SCALE::MI_LOADCELL_SCALE()
    : WiSpin(config_store().loadcell_scale.get(), loadcell_scale_spin_config, _(label), nullptr, is_enabled_t::yes, is_hidden_t::no) {}

void MI_LOADCELL_SCALE::Store() {
    config_store().loadcell_scale.set(value());
}

/*****************************************************************************/
// MI_INFO_LOADCELL
MI_INFO_LOADCELL::MI_INFO_LOADCELL()
    : MenuItemAutoUpdatingLabel(
        _("Loadcell Value"), "%.1f",
        [](auto) { return sensor_data().loadCell.load(); } //
    ) {}
