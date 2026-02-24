/// @file
#include <module/motion.h>
#include <module/planner.h>

#if HAS_LEVELING
    #include <feature/bedlevel/bedlevel.h>
#endif

#if ENABLED(ENABLE_LEVELING_FADE_HEIGHT)
/**
 * Get the Z leveling fade factor based on the given Z height,
 * re-calculating only when needed.
 *
 *  Returns 1.0 if planner.z_fade_height is 0.0.
 *  Returns 0.0 if Z is past the specified 'Fade Height'.
 */
static inline float fade_scaling_factor_for_z(float rz) {
    if (!Planner::z_fade_height) {
        return 1;
    }
    if (rz >= Planner::z_fade_height) {
        return 0;
    }
    return 1 - rz * Planner::inverse_z_fade_height;
}

#else

static constexpr float fade_scaling_factor_for_z(float) { return 1; }

#endif

MachinePosXYZ to_machine_pos(const xyz_pos_t &pos) {
    MachinePosXYZ result = pos.to_tag<MachinePosTag>();

#if HAS_MESH
    if (planner.leveling_active) {
        const float fade_scaling_factor = fade_scaling_factor_for_z(pos.z);
        result.z += fade_scaling_factor * ubl.get_z_correction(result);
    }
#endif

    return result;
}

xyz_pos_t to_native_pos(const MachinePosXYZ &pos) {
    xyz_pos_t result = pos.to_tag<NativePosTag>();

#if HAS_MESH
    if (planner.leveling_active) {
        // !!! This is WRONG. We are taking the scaling factor from the wrong Z.
        // We should be takking it from the original Z, but we can't because we don't know it,
        // otherwise we wouldn't be calling this function to compute it.
        // This logic is principially flawed, I don't have a better solution at hand though.
        const float fade_scaling_factor = fade_scaling_factor_for_z(pos.z);
        result.z -= fade_scaling_factor * ubl.get_z_correction(result);
    }
#endif

    return result;
}
