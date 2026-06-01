/// @file
#pragma once

#include <bitset>
#include <cmath>

#include "requests_read_multi.hpp"

#include <filament.hpp>
#include <filament_meta.hpp>
#include <option/has_chamber_api.h>

namespace buddy::openprinttag {

/// Utility for determining remaining and full material amounts
struct AmountsInfo {

    using Requirements = ValuePack<
        MainField::nominal_netto_full_weight,
        MainField::actual_netto_full_weight,
        AuxField::consumed_weight>;

    using RequestRef = Requirements::ApplyOn<MultiReadFieldRequestRef>;

    explicit AmountsInfo(const RequestRef &req);

    /// Remaining amount of material, in grams
    /// NAN if unknown
    std::optional<float> remaining_weight_g;

    /// Netto weight of the full spool, in grams
    /// NAN if unknown
    std::optional<float> full_weight_g;
};

/// Utilify for determining material type and its abbreviation
struct AbbreviationInfo {

    using Requirements = ValuePack<
        MainField::material_type,
        MainField::material_abbreviation>;

    using RequestRef = Requirements::ApplyOn<MultiReadFieldRequestRef>;

    explicit AbbreviationInfo(const RequestRef &req);

    /// Abbreviation of the material
    /// subset of the @p abbreviation_buffer
    std::string_view abbreviation;

    /// Buffer used for storing @p abbreviation
    FieldTraits<MainField::material_abbreviation>::Buffer abbreviation_buffer;
};

/// Utility for determining filament parameters
struct FilamentParametersInfo {

    using BaseRequirements = ValuePack<
        MainField::min_print_temperature,
        MainField::max_print_temperature,
        MainField::preheat_temperature,
        MainField::max_bed_temperature,
        MainField::min_bed_temperature,
#if HAS_CHAMBER_API()
        MainField::min_chamber_temperature,
        MainField::max_chamber_temperature,
        MainField::chamber_temperature,
#endif
        MainField::shore_hardness_a,
        MainField::shore_hardness_d,
        MainField::tags,
        MainField::material_type>;

    using Requirements = BaseRequirements::Concatenate<AbbreviationInfo::Requirements>::Unique<>;
    using RequestRef = Requirements::ApplyOn<MultiReadFieldRequestRef>;

    using MissingParameters = std::bitset<filament_type_parameter_count>;

    explicit FilamentParametersInfo(const RequestRef &req);

    /// @returns whether the information is missing the specified parameter
    template <auto FilamentTypeParameters::*mem_ptr>
    inline bool is_missing() const {
        return missing_parameters.test(filament_type_parameter_index<mem_ptr>);
    }

    FilamentTypeParameters parameters;

    /// Bitset of parameters that the utility failed to deduce
    /// Indexes correspond with @p filament_type_parameter_index of each member
    MissingParameters missing_parameters;
};

} // namespace buddy::openprinttag
