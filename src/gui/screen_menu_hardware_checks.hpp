/// @file
#pragma once

#include <basic_screen_menu.hpp>
#include "MItem_hardware.hpp"

template <typename>
struct ScreenMenuHardwareChecks_;

template <size_t... ix>
struct ScreenMenuHardwareChecks_<std::index_sequence<ix...>> {
    using T = BasicScreenMenu<
        WithConstructorArgs<MI_HARDWARE_CHECK, static_cast<HWCheckType>(ix)>...>;
};

class ScreenMenuHardwareChecks : public ScreenMenuHardwareChecks_<std::make_index_sequence<hw_check_type_count>>::T {
public:
    ScreenMenuHardwareChecks()
        : BasicScreenMenu(_("CHECKS")) {}
};
