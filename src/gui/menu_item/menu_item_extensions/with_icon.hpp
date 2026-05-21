#pragma once

#include <type_traits>

#include <i_window_menu_item.hpp>
#include <img_resources.hpp>

/// Mixin that sets a left icon on an IWindowMenuItem-derived Base.
/// The `icon` pointer must point to an object with static storage duration and external linkage
/// (e.g. `inline constexpr img::Resource my_icon`).
template <typename Base, const img::Resource *icon>
class WithIcon : public Base {

public:
    static_assert(
        std::is_base_of_v<IWindowMenuItem, Base>, "WithIcon requires Base to derive from IWindowMenuItem");

    template <typename... Args>
    WithIcon(Args &&...args)
        : Base(std::forward<Args>(args)...) {
        Base::SetIconId(icon);
    }
};
