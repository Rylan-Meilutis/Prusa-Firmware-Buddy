#pragma once

#include <marlin_server_types/marlin_server_state.h>

namespace buddy::serial_print_finalize_policy {

constexpr bool is_result(marlin_server::State state) {
    return state == marlin_server::State::Finished || state == marlin_server::State::Aborted;
}

constexpr bool keep_result_screen(bool serial_job, marlin_server::State state) {
    return serial_job && is_result(state);
}

// "Done printing file" belongs to the media-print protocol. A serial host
// owns the streamed job lifecycle on both success and abort, and can still
// have numbered commands in flight when firmware finalization runs.
constexpr bool emit_file_printed_marker(bool was_serial_print) {
    return !was_serial_print;
}

} // namespace buddy::serial_print_finalize_policy
