#include <screen/screen_menu_external_light_bar.hpp>

#if HAS_I2C_EXPANDER() && BOARD_IS_XBUDDY()

MI_EXTERNAL_LIGHT_BAR_WIRING_1::MI_EXTERNAL_LIGHT_BAR_WIRING_1()
    : WI_INFO_t(_(label), nullptr, is_enabled_t::yes, is_hidden_t::no) {
    ChangeInformation(_("GPIO breakout"));
}

MI_EXTERNAL_LIGHT_BAR_WIRING_2::MI_EXTERNAL_LIGHT_BAR_WIRING_2()
    : WI_INFO_t(_(label), nullptr, is_enabled_t::yes, is_hidden_t::no) {
    ChangeInformation(_("Use external 24V/5V"));
}

MI_EXTERNAL_LIGHT_BAR_WIRING_3::MI_EXTERNAL_LIGHT_BAR_WIRING_3()
    : WI_INFO_t(_(label), nullptr, is_enabled_t::yes, is_hidden_t::no) {
    ChangeInformation(_("0-3: sink only"));
}

MI_EXTERNAL_LIGHT_BAR_WIRING_4::MI_EXTERNAL_LIGHT_BAR_WIRING_4()
    : WI_INFO_t(_(label), nullptr, is_enabled_t::yes, is_hidden_t::no) {
    ChangeInformation(_("4-7: logic/pull down"));
}

MI_EXTERNAL_LIGHT_BAR_WIRING_5::MI_EXTERNAL_LIGHT_BAR_WIRING_5()
    : WI_INFO_t(_(label), nullptr, is_enabled_t::yes, is_hidden_t::no) {
    ChangeInformation(_("Use driver, not LEDs"));
}

ScreenMenuExternalLightBar::ScreenMenuExternalLightBar()
    : ScreenMenuExternalLightBar__(_(label)) {
}

#endif
