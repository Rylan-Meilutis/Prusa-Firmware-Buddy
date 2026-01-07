#include "requests_base.hpp"

#include <logging/log.hpp>

LOG_COMPONENT_DEF(OpenPrintTag, logging::Severity::info);

namespace buddy::openprinttag {

#ifndef UNITTESTS
// Do mind sizeof(Request) greatly.
// The class will get allocated on the stack many times at the same time, so every byte counts.
static_assert(sizeof(Request) == 8);

// Do not implement for unittests, let them stub it
void Request::issue() {
    issued_ = true;
    // TODO implementation, this will possibly turn into virtual
}
#endif

void Request::set_finished(std::expected<std::monostate, Error> result) {
    assert(!finished_);
    finished_ = true;
    error_ = result.error_or(Error::_cnt);
}

} // namespace buddy::openprinttag
