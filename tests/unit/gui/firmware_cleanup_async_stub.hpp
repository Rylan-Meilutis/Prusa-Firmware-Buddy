#pragma once

#include <cassert>
#include <functional>

// Deterministic executor: ordinary fault tests run immediately; the watchdog
// regression holds a queued/running worker while repeatedly cycling Marlin.
class AsyncJobExecutionControl {};
inline bool defer_cleanup = false;
inline bool in_cleanup_worker = false;
inline unsigned cleanup_issues = 0;
class AsyncJob {
public:
    bool is_active() const { return active; }
    void issue(std::function<void(AsyncJobExecutionControl &)> cb) {
        assert(!active);
        ++cleanup_issues;
        active = true;
        callback = cb;
        pending = this;
        if (!defer_cleanup) {
            run();
        }
    }
    static void run() {
        assert(pending && !in_cleanup_worker);
        auto *job = pending;
        in_cleanup_worker = true;
        AsyncJobExecutionControl control;
        job->callback(control);
        in_cleanup_worker = false;
        job->active = false;
        pending = nullptr;
    }

private:
    bool active = false;
    std::function<void(AsyncJobExecutionControl &)> callback;
    static inline AsyncJob *pending = nullptr;
};
