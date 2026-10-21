#pragma once

namespace firmware_update_handoff {

enum class ReceiptState { missing,
    valid,
    error };

// Independent, non-empty handoff receipt. Unlike staging metadata, this is
// published only by an explicitly authorized M997 operation.
ReceiptState cleanup_receipt_state();
bool persist_cleanup_receipt();
bool remove_cleanup_receipt();
// Must succeed before publishing a newly staged image. An old authorization
// must never carry over to a replacement candidate.
bool clear_cleanup_authorization();

} // namespace firmware_update_handoff
