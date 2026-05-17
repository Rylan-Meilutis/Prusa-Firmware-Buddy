#include "ui_theme.hpp"

#include <config_store/store_instance.hpp>

namespace ui_theme {
namespace {
Color color_from_store(uint32_t raw) {
    return Color::from_raw(raw & 0xffffff);
}
} // namespace

Color primary() {
    return color_from_store(config_store().ui_theme_primary_color.get());
}

Color progress() {
    return color_from_store(config_store().ui_theme_progress_color.get());
}

Color warning() {
    return color_from_store(config_store().ui_theme_warning_color.get());
}

Color error() {
    return color_from_store(config_store().ui_theme_error_color.get());
}

} // namespace ui_theme
