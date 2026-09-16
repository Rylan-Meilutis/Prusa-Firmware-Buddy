#pragma once

#include <WindowMenuItems.hpp>
#include <filament.hpp>
#include <filament_color.hpp>
#include <filament_color_gui.hpp>
#include <config_store/store_instance.hpp>
#include <utils/string_builder.hpp>

// Compact loaded-material presentation, never the saved profile identifier.
class LoadedToolInfo : public WiInfo<64> {
public:
    LoadedToolInfo()
        : WiInfo<64>(string_view_utf8 {}) {}

    void set_loaded_tool(VirtualToolIndex tool) {
        StringBuilder sb(value_array_);
        sb.append_string(filament_material_name(config_store().get_filament_type(tool).parameters()).data());
        value_ = string_view_utf8::MakeRAM(value_array_.data());
        color_ = filament_color::loaded(tool.to_raw());
        update_extension_width();
        if (color_) {
            extension_width += filament_color_gui::swatch_extension_width;
        }
    }

protected:
    void printExtension(Rect16 rect, Color text, Color background, ropfn op) const override {
        if (!color_) {
            WiInfo<64>::printExtension(rect, text, background, op);
            return;
        }
        Rect16 swatch = rect;
        swatch += Rect16::Left_t { static_cast<int16_t>(rect.Width() - filament_color_gui::swatch_extension_width) };
        swatch = filament_color_gui::swatch_extension_width;
        rect -= filament_color_gui::swatch_extension_width;
        WiInfo<64>::printExtension(rect, text, background, op);
        filament_color_gui::draw_swatch(swatch, *color_, background);
    }

private:
    std::optional<Color> color_;
};
