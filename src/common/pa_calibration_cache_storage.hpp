#pragma once

#include "pa_calibration_cache.hpp"
#include <cstdio>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

namespace buddy::pa_cache {
struct FileRecord {
    Record record {};
    uint32_t checksum = 0;
};

template <typename T>
inline uint32_t checksum(const T &record) {
    uint32_t crc = 0xffffffff;
    const auto *bytes = reinterpret_cast<const uint8_t *>(&record);
    for (size_t i = 0; i < sizeof(record); ++i) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

inline void path_for(char (&path)[48], uint8_t slot, bool temporary = false) {
    // Keep the original path so a new-format record replaces the old one
    // instead of accumulating obsolete files. Record::version gates reads.
    snprintf(path, sizeof(path), "/internal/pa-cache-v1-%u.%s", unsigned(slot), temporary ? "tmp" : "bin");
}

inline bool read_record(const char *path, Record &record, const char **reason = nullptr) {
    const auto fail = [&](const char *why) {
        if (reason) {
            *reason = why;
        }
        return false;
    };
    const int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return fail(errno == ENOENT ? "missing_record" : "record_open_failed");
    }
    FileRecord file {};
    const auto size = read(fd, &file, sizeof(file));
    char extra;
    const bool exact_size = size == sizeof(file) && read(fd, &extra, 1) == 0;
    close(fd);
    if (!exact_size) {
        return fail("record_size_or_read_error");
    }
    if (file.record.version != record_version) {
        return fail("record_version_changed");
    }
    if (file.checksum != checksum(file.record)) {
        return fail("record_checksum_failed");
    }
    record = file.record;
    if (reason) {
        *reason = nullptr;
    }
    return true;
}

inline bool write_record(const char *path, const char *temporary, const Record &record) {
    FileRecord file {};
    file.record = record;
    file.checksum = checksum(file.record);
    const int fd = open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        return false;
    }
    const bool written = write(fd, &file, sizeof(file)) == sizeof(file);
    const bool closed = close(fd) == 0; // littlefs close commits file data
    if (!written || !closed || rename(temporary, path) != 0) {
        unlink(temporary);
        return false;
    }
    return true;
}

struct FileBank {
    Bank bank {};
    uint32_t checksum = 0;
};

inline bool read_bank(const char *path, Bank &bank, const char **reason = nullptr) {
    const auto fail = [&](const char *why) {
        if (reason) {
            *reason = why;
        }
        return false;
    };
    const int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return fail(errno == ENOENT ? "missing_record" : "record_open_failed");
    }
    // Foreground-only flash I/O; do not put a bank on Marlin's small stack.
    static FileBank file;
    file = {};
    const auto size = read(fd, &file, sizeof(file));
    char extra;
    const bool exact_size = size == sizeof(file) && read(fd, &extra, 1) == 0;
    close(fd);
    if (!exact_size) {
        // Preserve accepted single-temperature v3 caches across this upgrade.
        Record legacy {};
        if (size == sizeof(FileRecord) && read_record(path, legacy, reason)) {
            bank = {};
            bank.records[0] = legacy;
            bank.count = 1;
            return true;
        }
        return fail("record_size_or_read_error");
    }
    if (file.bank.version != 1) {
        return fail("record_version_changed");
    }
    if (file.checksum != checksum(file.bank)) {
        return fail("record_checksum_failed");
    }
    if (!file.bank.count || file.bank.count > file.bank.records.size()) {
        return fail("invalid_point_count");
    }
    for (size_t i = 0; i < file.bank.count; ++i) {
        if (!file.bank.records[i].matches(file.bank.records[i].key)) {
            return fail("invalid_cached_point");
        }
        auto identity = file.bank.records[i].key;
        identity.temperature = file.bank.records[0].key.temperature;
        if (!file.bank.records[0].matches(identity)) {
            return fail("mixed_cached_identity");
        }
        for (size_t j = 0; j < i; ++j) {
            if (file.bank.records[j].key.temperature == file.bank.records[i].key.temperature) {
                return fail("duplicate_cached_temperature");
            }
        }
    }
    bank = file.bank;
    if (reason) {
        *reason = nullptr;
    }
    return true;
}

inline bool write_bank(const char *path, const char *temporary, const Bank &bank) {
    static FileBank file;
    file = {};
    file.bank = bank;
    file.checksum = checksum(file.bank);
    const int fd = open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        return false;
    }
    const bool written = write(fd, &file, sizeof(file)) == sizeof(file);
    const bool closed = close(fd) == 0;
    if (!written || !closed || rename(temporary, path) != 0) {
        unlink(temporary);
        return false;
    }
    return true;
}

inline void invalidate(uint8_t slot) {
    char path[48];
    path_for(path, slot);
    unlink(path);
}
} // namespace buddy::pa_cache
