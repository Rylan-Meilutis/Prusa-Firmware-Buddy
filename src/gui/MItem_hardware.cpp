#include "MItem_hardware.hpp"
#include "ScreenHandler.hpp"
#include "WindowMenuSpin.hpp"
#include "window_msgbox.hpp"
#include "marlin_client.hpp"
#include <common/sys.hpp>
#include <option/has_15gt_belts.h>
#include <option/has_toolchanger.h>
#include <option/has_side_fsensor_remap.h>
#include <option/has_nozzle_cleaner_lite.h>
#include <common/nozzle_diameter.hpp>
#include <common/printer_model_data.hpp>
#include <option/has_print_fan_type.h>
#if HAS_PRINT_FAN_TYPE()
    #include <print_fan_type.hpp>
    #include <tool_index.hpp>
#endif

#if HAS_CHAMBER_VENTS()
    #include <feature/chamber/chamber_enums.hpp>
#endif

#if HAS_TOOLCHANGER()
    #include <module/prusa/toolchanger.h>
    #include <puppies/Dwarf.hpp>
#endif

#include <option/has_side_fsensor_remap.h>
#if HAS_SIDE_FSENSOR_REMAP()
    #include <feature/filament_sensor/filament_sensors_handler_remap.hpp>
#endif

static constexpr const char *hw_check_items[] = {
    N_("None"),
    N_("Warn"),
    N_("Strict"),
};

MI_HARDWARE_CHECK::MI_HARDWARE_CHECK(HWCheckType check_type)
    : MenuItemSwitch(_(hw_check_type_names[check_type]), hw_check_items, static_cast<int>(config_store().visit_hw_check(check_type, [](auto &item) { return item.get(); })))
    , check_type(check_type) //
{}

void MI_HARDWARE_CHECK::OnChange([[maybe_unused]] size_t old_index) {
    config_store().visit_hw_check(check_type, [set = static_cast<HWCheckSeverity>(this->get_index())](auto &item) { item.set(set); });
}

#if HAS_SIDE_FSENSOR_REMAP()
// MI_SIDE_FSENSOR_REMAP
MI_SIDE_FSENSOR_REMAP::MI_SIDE_FSENSOR_REMAP()
    : WI_ICON_SWITCH_OFF_ON_t(side_fsensor_remap::is_remapped(), _(label), nullptr, is_enabled_t::yes, is_hidden_t::no) {};

void MI_SIDE_FSENSOR_REMAP::OnChange([[maybe_unused]] size_t old_index) {
    if (uint8_t mask = side_fsensor_remap::ask_to_remap(); mask != 0) { // Ask user to remap
        Screens::Access()->Get()->Validate(); // Do not redraw this menu yet

        // Change index by what user selected)
        set_value(side_fsensor_remap::is_remapped());

    #if HAS_SELFTEST()
        Validate(); // Do not redraw this switch yet
        marlin_client::gcode_printf("M1981 F%i", (int)mask); // Start filament sensor calibration for moved tools
    #endif

    } else {
        // Change index by what user selected)
        set_value(side_fsensor_remap::is_remapped());
    }
}
#endif

#if HAS_EXTENDED_PRINTER_TYPE()
MI_EXTENDED_PRINTER_TYPE::MI_EXTENDED_PRINTER_TYPE()
    : MenuItemSelectMenu(_("Printer Type")) //
{
    set_current_item(config_store().extended_printer_type.get());
}

int MI_EXTENDED_PRINTER_TYPE::item_count() const {
    return static_cast<int>(extended_printer_type_model.size());
}

string_view_utf8 MI_EXTENDED_PRINTER_TYPE::build_item_text(int index, [[maybe_unused]] MenuItemSelectMenu::ItemTextParams &params) const {
    return string_view_utf8::MakeCPUFLASH(PrinterModelInfo::get(extended_printer_type_model[index]).id_str);
}

bool MI_EXTENDED_PRINTER_TYPE::on_item_selected(const OnItemSelectedArgs &args) {
    config_store().extended_printer_type.set(args.new_index);

    #if HAS_PRINT_FAN_TYPE() && PRINTER_IS_PRUSA_XL()
    // Auto-set print fan type based on variant: XLS uses LDO, XL uses Delta (default)
    {
        auto model = extended_printer_type_model[args.new_index];
        auto fan_type = (model == PrinterModel::xls) ? PrintFanType::LDO_D5015G08B05X71 : PrintFanType::DELTA_BFB0505HHA_CWCD;
        for (auto tool : PhysicalToolIndex::all()) {
            set_print_fan_type(tool.to_raw(), fan_type);
        }
    }
    #endif

    #if HAS_TOOLCHANGER() && PRINTER_IS_PRUSA_XL()
    {
        buddy::puppies::Dwarf::FanMode fan_mode = (extended_printer_type_model[args.new_index] == PrinterModel::xls) ? buddy::puppies::Dwarf::FanMode::XLS_NATIVE : buddy::puppies::Dwarf::FanMode::XL_LEGACY;
        for (auto &dwarf : buddy::puppies::dwarfs) {
            dwarf.set_fan_mode(fan_mode);
        }
    }
    #endif

    #if EXTENDED_PRINTER_TYPE_DETERMINES_MOTOR_STEPS()
    // Reset motor configuration if the printer types have different motors
    if (extended_printer_type_has_400step_motors[args.old_index] != extended_printer_type_has_400step_motors[args.new_index]) {
        {
            auto &store = config_store();
            auto transaction = store.get_backend().transaction_guard();
            store.homing_sens_x.set_to_default();
            store.homing_sens_y.set_to_default();
            store.homing_bump_divisor_x.set_to_default();
            store.homing_bump_divisor_y.set_to_default();

        #if HAS_PRECISE_HOMING()
            store.precise_homing_sample_history.set_all_to_default();
            store.precise_homing_sample_history_index.set_all_to_default();
        #endif
        }

        // Reset XY homing sensitivity
        marlin_client::gcode("M914 X Y");

        // XY motor currents
        marlin_client::gcode_printf("M906 X%u Y%u", get_rms_current_ma_x(), get_rms_current_ma_y());

        // XY motor microsteps
        marlin_client::gcode_printf("M350 X%u Y%u", get_microsteps_x(), get_microsteps_y());
    }
    #endif

    return true;
}
#endif

#if HAS_EMERGENCY_STOP()
static bool user_made_informed_decision_to_disable_door_sensor() {
    const Response response = MsgBoxWarning(
        _(
            "Caution! Disabling the door sensor may lead to injury or printer damage. "
            "Proceeding means you accept full responsibility. "
            "We are not liable for any harm or damages."),
        { Response::Disable, Response::Cancel },
        1 /* default is to cancel in order to prevent double clicks */);
    return response == Response::Disable;
}

MI_EMERGENCY_STOP_ENABLE::MI_EMERGENCY_STOP_ENABLE()
    : WI_ICON_SWITCH_OFF_ON_t(config_store().emergency_stop_enable.get(), _(label), nullptr, is_enabled_t::yes, is_hidden_t::no) {};

void MI_EMERGENCY_STOP_ENABLE::OnChange([[maybe_unused]] size_t old_index) {
    if (!value() && !user_made_informed_decision_to_disable_door_sensor()) {
        // revert the change in GUI and keep config store intact
        set_value(true);
        return;
    }

    config_store().emergency_stop_enable.set(value());
    config_store().emergency_stop_disable_consent_given.set(!value());
}
#endif

#if HAS_CHAMBER_VENTS()
static constexpr const char *chamber_vent_control_items[] = {
    N_("Off"),
    N_("Auto"),
    N_("Manual"),
};

static_assert(VentControl(0) == VentControl::off && VentControl(1) == VentControl::automatic && VentControl(2) == VentControl::manual, "menu item misalignment");

MI_SWITCH_VENT_MECHANISM::MI_SWITCH_VENT_MECHANISM()
    : MenuItemSwitch(_("Chamber Vent Control"), chamber_vent_control_items, std::to_underlying(config_store().get_vent_control())) {}

void MI_SWITCH_VENT_MECHANISM::OnChange([[maybe_unused]] size_t old_index) {
    config_store().set_vent_control(VentControl(get_index()));
}
#endif

#if HAS_PRECISE_HOMING_COREXY()
constexpr const EnumArray<Tristate::Value, const char *, 3> ask_always_never_texts {
    { Tristate::no, N_("Never") },
    { Tristate::yes, N_("Auto") },
    { Tristate::other, N_("Ask") },
};

MI_AUTO_PRECISE_HOMING_CALIBRATION::MI_AUTO_PRECISE_HOMING_CALIBRATION()
    : MenuItemSwitch(_("Homing Calibration"), ask_always_never_texts, config_store().auto_recalibrate_precise_homing.get().value) {
}

void MI_AUTO_PRECISE_HOMING_CALIBRATION::OnChange(size_t) {
    config_store().auto_recalibrate_precise_homing.set(static_cast<Tristate::Value>(get_index()));
}
#endif
#if HAS_EXPANSION_JOINTS_GEN_2()
MI_EXPANSION_JOINTS_GEN_2::MI_EXPANSION_JOINTS_GEN_2()
    : WI_ICON_SWITCH_OFF_ON_t(config_store().ejg2_installed.get(), _(label), nullptr, is_enabled_t::yes, is_hidden_t::no) {}

void MI_EXPANSION_JOINTS_GEN_2::OnChange([[maybe_unused]] size_t old_index) {
    config_store().ejg2_installed.set(value());
}
#endif

#if HAS_NOZZLE_CLEANER_LITE()
MI_NOZZLE_CLEANER_LITE::MI_NOZZLE_CLEANER_LITE()
    : WI_ICON_SWITCH_OFF_ON_t(config_store().nozzle_cleaner_lite_installed.get(), _(label), nullptr, is_enabled_t::yes, is_hidden_t::no) {};

void MI_NOZZLE_CLEANER_LITE::OnChange([[maybe_unused]] size_t old_index) {
    bool nozzle_cleaner_lite_present = value();
    if (MsgBoxWarning(_("Enabling/disabling nozzle cleaner lite will result in different moves in some sequences. Continue?"), { Response::Yes, Response::No }, 1) != Response::Yes) {
        set_value(!nozzle_cleaner_lite_present);
        return;
    }

    {
        config_store().nozzle_cleaner_lite_installed.set(nozzle_cleaner_lite_present);
    }
}
#endif

#if HAS_15GT_BELTS()
MI_BELTS_15GT::MI_BELTS_15GT()
    : WI_ICON_SWITCH_OFF_ON_t(config_store().belts_15gt_installed.get(), _(label), nullptr, is_enabled_t::yes, is_hidden_t::no) {}

void MI_BELTS_15GT::OnChange([[maybe_unused]] size_t old_index) {
    const bool belts_15gt_installed = value();
    if (MsgBoxWarning(_("Changing the belt type updates the X/Y steps/mm, resets the XY calibration and restarts the printer. Wrongly selected belt type may cause imprecise prints and XY homing issues. Continue?"),
            { Response::Yes, Response::No }, 1)
        != Response::Yes) {
        set_value(!belts_15gt_installed); // revert the GUI, keep config store intact
        return;
    }

    {
        auto &store = config_store();
        auto transaction = store.get_backend().transaction_guard();
        store.belts_15gt_installed.set(belts_15gt_installed);
        // clear any manual override so steps follow the used HW setup (belts)
        store.axis_steps_per_unit_x.set_to_default();
        store.axis_steps_per_unit_y.set_to_default();
        // steps/mm changed -> XY geometry calibration is invalid
        store.homing_sens_x.set_to_default();
        store.homing_sens_y.set_to_default();
        store.homing_bump_divisor_x.set_to_default();
        store.homing_bump_divisor_y.set_to_default();
    #if HAS_PRECISE_HOMING()
        store.precise_homing_sample_history.set_all_to_default();
        store.precise_homing_sample_history_index.set_all_to_default();
    #endif

        // Also let the user re-do the axis tests
        store.selftest_result.apply([](SelftestResult &r) {
            r.set_xaxis(TestResult::unknown);
            r.set_yaxis(TestResult::unknown);
        });
    }

    sys_reset();
}
#endif
