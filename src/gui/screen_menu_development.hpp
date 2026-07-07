/// @file
#pragma once

#include "MItem_development.hpp"
#include <basic_screen_menu.hpp>
#include <option/development_items.h>

static_assert(DEVELOPMENT_ITEMS());

using ScreenMenuDevelopmentBase = BasicScreenMenu<
    MI_DRY_RUN,
    MI_TRIGGER_BANK_MIGRATION>;

class ScreenMenuDevelopment final : public ScreenMenuDevelopmentBase {
public:
    ScreenMenuDevelopment();
};
