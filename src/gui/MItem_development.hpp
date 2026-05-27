/// @file
#pragma once

#include "WindowMenuItems.hpp"
#include <option/development_items.h>

static_assert(DEVELOPMENT_ITEMS());

class MI_TRIGGER_BANK_MIGRATION final : public IWindowMenuItem {
public:
    MI_TRIGGER_BANK_MIGRATION();

protected:
    void click(IWindowMenu &) override;
};
