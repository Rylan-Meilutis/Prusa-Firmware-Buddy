// Unittest stub

#pragma once

#include <functional>
#include <optional>

inline size_t executed_job_count = 0;

class AsyncJobBase;

class AsyncJobExecutionControl {
public:
    AsyncJobBase *job = nullptr;

    bool is_discarded();
};

class AsyncJobExecutor {

public:
    static constexpr int worker_count() {
        return 1;
    }
};

class AsyncJobBase {

public:
    enum class State {
        idle,
        queued,
        running,
        finished,
        cancelled
    };

    inline State state() const {
        return state_;
    }

    bool is_active() const {
        return (state_ == State::queued) || (state_ == State::running);
    }

    void discard() {
        state_ = State::idle;
    }

    bool was_discarded() {
        if (discard_check_callback) {
            discard_check_callback();
        }
        return was_discarded_;
    }

    // If set, marks the job as discarded after X checks
    std::optional<int> discard_after;

    bool was_discarded_ = false;

    size_t discard_check_count = 0;

    State state_ = State::idle;

    /// When set, the function is executed on each discard check
    std::function<void()> discard_check_callback;

protected:
    void issue(const std::function<void(AsyncJobExecutionControl &)> &f);
};

class AsyncJob : public AsyncJobBase {

public:
    using AsyncJobBase::issue;
};

template <typename T>
class AsyncJobWithResult : public AsyncJobBase {

public:
    void issue(const std::function<void(AsyncJobExecutionControl &, T &result)> &f) {
        AsyncJobBase::issue([this, &f](AsyncJobExecutionControl &control) {
            f(control, result_);
        });
    }

    const T &result() const {
        return result_;
    }

    T result_;
};
