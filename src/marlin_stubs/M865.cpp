#include "PrusaGcodeSuite.hpp"

#include <option/has_filament_heatbreak_param.h>

#include <filament.hpp>
#include <print_utils.hpp>
#include <filament_color.hpp>
#include <temperature.hpp>
#include <utils/string_builder.hpp>

namespace {

void report_loaded_filaments() {
    for (uint8_t tool = 0; tool < EXTRUDERS; ++tool) {
        if (!is_tool_enabled(tool)) {
            continue;
        }

        const FilamentType filament_type = config_store().get_filament_type(tool);
        const FilamentTypeParameters params = filament_type.parameters();

        SERIAL_ECHO("loaded_filament T");
        SERIAL_ECHO(tool);
        SERIAL_ECHO(" S\"");
        SERIAL_ECHO(params.name.data());
        SERIAL_ECHO("\" O\"");
        if (const auto color = filament_color::loaded(tool)) {
            SERIAL_ECHO(filament_color::profile_for(*color).name.data());
            char hex[8];
            snprintf(hex, sizeof(hex), "#%06lx", static_cast<unsigned long>(color->raw & 0xffffff));
            SERIAL_ECHO("\" H\"");
            SERIAL_ECHO(hex);
        } else {
            SERIAL_ECHO("None\" H\"none");
        }
        SERIAL_ECHOLN("\"");
    }
}

} // namespace

/** \addtogroup G-Codes
 * @{
 */

/**
 *### M865: Manage filament parameters
 *
 * Utility G-Code that allows managing filament types and their parameters.
 * Allows changing filament parameters and force-setting the currently loaded filament.
 * Also print selected filament parameters to the serial.
 *
 *#### Parameters
 * - `S"<name>"` - Select filament with the specified name
 * - `I<ix>` - Select filament currently loaded to the specified tool (indexed from 0)
 * - `U<ix>` - Select User filament (indexed from 0)
 * - `X` - Select (pending) Custom filament type that will be loaded using `M600 F"#"` (or similar filament change gcode)
 * - `Q` - Query the currently loaded filament material for all enabled tools
 * - `V<ix> O<color> N"<name>"` - define one of eight persistent custom colors
 *
 * - `L<ix>` - Set currently loaded filament for the given tool to the selected filament
 * - `O<color>` - Set the loaded color together with `L` (`#RRGGBB`, palette name, or RGB integer)
 *
 * - `R` - Reset parameters not specified in this gcode to defaults
 *
 * - `T<val>` - Set nozzle temperature
 * - `P<val>` - Set nozzle preheat temperature
 * - `B<val>` - Set bed temperature
 * - `H<val>` - Set heatbreak temperature
 * - `A<val>` - Set is abrasive
 * - `G<val>` - Set disable auto retract (1 = no retraction)
 *
 * - `C<val>` - Set target chamber temperature
 * - `D<val>` - Set minimum chamber temperature
 * - `E<val>` - Set maximum chamber temperature
 * - `F<val>` - Set requires filtration
 *
 * - `N"<name>"` - Set name
 *
 */
void PrusaGcodeSuite::M865() {
    GCodeParser2 p;
    if (!p.parse_marlin_command()) {
        return;
    }

    if (p.option<bool>('Q').value_or(false)) {
        report_loaded_filaments();
        return;
    }

    const auto requested_color = p.option<Color>('O');
    if (const auto custom_slot = p.option<uint8_t>('V', static_cast<uint8_t>(0), static_cast<uint8_t>(filament_color::custom_slot_count - 1))) {
        std::array<char, filament_color::name_capacity> color_name {};
        const auto name = p.option<std::string_view>('N', color_name);
        if (!requested_color || !name || !filament_color::set_custom(*custom_slot, *name, *requested_color)) {
            SERIAL_ERROR_MSG("Custom color requires V0..7, O<color>, and N\"name\" (15 characters maximum).");
        }
        return;
    }

    FilamentType filament_type;

    if (const auto slot = p.option<uint8_t>('I', static_cast<uint8_t>(0), static_cast<uint8_t>(EXTRUDERS - 1))) {
        filament_type = config_store().get_filament_type(*slot);

    } else if (p.option<bool>('X').value_or(false)) {
        filament_type = PendingAdHocFilamentType {};

    } else if (const auto slot = p.option<uint8_t>('U', static_cast<uint8_t>(0), static_cast<uint8_t>(user_filament_type_count - 1))) {
        filament_type = UserFilamentType { .index = *slot };

    } else if (const auto ft = p.option<FilamentType>('S')) {
        filament_type = *ft;

    } else {
        SERIAL_ERROR_MSG("Filament type invalid or not specified.");
        return;
    }

    FilamentTypeParameters params = filament_type.parameters();

    if (p.option<bool>('R').value_or(false)) {
        params = {};
    }

    p.store_option('T', params.nozzle_temperature);
    p.store_option('P', params.nozzle_preheat_temperature);
    p.store_option('B', params.heatbed_temperature);

    p.store_option('A', params.is_abrasive);
    p.store_option('G', params.do_not_auto_retract);

#if HAS_FILAMENT_HEATBREAK_PARAM()
    p.store_option('H', params.heatbreak_temperature);
#endif

#if HAS_CHAMBER_API()
    p.store_option('C', params.chamber_target_temperature);
    p.store_option('D', params.chamber_min_temperature);
    p.store_option('E', params.chamber_max_temperature);
    p.store_option('F', params.requires_filtration);
#endif

    std::array<char, filament_name_buffer_size - 1> name_buf;
    if (const auto opt = p.option<std::string_view>('N', name_buf)) {
        StringBuilder b(params.name);
        b.append_std_string_view(*opt);

        if (const auto r = filament_type.can_be_renamed_to(params.name.data()); !r) {
            SERIAL_ERROR_START();
            SERIAL_ECHOLN(r.error());
            return;
        }
    }

    if (filament_type.is_customizable()) {
        filament_type.set_parameters(params);
    }

    if (auto load = p.option<uint8_t>('L', static_cast<uint8_t>(0), static_cast<uint8_t>(EXTRUDERS - 1))) {
        config_store().set_filament_type(*load, filament_type);
        if (requested_color) filament_color::set_loaded(*load, requested_color);
    }

    if (filament_type != FilamentType::none) {
        SERIAL_ECHOLNPAIR("name:", params.name.data());
        SERIAL_ECHOLNPAIR("nozzle_temperature:", params.nozzle_temperature);
        SERIAL_ECHOLNPAIR("heatbed_temperature:", params.heatbed_temperature);
        SERIAL_ECHOLNPAIR("is_abrasive:", params.is_abrasive);
#if HAS_CHAMBER_API()
        SERIAL_ECHOLNPAIR("requires_filtration:", params.requires_filtration);
#endif
    }
}

/** @}*/
