#pragma once

namespace buddy::filament_material {

/// Detach every loaded reference before the caller replaces a shared preset.
/// The snapshot callback must copy complete parameters into per-tool storage.
template <typename Preset, typename Tools, typename GetLoaded, typename Snapshot>
void preserve_loaded_preset(const Preset &preset, Tools tools, GetLoaded get_loaded, Snapshot snapshot) {
    for (const auto tool : tools) {
        if (get_loaded(tool) == preset) {
            snapshot(tool);
        }
    }
}

} // namespace buddy::filament_material
