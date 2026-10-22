#pragma once
#include <cstdint>

namespace rme_tool_review {
// One bounded, job-scoped transaction. Terminal IDs are retained so retries
// and delayed SHOW/SUBMIT frames cannot reopen an already completed decision.
enum class Phase : uint8_t { idle,
    loading,
    editing,
    submitted,
    complete,
    cancelled };
struct State {
    uint32_t id = 0;
    uint32_t revision = 0;
    uint8_t used = 0;
    uint8_t generation = 0;
    Phase phase = Phase::idle;
    bool matches(uint32_t candidate) const { return id && id == candidate; }
    bool active() const { return phase == Phase::loading || phase == Phase::editing || phase == Phase::submitted; }
    bool begin(uint32_t candidate) {
        if (!candidate || matches(candidate) || active()) {
            return false;
        }
        id = candidate;
        revision = 0;
        used = 0;
        generation = 0;
        phase = Phase::loading;
        return true;
    }
    bool commit(uint32_t candidate, uint32_t expected, uint32_t current) {
        if (!matches(candidate) || phase != Phase::submitted || expected != revision || current != revision) {
            return false;
        }
        phase = Phase::complete;
        return true;
    }
};
constexpr const char *name(Phase phase) {
    switch (phase) {
    case Phase::loading:
        return "loading";
    case Phase::editing:
        return "editing";
    case Phase::submitted:
        return "submitted";
    case Phase::complete:
        return "complete";
    case Phase::cancelled:
        return "cancelled";
    default:
        return "idle";
    }
}
} // namespace rme_tool_review
