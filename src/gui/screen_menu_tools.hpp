/**
 * @file screen_menu_tools.hpp
 * @brief Helpers for tool-specific menu items.
 */

#pragma once

#include "MItem_tools.hpp"

template <class OdometerT, int N>
class MI_ODOMETER_N : public OdometerT {
    static_assert(N >= 0 && N <= 4, "bad input");

    static consteval const char *get_name() {
        switch (N) {
        case 0:
            return N_("  Tool 1");
        case 1:
            return N_("  Tool 2");
        case 2:
            return N_("  Tool 3");
        case 3:
            return N_("  Tool 4");
        case 4:
            return N_("  Tool 5");
        }
        consteval_assert_false();
        return "";
    }

    static constexpr const char *const specific_label = get_name();

public:
    MI_ODOMETER_N()
        : OdometerT(specific_label, N) {
    }
};

template <int N>
using MI_ODOMETER_DIST_E_N = MI_ODOMETER_N<MI_ODOMETER_DIST_E, N>;

template <int N>
using MI_ODOMETER_TOOL_N = MI_ODOMETER_N<MI_ODOMETER_TOOL, N>;
