#include <img_resources.hpp>

#include <freertos/timing.hpp>

namespace img {

const Resource *spinner_16x16_animated() {
    return spinner_16x16_stages[(freertos::millis() / 256) % spinner_16x16_stages.size()];
}

} // namespace img
