#include "screen_splash.hpp"
#include "ScreenHandler.hpp"

#include <buddy/bootstrap_state.hpp>
#include "config.h"
#include "config_features.h"
#include <version/version.hpp>
#include "img_resources.hpp"
#include "marlin_client.hpp"
#include <config_store/store_instance.hpp>

#include "i18n.h"
#include "../lang/translator.hpp"
#include "language_eeprom.hpp"
#include "screen_menu_languages.hpp"
#include <pseudo_screen_callback.hpp>
#include <bsod.h>
#include <guiconfig/guiconfig.h>
#include <gui/screen/screen_printer_type_changed.hpp>
#include <window_msgbox_happy_printing.hpp>

#include <option/bootloader.h>
#include <option/developer_mode.h>
#include <option/has_power_panic.h>
#include <option/has_translations.h>
#include <gui/screen_initial_network_setup.hpp>
#include <gui/screen_printer_setup.hpp>
#include <gui/screen_welcome.hpp>
#include <option/has_emergency_stop.h>
#include <option/has_heatbed_screws_during_transport.h>
#include <option/has_ht_hotend.h>
#if HAS_HT_HOTEND()
    #include <hotend_detect.hpp>
    #include <sys.hpp>
#endif
#include <option/has_indx_head.h>
#include <option/has_indx.h>

#include <option/has_selftest.h>
#if HAS_SELFTEST()
    #include "printer_selftest.hpp"
    #include "screen_menu_selftest_snake.hpp"
#endif // HAS_SELFTEST

#include <option/has_touch.h>
#if HAS_TOUCH()
    #include <gui/screen_touch_driver_failed.hpp>
#endif

#if HAS_POWER_PANIC()
    #include "power_panic.hpp"
#endif

#include <option/has_toolchanger.h>
#if HAS_TOOLCHANGER()
    #include <module/prusa/toolchanger.h>
#endif

#include "display.hpp"
#include <option/has_switched_fan_test.h>

#if HAS_MINI_DISPLAY()
    #define SPLASHSCREEN_PROGRESSBAR_X 16
    #define SPLASHSCREEN_PROGRESSBAR_Y 148
    #define SPLASHSCREEN_PROGRESSBAR_W 206
    #define SPLASHSCREEN_PROGRESSBAR_H 12
    #define SPLASHSCREEN_VERSION_Y     165

#elif HAS_LARGE_DISPLAY()
    #define SPLASHSCREEN_PROGRESSBAR_X 100
    #define SPLASHSCREEN_PROGRESSBAR_Y 165
    #define SPLASHSCREEN_PROGRESSBAR_W 280
    #define SPLASHSCREEN_PROGRESSBAR_H 12
    #define SPLASHSCREEN_VERSION_Y     185
#endif

using buddy::BootstrapStage;

ScreenSplash::ScreenSplash()
    : screen_t()
    , text_progress(this, Rect16(0, SPLASHSCREEN_VERSION_Y, GuiDefaults::ScreenWidth, 18), is_multiline::no)
    , progress(this, Rect16(SPLASHSCREEN_PROGRESSBAR_X, SPLASHSCREEN_PROGRESSBAR_Y, SPLASHSCREEN_PROGRESSBAR_W, SPLASHSCREEN_PROGRESSBAR_H), COLOR_BRAND, COLOR_GRAY, 6) {
    ClrMenuTimeoutClose();

    text_progress.set_font(Font::small);
    text_progress.SetAlignment(Align_t::Center());
    text_progress.SetTextColor(COLOR_GRAY);

    snprintf(text_progress_buffer, sizeof(text_progress_buffer), "Firmware %s", version::project_version_full);
    text_progress.SetText(string_view_utf8::MakeRAM(text_progress_buffer));
    progress.set_progress_percent(50);

#if HAS_POWER_PANIC()
    // don't present any screen or wizard if there is a powerpanic pending
    if (power_panic::state_stored()) {
        return;
    }
#endif

#if DEVELOPER_MODE()
    // #error dead code found by automatic analyses (see BFW-5461)
    // don't present any screen or wizard
    return;
#endif

    Screens::Access()->PushBeforeCurrent(ScreenFactory::Screen<PseudoScreenCallback, MsgBoxHappyPrinting>);

#if HAS_EMERGENCY_STOP()
    static constexpr auto needs_emergency_stop_consent = [] {
        return !config_store().emergency_stop_enable.get()
            && !config_store().emergency_stop_disable_consent_given.get();
    };
    // Check first time - avoid black screen blinking if we're sure we won't need it
    if (needs_emergency_stop_consent()) {
        constexpr auto callback = +[] {
            // Check again - the user might have given the consent as part of the selftest snake
            if (needs_emergency_stop_consent()) {
                // Run the door sensor calibration, only ask for the consent (and run the calibration)
                marlin_client::gcode("M1980 O");
                static_assert(HAS_DOOR_SENSOR_CALIBRATION());
            }
        };
        Screens::Access()->PushBeforeCurrent(ScreenFactory::Screen<PseudoScreenCallback, callback>);
    }
#endif

#if HAS_HT_HOTEND()
    // Boot-time hotend detection was inconclusive (a hot NTC and a cold-to-hot PT1000
    // read alike on the nozzle ADC) — ask the user which hotend is installed before
    // running with a possibly-wrong temperature config. See detect_impl() and
    // hotend_detect_dialog_pending in tools_xbuddy.cpp for when this is raised.
    static_assert(PhysicalToolIndex::count == 1, "Hotend dialog assumes a single tool");
    if (hotend_detect_dialog_pending) {
        // Unary + decays the captureless lambda to a plain function pointer, which the
        // PseudoScreenCallback screen-factory template below requires.
        constexpr auto hotend_dialog_callback = +[] {
            // hotend_type is read from EEPROM: treat any non-HT value as the standard hotend
            // (the safe, lower-temperature one) — a future-FW value or storage corruption
            // must not soft-brick the printer (config-store enum policy).
            const bool is_high_temp = config_store().hotend_type.get(0) == HotendType::high_temp;

            const char *msg = is_high_temp
                ? N_("Hotend change detected\n\nSelected hotend: High-temp\n\nIs this correct?")
                : N_("Hotend change detected\n\nSelected hotend: Standard\n\nIs this correct?");

            // No = switch to the other hotend and reboot. A standard hotend gets stock_with_sock
            // (its sock is mandatory); HT clears it.
            if (MsgBoxWarning(_(msg), Responses_YesNo) == Response::No) {
                config_store().set_hotend_type_detected(PhysicalToolIndex::from_raw(0),
                    is_high_temp ? HotendType::stock_with_sock : HotendType::high_temp);
                sys_reset(); // Reboot with the new hotend config
            }
            // User confirmed the current hotend — continue booting
        };
        Screens::Access()->PushBeforeCurrent(ScreenFactory::Screen<PseudoScreenCallback, hotend_dialog_callback>);
    }
#endif

    bool should_show_welcome_screen = false;

#if HAS_SELFTEST() && !PRINTER_IS_PRUSA_iX()
    // A crude heuristic to make the wizard show only "on the first run"
    // Yes, we are ignoring other selftest results outside of this struct, but this is good enough for the purpose
    if (config_store().selftest_result.get() == config_store_ns::defaults::selftest_result) {
        Screens::Access()->PushBeforeCurrent(ScreenFactory::Screen<ScreenMenuSTSWizard>);
        should_show_welcome_screen = true;
    }
#endif

#if HAS_HEATBED_SCREWS_DURING_TRANSPORT()
    //  C1L is shipped with the bed screwed into the bottom of the chassis. And hence the screws have to be removed.
    const bool bed_screws_removal_approved = config_store().heatbed_screws_removal_approved.get();

    if (!bed_screws_removal_approved) {
        // Ask the user to approve the removal of the bed screws
        static constexpr point_ui16_t icon_point = point_ui16_t(40, 20);
        constexpr auto callback = [] {
            MsgBoxIconned msgbox(
                Rect16(0, 0, GuiDefaults::ScreenWidth, GuiDefaults::ScreenHeight),
                icon_point,
                Responses_Ok,
                0,
                _("Before using the 3D printer, it is necessary to remove all 3 screws, that secure the heated bed during transport.\n\nThe screws are marked with a sticker."),
                is_multiline::yes,
                &img::ac_heatbed_screw_80x246,
                is_closed_on_click_t::yes);
            Screens::Access()->gui_loop_until_dialog_closed();
            config_store().heatbed_screws_removal_approved.set(true);
        };
        Screens::Access()->PushBeforeCurrent(ScreenFactory::Screen<PseudoScreenCallback, callback>);
    };
#endif

    if (ScreenInitialNetworkSetup::should_show()) {
        Screens::Access()->PushBeforeCurrent(ScreenFactory::Screen<ScreenInitialNetworkSetup>);
        should_show_welcome_screen = true;
    }

    if (ScreenPrinterSetup::should_show()) {
        Screens::Access()->PushBeforeCurrent(ScreenFactory::Screen<ScreenPrinterSetup>);
        should_show_welcome_screen = true;
    }

    if (should_show_welcome_screen) {
        Screens::Access()->PushBeforeCurrent(ScreenFactory::Screen<ScreenWelcome>);
    }

    if (ScreenPrinterTypeChanged::should_show()) {
        Screens::Access()->PushBeforeCurrent(ScreenFactory::Screen<ScreenPrinterTypeChanged>);
    }

#if HAS_TOUCH()
    if (ScreenTouchDriverFailed::should_show()) {
        Screens::Access()->PushBeforeCurrent(ScreenFactory::Screen<ScreenTouchDriverFailed>);
    }
#endif

#if HAS_TRANSLATIONS()
    if (!LangEEPROM::getInstance().IsValid()) {
        Screens::Access()->PushBeforeCurrent(ScreenFactory::Screen<ScreenMenuLanguages, ScreenMenuLanguages::Context::initial_language_selection>);
    }
#endif

    // Set up progress mapper
    // Processes with low occurrence or short duration should have small scale number
    constexpr static ProgressMapperWorkflowArray workflow { std::to_array<ProgressMapperWorkflowStep<BootstrapStage>>(
        {
#if RESOURCES() || BOOTLOADER_UPDATE()
            { BootstrapStage::looking_for_bbf, 1 },
#endif
#if RESOURCES()
                { BootstrapStage::preparing_bootstrap, 1 },
                { BootstrapStage::copying_files, 50 },
#endif
#if BOOTLOADER_UPDATE()
                { BootstrapStage::preparing_update, 1 },
                { BootstrapStage::updating, 1 },
#endif
#if HAS_ESP()
                { BootstrapStage::flashing_esp, 1 },
                { BootstrapStage::reflashing_esp, 1 },
#endif
#if HAS_XBUDDY_EXTENSION()
                { BootstrapStage::looking_for_xbuddy_extension, 1 },
                { BootstrapStage::verifying_xbuddy_extension, 1 },
                { BootstrapStage::flashing_xbuddy_extension, 10 },
#endif
#if HAS_PUPPIES()
                { BootstrapStage::waking_up_puppies, 1 },
                { BootstrapStage::looking_for_puppies, 1 },
                { BootstrapStage::verifying_puppies, 1 },
    #if HAS_DWARF()
                { BootstrapStage::flashing_dwarf, 10 },
                { BootstrapStage::verifying_dwarf, 1 },
    #endif
    #if HAS_PUPPY_MODULARBED()
                { BootstrapStage::flashing_modular_bed, 10 },
                { BootstrapStage::verifying_modular_bed, 1 },
    #endif
    #if HAS_AC_CONTROLLER()
                { BootstrapStage::ac_controller_unknown, 1 },
                { BootstrapStage::ac_controller_verify, 1 },
                { BootstrapStage::ac_controller_flash, 10 },
                { BootstrapStage::ac_controller_ready, 1 },
    #endif
    #if HAS_TOOL_OFFSET_SENSOR()
                { BootstrapStage::tool_offset_sensor_unknown, 1 },
                { BootstrapStage::tool_offset_sensor_verify, 1 },
                { BootstrapStage::tool_offset_sensor_flash, 10 },
                { BootstrapStage::tool_offset_sensor_ready, 1 },
    #endif
    #if HAS_INDX_HEAD()
                { BootstrapStage::flashing_indx_head, 10 },
                { BootstrapStage::verifying_indx_head, 1 },
    #endif
#endif
        }) };

    progress_mapper.setup(workflow);
}

static const char *message(BootstrapStage stage) {
    switch (stage) {
    case BootstrapStage::initial:
        break;
#if RESOURCES() || BOOTLOADER_UPDATE()
    case BootstrapStage::looking_for_bbf:
        return "Looking for BBF...";
#endif
#if RESOURCES()
    case BootstrapStage::preparing_bootstrap:
        return "Preparing";
    case BootstrapStage::copying_files:
        return "Installing";
#endif
#if BOOTLOADER_UPDATE()
    case BootstrapStage::preparing_update:
    case BootstrapStage::updating:
        return "Updating bootloader";
#endif
#if HAS_ESP()
    case BootstrapStage::flashing_esp:
        return "Flashing ESP";
    case BootstrapStage::reflashing_esp:
        return "[ESP] Reflashing broken sectors";
#endif
#if HAS_XBUDDY_EXTENSION()
    case BootstrapStage::looking_for_xbuddy_extension:
        return "Looking for xbuddy extension";
    case BootstrapStage::verifying_xbuddy_extension:
        return "Verifying xbuddy extension";
    case BootstrapStage::flashing_xbuddy_extension:
        return "Flashing xbuddy extension";
#endif
#if HAS_PUPPIES()
    case BootstrapStage::waking_up_puppies:
        return "Waking up puppies";
    case BootstrapStage::looking_for_puppies:
        return "Looking for puppies";
    case BootstrapStage::verifying_puppies:
        return "Verifying puppies";
    #if HAS_DWARF()
    case BootstrapStage::flashing_dwarf:
        return "Flashing dwarf";
    case BootstrapStage::verifying_dwarf:
        return "Verifying dwarf";
    #endif
    #if HAS_PUPPY_MODULARBED()
    case BootstrapStage::flashing_modular_bed:
        return "Flashing modularbed";
    case BootstrapStage::verifying_modular_bed:
        return "Verifying modularbed";
    #endif
    #if HAS_AC_CONTROLLER()
    case BootstrapStage::ac_controller_unknown:
        return "AC controller: unknown";
    case BootstrapStage::ac_controller_verify:
        return "AC controller: verifying";
    case BootstrapStage::ac_controller_flash:
        return "AC controller: flashing";
    case BootstrapStage::ac_controller_ready:
        return "AC controller: ready";
    #endif
    #if HAS_TOOL_OFFSET_SENSOR()
    case BootstrapStage::tool_offset_sensor_unknown:
        return "Tool offset sensor: unknown";
    case BootstrapStage::tool_offset_sensor_verify:
        return "Tool offset sensor: verifying";
    case BootstrapStage::tool_offset_sensor_flash:
        return "Tool offset sensor: flashing";
    case BootstrapStage::tool_offset_sensor_ready:
        return "Tool offset sensor: ready";
    #endif
    #if HAS_INDX_HEAD()
    case BootstrapStage::flashing_indx_head:
        return "Flashing INDX head"; // INDX_TODO: Verify text with product
    case BootstrapStage::verifying_indx_head:
        return "Verifying INDX head"; // INDX_TODO: Verify text with product
    #endif
#endif
    }
    bsod_unreachable();
}

ScreenSplash::~ScreenSplash() {
    display::enable_resource_file(); // now it is safe to use resources from xFlash
}

void ScreenSplash::draw() {
    Validate();
    progress.Invalidate();
    text_progress.Invalidate();
    screen_t::draw(); // We want to draw over bootloader's screen without flickering/redrawing
#ifdef _DEBUG
    #if HAS_MINI_DISPLAY()
    display::draw_text(Rect16(180, 91, 60, 16), string_view_utf8::MakeCPUFLASH("DEBUG"), Font::small, COLOR_BLACK, COLOR_RED);
    #endif
    #if HAS_LARGE_DISPLAY()
    display::draw_text(Rect16(340, 130, 60, 16), string_view_utf8::MakeCPUFLASH("DEBUG"), Font::small, COLOR_BLACK, COLOR_RED);
    #endif
#endif //_DEBUG
}

void ScreenSplash::windowEvent(window_t *, GUI_event_t event, void *) {
    if (event == GUI_event_t::LOOP) {
        const auto bootstrap_state = buddy::bootstrap_state_get();
        text_progress.SetText(bootstrap_state.stage == BootstrapStage::initial
                ? string_view_utf8::MakeRAM(text_progress_buffer)
                : string_view_utf8::MakeCPUFLASH(message(bootstrap_state.stage)));
        // FW Splash screen starts from progress bar on 50 %
        const uint8_t progress_percent = bootstrap_state.stage == BootstrapStage::initial ? 50 : 50 + progress_mapper.update_progress(bootstrap_state.stage, static_cast<float>(bootstrap_state.percent) / 100.f) / 2;
        progress.set_progress_percent(progress_percent);
    }
}
