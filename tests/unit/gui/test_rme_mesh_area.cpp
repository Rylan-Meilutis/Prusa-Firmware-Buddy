#include "src/common/rme_mesh_area.hpp"
#include <limits>

int main() {
    using namespace buddy::rme_mesh_area;
    if (!Area { 0, 0, 250, 205.5f }.valid(250, 205.5f)
        || Area { -1, 0, 20, 20 }.valid(250, 205.5f)
        || Area { 0, 0, 0, 20 }.valid(250, 205.5f)
        || Area { 240, 0, 20, 20 }.valid(250, 205.5f)
        || Area { 0, 0, 20, std::numeric_limits<float>::infinity() }.valid(250, 205.5f)
        || Area { std::numeric_limits<float>::quiet_NaN(), 0, 20, 20 }.valid(250, 205.5f)) {
        return 1;
    }
    pending = Area { 10, 20, 30, 40 };
    const auto area = take();
    return !area || area->x != 10 || area->height != 40 || take().has_value();
}
