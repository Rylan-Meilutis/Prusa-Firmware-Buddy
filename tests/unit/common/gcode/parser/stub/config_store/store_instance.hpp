#pragma once

#include <bitset>

#include <filament.hpp>

struct ConfigStoreStub {
    template <typename T>
    struct Item {
        std::array<T, 8> values {};
        T get() { return values[0]; }
        T get(auto index) { return values[static_cast<size_t>(index)]; }

        void set(T value) { values[0] = value; }
        void set(auto index, T value) { values[static_cast<size_t>(index)] = value; }

        void apply(auto callback) { callback(values[0]); }
    };

    Item<std::bitset<max_preset_filament_type_count>> visible_preset_filament_types;
    Item<std::bitset<max_preset_filament_type_count>> visible_user_filament_types;

    Item<FilamentTypeParameters_EEPROM1> adhoc_filament_parameters;
    Item<FilamentTypeParameters_EEPROM5> adhoc_filament_parameters_5;

    Item<FilamentTypeParameters_EEPROM1> user_filament_parameters;
};

ConfigStoreStub &config_store() {
    static ConfigStoreStub stub;
    return stub;
}
