#include "gcode_parser2_test_common.hpp"

#include <filament.hpp>
#include <filament_list.hpp>
#include <m865_material.hpp>
#include <utils/color.hpp>

TEST_CASE("M865 eighth-spool J parameter retains its material", "[m865][regression]") {
    GCodeParser2 p(fail_test_error_callback);
    REQUIRE(p.parse("M865 X R N\"PLA-00C\" J\"PLA\" T215 P175 B60 G0 L7 O\"#0073ff\""));
    FilamentTypeParameters params {};
    REQUIRE(buddy::m865_material::apply_base_option(p, params));
    REQUIRE(params.base_preset == PresetFilamentType::PLA);
    std::array<char, filament_name_buffer_size> name;
    params.name = *p.option<std::string_view>('N', name);
    REQUIRE(p.store_option_if_present('T', params.nozzle_temperature));
    REQUIRE(p.store_option_if_present('P', params.nozzle_preheat_temperature));
    REQUIRE(p.store_option_if_present('B', params.heatbed_temperature));
    // Execute real PendingAdHoc -> per-tool serialization/readback instead
    // of stubbing either FilamentType::set_parameters or parameters().
    const FilamentType pending = PendingAdHocFilamentType {};
    pending.set_parameters(params);
    const FilamentType loaded = AdHocFilamentType { .tool = 7 };
    loaded.set_parameters(pending.parameters());
    const auto saved = loaded.parameters();
    CHECK(saved.base_preset == PresetFilamentType::PLA);
    CHECK(saved.name == "PLA-00C");
    CHECK(std::string_view(filament_material_name(saved).data()) == "PLA");
    CHECK(saved.nozzle_temperature == 215);
    CHECK(saved.nozzle_preheat_temperature == 175);
    CHECK(saved.heatbed_temperature == 60);
    CHECK(p.option<Color>('O') == Color::from_rgb(0, 0x73, 0xff));
}

TEST_CASE("M865 real parser retains every preset family", "[m865][regression]") {
    for (const auto type : all_filament_types) {
        const auto preset = std::get_if<PresetFilamentType>(&type);
        if (!preset) {
            continue;
        }
        const auto name = type.parameters().name;
        INFO(name.data());
        GCodeParser2 p(fail_test_error_callback);
        const auto command = std::string("M865 X R N\"CUSTOM\" J\"") + name.data() + "\" L7";
        REQUIRE(p.parse(command));
        FilamentTypeParameters params {};
        REQUIRE(buddy::m865_material::apply_base_option(p, params));
        CHECK(params.base_preset == *preset);
    }
}

TEST_CASE("M865 missing empty invalid and oversized J values are distinct", "[m865][regression]") {
    FilamentTypeParameters params {};
    params.base_preset = PresetFilamentType::PETG;
    GCodeParser2 p(fail_test_error_callback);
    REQUIRE(p.parse("M865 X N\"CUSTOM\" L7"));
    REQUIRE(buddy::m865_material::apply_base_option(p, params));
    CHECK(params.base_preset == PresetFilamentType::PETG);
    REQUIRE(p.parse("M865 X J\"\" L7"));
    REQUIRE(buddy::m865_material::apply_base_option(p, params));
    CHECK_FALSE(params.base_preset.has_value());
    for (const char *bad : { "INVALID", "PLA-00C", "THIS_VALUE_IS_TOO_LONG" }) {
        params.base_preset = PresetFilamentType::PETG;
        const auto command = std::string("M865 X J\"") + bad + "\" L7";
        REQUIRE(p.parse(command));
        bool accepted = true;
        CHECK(execution_failed(p, [&] { accepted = buddy::m865_material::apply_base_option(p, params); }));
        CHECK_FALSE(accepted);
        CHECK(params.base_preset == PresetFilamentType::PETG);
    }
}

TEST_CASE("gcode_parser::custom_type::filament") {
    std::array<char, 64> buf;

    SECTION("1") {
        GCodeParser2 p(fail_test_error_callback);
        REQUIRE(p.parse("M1 C\"PETG\" A\"PLA\" G\"X\" O\"#\""));

        CHECK(p.option<std::string_view>('G', buf) == "X");
        CHECK(p.option<FilamentType>('G') == std::nullopt);

        CHECK(p.option<std::string_view>('C', buf) == "PETG");
        CHECK(p.option<FilamentType>('C') == PresetFilamentType::PETG);

        CHECK(p.option<std::string_view>('A', buf) == "PLA");
        CHECK(p.option<FilamentType>('A') == PresetFilamentType::PLA);

        CHECK(p.option<std::string_view>('O', buf) == "#");
        CHECK(p.option<FilamentType>('O') == PendingAdHocFilamentType());
    }
}

TEST_CASE("gcode_parser::custom_type::color") {
    SECTION("1") {
        GCodeParser2 p(fail_test_error_callback);
        REQUIRE(p.parse("A1 A\"TERRACOTTA\" C\"#112233\" B0 D65535"));

        CHECK(p.option<Color>('A') == Color::from_raw(0xB87F6A));
        CHECK(p.option<Color>('B') == COLOR_BLACK);
        CHECK(p.option<Color>('C') == Color::from_rgb(0x11, 0x22, 0x33));
        CHECK(p.option<Color>('D') == Color::from_rgb(0, 255, 255));
    }
}
