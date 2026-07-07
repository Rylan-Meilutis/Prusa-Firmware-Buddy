/// @file
#pragma once

#include "WindowMenuItems.hpp"
#include <option/development_items.h>

static_assert(DEVELOPMENT_ITEMS());

class MI_DRY_RUN final : public WI_ICON_SWITCH_OFF_ON_t {
public:
    MI_DRY_RUN();

protected:
    virtual void OnChange(size_t) override;
};

class MI_TRIGGER_BANK_MIGRATION final : public IWindowMenuItem {
public:
    MI_TRIGGER_BANK_MIGRATION();

protected:
    void click(IWindowMenu &) override;
};
