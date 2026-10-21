#include "firmware_cleanup_receipt.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace firmware_update_handoff {
namespace {
    constexpr const char *receipt_path = "/usb/FWUPD.CLN";
    constexpr const char *temporary_path = "/usb/FWUPD.TCL";
    constexpr char receipt_magic[] = "RME flash handoff v1\n";

    bool remove_if_present(const char *path) {
        errno = 0;
        return remove(path) == 0 || errno == ENOENT;
    }
} // namespace

ReceiptState cleanup_receipt_state() {
    FILE *file = fopen(receipt_path, "rb");
    if (!file) {
        return errno == ENOENT ? ReceiptState::missing : ReceiptState::error;
    }
    setvbuf(file, nullptr, _IONBF, 0);
    char contents[sizeof(receipt_magic)] {};
    const size_t count = fread(contents, 1, sizeof(contents), file);
    const bool valid = !ferror(file) && count == sizeof(receipt_magic) - 1
        && memcmp(contents, receipt_magic, sizeof(receipt_magic) - 1) == 0;
    const bool closed = fclose(file) == 0;
    return valid && closed ? ReceiptState::valid : ReceiptState::error;
}

bool persist_cleanup_receipt() {
    if (!remove_if_present(temporary_path)) {
        return false;
    }
    FILE *file = fopen(temporary_path, "wb");
    if (!file) {
        return false;
    }
    setvbuf(file, nullptr, _IONBF, 0);
    bool ok = fwrite(receipt_magic, 1, sizeof(receipt_magic) - 1, file) == sizeof(receipt_magic) - 1
        && fflush(file) == 0 && fsync(fileno(file)) == 0;
    ok = fclose(file) == 0 && ok;
    if (ok) {
        ok = rename(temporary_path, receipt_path) == 0;
    }
    if (!ok) {
        remove_if_present(temporary_path);
    }
    return ok;
}

bool remove_cleanup_receipt() {
    const bool temporary_removed = remove_if_present(temporary_path);
    // The receipt is the last durable authorization. Preserve it when a
    // temporary-file unlink fails so the server can retry the whole cleanup.
    return temporary_removed && remove_if_present(receipt_path);
}

bool clear_cleanup_authorization() {
    const bool receipt_removed = remove_cleanup_receipt();
    const bool marker_removed = remove_if_present("/usb/FWUPD.UI");
    const bool temporary_removed = remove_if_present("/usb/FWUPD.UI.tmp");
    return receipt_removed && marker_removed && temporary_removed;
}

} // namespace firmware_update_handoff
