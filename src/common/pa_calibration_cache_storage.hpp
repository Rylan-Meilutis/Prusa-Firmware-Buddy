#pragma once

#include "pa_calibration_cache.hpp"
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

namespace buddy::pa_cache {
struct FileRecord {
    Record record {};
    uint32_t checksum = 0;
};

inline uint32_t checksum(const Record &record) {
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

inline bool read_record(const char *path, Record &record) {
    const int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return false;
    }
    FileRecord file {};
    const auto size = read(fd, &file, sizeof(file));
    char extra;
    const bool exact_size = size == sizeof(file) && read(fd, &extra, 1) == 0;
    close(fd);
    if (!exact_size || file.record.version != record_version || file.checksum != checksum(file.record)) {
        return false;
    }
    record = file.record;
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

inline void invalidate(uint8_t slot) {
    char path[48];
    path_for(path, slot);
    unlink(path);
}
} // namespace buddy::pa_cache
