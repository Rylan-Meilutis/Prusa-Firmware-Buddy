/// @file
#pragma once

#include <gui/pseudo_screen_callback.hpp>

class ScreenSelftestWarning final : public PseudoScreenCallback {
public:
    ScreenSelftestWarning();

    [[nodiscard]] static bool should_show();
};
