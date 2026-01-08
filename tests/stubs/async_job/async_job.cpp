#include "async_job.hpp"

void AsyncJobBase::issue(const std::function<void(AsyncJobExecutionControl &)> &f) {
    AsyncJobExecutionControl ctrl;
    ctrl.job = this;
    was_discarded_ = false;
    f(ctrl);
    executed_job_count++;
    state_ = State::finished;
    discard_after = {};
}

bool AsyncJobExecutionControl::is_discarded() {
    job->discard_check_count++;

    if (job->was_discarded_ || (job->discard_after && --(*job->discard_after) <= 0)) {
        job->was_discarded_ = true;
        return true;
    }

    return false;
}
