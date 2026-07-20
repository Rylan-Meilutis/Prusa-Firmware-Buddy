#include <leds/side_strip_handler.hpp>

namespace leds {

uint8_t side_max_brightness = 0;
bool side_activity_pinged = false;

SideStripHandler::SideStripHandler() {}

SideStripHandler &SideStripHandler::instance() {
    static SideStripHandler instance;
    return instance;
}

void SideStripHandler::activity_ping() {
    side_activity_pinged = true;
}

void SideStripHandler::set_max_brightness(uint8_t set) {
    side_max_brightness = set;
}

uint8_t SideStripHandler::current_brightness() const {
    return side_max_brightness;
}

} // namespace leds
