#pragma once

#include <cstdio>
#include <cstdint>
#include <sys/stat.h>
#include <unistd.h>

// RME filesystem operations are serialized by the Marlin task, but the FAT
// operation itself must not block that task's temperature/watchdog updates.
namespace rme_file_io {

enum class Operation : uint8_t { open,
    close,
    read,
    write,
    seek,
    flush,
    flush_sync,
    stat,
    remove,
    rename,
    mkdir,
    read_chunk,
    write_metadata,
    prepare_directory,
    clear_cleanup };

struct ReadChunk {
    const char *path;
    uint32_t offset;
    void *data;
    size_t length;
    uint32_t file_size = 0;
    size_t count = 0;
};

struct MetadataWrite {
    const char *temporary;
    const char *destination;
    const void *data;
    size_t length;
};

// One small dispatcher avoids a separate AsyncJob/callback instantiation for
// every libc call, preserving the flash budget of the MINI as well as xBuddy.
// Arguments/results are pointer-width scalars; no function-pointer ABI casts.
[[gnu::noinline, gnu::noclone]] intptr_t run(Operation operation, uintptr_t a, uintptr_t b = 0, uintptr_t c = 0);

inline bool read_chunk(ReadChunk &chunk) {
    return run(Operation::read_chunk, reinterpret_cast<uintptr_t>(&chunk)) == 0;
}
inline bool write_metadata(const char *temporary, const char *destination, const void *data, size_t length) {
    MetadataWrite metadata { temporary, destination, data, length };
    return run(Operation::write_metadata, reinterpret_cast<uintptr_t>(&metadata)) == 0;
}
inline bool prepare_directory(const char *path) {
    return run(Operation::prepare_directory, reinterpret_cast<uintptr_t>(path)) == 0;
}
inline bool clear_cleanup() {
    return run(Operation::clear_cleanup, 0) == 0;
}

inline FILE *fopen(const char *path, const char *mode) {
    return reinterpret_cast<FILE *>(run(Operation::open, reinterpret_cast<uintptr_t>(path), reinterpret_cast<uintptr_t>(mode)));
}
inline int fclose(FILE *file) {
    return run(Operation::close, reinterpret_cast<uintptr_t>(file));
}
// Transfers always use byte-sized elements. Four ARM registers can hold the
// whole request, with no outgoing stack argument at every filesystem call.
inline size_t fread(void *data, size_t count, FILE *file) {
    return run(Operation::read, reinterpret_cast<uintptr_t>(data), count, reinterpret_cast<uintptr_t>(file));
}
inline size_t fwrite(const void *data, size_t count, FILE *file) {
    return run(Operation::write, reinterpret_cast<uintptr_t>(data), count, reinterpret_cast<uintptr_t>(file));
}
inline int fseek(FILE *file, long offset, int origin) {
    return run(Operation::seek, reinterpret_cast<uintptr_t>(file), static_cast<uintptr_t>(offset), static_cast<uintptr_t>(origin));
}
inline int fflush(FILE *file) {
    return run(Operation::flush, reinterpret_cast<uintptr_t>(file));
}
inline int flush_sync(FILE *file) {
    return run(Operation::flush_sync, reinterpret_cast<uintptr_t>(file));
}
inline int stat(const char *path, struct stat *info) {
    return run(Operation::stat, reinterpret_cast<uintptr_t>(path), reinterpret_cast<uintptr_t>(info));
}
inline int remove(const char *path) {
    return run(Operation::remove, reinterpret_cast<uintptr_t>(path));
}
inline int rename(const char *source, const char *destination) {
    return run(Operation::rename, reinterpret_cast<uintptr_t>(source), reinterpret_cast<uintptr_t>(destination));
}
inline int mkdir(const char *path, mode_t mode) {
    return run(Operation::mkdir, reinterpret_cast<uintptr_t>(path), static_cast<uintptr_t>(mode));
}

} // namespace rme_file_io
