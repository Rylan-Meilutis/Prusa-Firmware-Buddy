#pragma once

#include <cmath>
#include <optional>

namespace buddy::rme_mesh_area {
struct Area {
    float x, y, width, height;
    bool valid(float bed_x, float bed_y) const {
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(width) && std::isfinite(height)
            && x >= 0 && y >= 0 && width > 0 && height > 0
            && x + width <= bed_x && y + height <= bed_y;
    }
};
// Service frames can arrive while another command is executing. Stage the
// bounds and consume them only at G29 P1, never during an ongoing probe.
inline std::optional<Area> pending;
inline std::optional<Area> take() {
    const auto result = pending;
    pending.reset();
    return result;
}
} // namespace buddy::rme_mesh_area
