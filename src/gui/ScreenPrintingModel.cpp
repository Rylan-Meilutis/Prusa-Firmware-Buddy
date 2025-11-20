/**
 * @file ScreenPrintingModel.cpp
 */
#include "ScreenPrintingModel.hpp"
#include "i18n.h"
#include "ScreenHandler.hpp"
#include <guiconfig/guiconfig.h>
#include "img_resources.hpp"
#include "window_icon.hpp"
namespace {
constexpr uint16_t btn_size = GuiDefaults::ButtonIconSize;
constexpr uint16_t btn_y_offset = 185;
constexpr uint16_t btn_text_spacing = 5;
constexpr uint16_t text_height = 17;

#if HAS_LARGE_DISPLAY()
inline constexpr uint16_t btn_padding = 90;
inline constexpr uint16_t btn_spacing = 30;
inline constexpr WindowMultiIconButton::Pngs icon_resources[] = {
    { img::settings_80x80, img::settings_80x80_focused, img::settings_80x80_disabled },
    { img::pause_80x80, img::pause_80x80_focused, img::pause_80x80_disabled },
    { img::stop_80x80, img::stop_80x80_focused, img::stop_80x80_disabled },
    { img::resume_80x80, img::resume_80x80_focused, img::resume_80x80_disabled },
    { img::home_80x80, img::home_80x80_focused, img::home_80x80_disabled },
    { img::reprint_80x80, img::reprint_80x80_focused, img::reprint_80x80_disabled },
    { img::disconnect_80x80, img::disconnect_80x80_focused, img::disconnect_80x80_disabled },
    { img::set_ready_80x80, img::set_ready_80x80_focused, img::set_ready_80x80_disabled },
};
#elif HAS_MINI_DISPLAY()
inline constexpr uint16_t btn_padding = 8;
inline constexpr uint16_t btn_spacing = 16;
inline constexpr WindowMultiIconButton::Pngs icon_resources[] = {
    { img::settings_64x64, img::settings_64x64_focused, img::settings_64x64_disabled },
    { img::pause_64x64, img::pause_64x64_focused, img::pause_64x64_disabled },
    { img::stop_64x64, img::stop_64x64_focused, img::stop_64x64_disabled },
    { img::resume_64x64, img::resume_64x64_focused, img::resume_64x64_disabled },
    { img::home_64x64, img::home_64x64_focused, img::home_64x64_disabled },
    { img::reprint_64x64, img::reprint_64x64_focused, img::reprint_64x64_disabled },
    { img::disconnect_64x64, img::disconnect_64x64_focused, img::disconnect_64x64_disabled },
    { img::set_ready_64x64, img::set_ready_64x64_focused, img::set_ready_64x64_disabled },
};
#else
    #error
#endif

constexpr const char *label_resources[] = {
    N_("Tune"),
    N_("Pause"),
    N_("Pausing..."),
    N_("Stop"),
    N_("Resume"),
    N_("Resuming..."),
    N_("Heating..."),
    N_("Reprint"),
    N_("Home"),
    N_("Skip"),
    N_("Disconnect"),
    N_("Set Ready"),
};
} // namespace

ScreenPrintingModel::ScreenPrintingModel(const string_view_utf8 &caption)
    : IScreenPrinting(caption)
    , buttons {
        { this, GetButtonRect(0), &icon_resources[std::to_underlying(BtnRes::Settings)], [this](window_t &) { tuneAction(); } },
        { this, GetButtonRect(1), &icon_resources[std::to_underlying(BtnRes::Pause)], [this](window_t &) { pauseAction(); } },
        { this, GetButtonRect(2), &icon_resources[std::to_underlying(BtnRes::Stop)], [this](window_t &) { stopAction(); } },
    }
    , labels {
        { this, GetButtonLabelRect(0), is_multiline::no, is_closed_on_click_t::no, _(label_resources[std::to_underlying(LabelRes::Settings)]) },
        { this, GetButtonLabelRect(1), is_multiline::no, is_closed_on_click_t::no, _(label_resources[std::to_underlying(LabelRes::Pause)]) },
        { this, GetButtonLabelRect(2), is_multiline::no, is_closed_on_click_t::no, _(label_resources[std::to_underlying(LabelRes::Stop)]) },
    } {
    for (uint8_t i = 0; i < socket_count; i++) {
        labels[i].set_font(Font::small);
        labels[i].SetPadding({ 0, 0, 0, 0 });
        labels[i].SetAlignment(Align_t::Center());
    }

    static_assert(std::size(label_resources) == std::to_underlying(LabelRes::_count), "Size mismatch");
    static_assert(std::size(icon_resources) == std::to_underlying(BtnRes::_count), "Size mismatch");
}

Rect16 ScreenPrintingModel::GetButtonRect(uint8_t idx) {
    return Rect16(btn_padding + (btn_size + btn_spacing) * idx, btn_y_offset, btn_size, btn_size);
}

Rect16 ScreenPrintingModel::GetButtonLabelRect(uint8_t idx) {
    return Rect16(btn_padding - btn_spacing / 2 + (btn_size + btn_spacing) * idx, btn_y_offset + btn_size + btn_text_spacing, btn_size + btn_spacing, text_height);
}

void ScreenPrintingModel::SetButtonIconAndLabel(BtnSocket idx, BtnRes ico_res, LabelRes txt_res) {
    SetButtonIcon(idx, ico_res);
    SetButtonLabel(idx, txt_res);
}

void ScreenPrintingModel::SetButtonIcon(BtnSocket idx, BtnRes ico_res) {
    if (idx > BtnSocket::_last || ico_res > BtnRes::_last) {
        return;
    }
    buttons[std::to_underlying(idx)].SetRes(&icon_resources[std::to_underlying(ico_res)]);
}

void ScreenPrintingModel::SetButtonLabel(BtnSocket idx, LabelRes txt_res) {
    if (idx > BtnSocket::_last || txt_res > LabelRes::_last) {
        return;
    }
    labels[std::to_underlying(idx)].SetText(_(label_resources[std::to_underlying(txt_res)]));
}

void ScreenPrintingModel::DisableButton(BtnSocket idx) {
    const size_t btn_idx = std::to_underlying(idx);
    if (idx > BtnSocket::_last || buttons[btn_idx].IsShadowed()) {
        return;
    }

    // Move focus to the right when disabled - if any button is enabled
    if (buttons[btn_idx].IsFocused()) {
        for (size_t i = 1; i < socket_count; i++) {
            if (!buttons[(btn_idx + i) % socket_count].IsShadowed()) {
                buttons[(btn_idx + i) % socket_count].SetFocus();
                break;
            }
        }
    }

    buttons[btn_idx].Shadow();
    buttons[btn_idx].Disable();
    buttons[btn_idx].Invalidate();
}

void ScreenPrintingModel::EnableButton(BtnSocket idx) {
    const size_t btn_idx = std::to_underlying(idx);
    if (idx > BtnSocket::_last || !buttons[btn_idx].IsShadowed()) {
        return;
    }

    buttons[btn_idx].Unshadow();
    buttons[btn_idx].Enable();
    buttons[btn_idx].Invalidate();
}
