#pragma once

namespace buddy::serial_print_finalize_policy {

// "Done printing file" belongs to the media-print protocol. A serial host
// owns the streamed job lifecycle on both success and abort, and can still
// have numbered commands in flight when firmware finalization runs.
constexpr bool emit_file_printed_marker(bool was_serial_print) {
    return !was_serial_print;
}

} // namespace buddy::serial_print_finalize_policy
