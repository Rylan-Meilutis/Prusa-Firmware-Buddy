/**
 * @file screen_menu_tools.cpp
 */

#include "screen_menu_tools.hpp"
#include "ScreenHandler.hpp"
#include "img_resources.hpp"

#include "module/motion.h"
#include "module/prusa/toolchanger.h"
#include "marlin_client.hpp"

#include <puppies/Dwarf.hpp>
#include <limits>
#include <config_store/store_instance.hpp>

/*****************************************************************************/
// MI_INFO_DWARF_BOARD_TEMPERATURE
/*****************************************************************************/
MI_INFO_DWARF_BOARD_TEMPERATURE::MI_INFO_DWARF_BOARD_TEMPERATURE()
    : MenuItemAutoUpdatingLabel(_("Dwarf Board Temp"), standard_print_format::temp_c,
        [](auto) { return sensor_data().dwarfBoardTemperature.load(); } //
    ) {}

/*****************************************************************************/
// MI_INFO_DWARF_MCU_TEMPERATURE
/*****************************************************************************/
MI_INFO_DWARF_MCU_TEMPERATURE::MI_INFO_DWARF_MCU_TEMPERATURE()
    : MenuItemAutoUpdatingLabel(_("Dwarf MCU Temp"), standard_print_format::temp_c,
        [](auto) { return sensor_data().dwarfMCUTemperature.load(); } //
    ) {}
