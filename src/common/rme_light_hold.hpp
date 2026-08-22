#pragma once

namespace rme_light_hold {

enum class SetResult {
    unchanged,
    changed,
    printer_busy,
};

class State {
public:
    SetResult set_from_host(const bool requested, const bool print_active) {
        if (requested && print_active) {
            return SetResult::printer_busy;
        }
        if (active_ == requested) {
            return SetResult::unchanged;
        }
        active_ = requested;
        return SetResult::changed;
    }

    void release_automatically() {
        if (active_) {
            active_ = false;
            release_pending_ = true;
        }
    }

    void release_with_session() {
        active_ = false;
        release_pending_ = false;
    }

    bool active() const { return active_; }

    bool consume_automatic_release() {
        const bool pending = release_pending_;
        release_pending_ = false;
        return pending;
    }

private:
    bool active_ = false;
    bool release_pending_ = false;
};

} // namespace rme_light_hold
