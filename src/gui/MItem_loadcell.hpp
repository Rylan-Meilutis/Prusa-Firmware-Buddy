/**
 * @file MItem_loadcell.hpp
 * @brief loadcell menu items
 */
#pragma once

#include "WindowItemFormatableLabel.hpp"
#include "WindowMenuItems.hpp"
#include "i18n.h"
#include <common/sensor_data.hpp>

class MI_LOADCELL_SCALE : public WiSpin {
    constexpr static const char *const label = N_("Custom Loadcell Scale");

public:
    MI_LOADCELL_SCALE();
    void Store();
};

class MI_INFO_LOADCELL : public MenuItemAutoUpdatingLabel<float> {
public:
    MI_INFO_LOADCELL();
};
