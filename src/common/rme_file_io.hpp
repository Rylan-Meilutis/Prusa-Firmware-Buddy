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
    sync,
    stat,
    remove,
    rename,
    mkdir };

// One small dispatcher avoids a separate AsyncJob/callback instantiation for
// every libc call, preserving the flash budget of the MINI as well as xBuddy.
// Arguments/results are pointer-width scalars; no function-pointer ABI casts.
[[gnu::noinline, gnu::noclone]] intptr_t run(Operation operation, uintptr_t a, uintptr_t b = 0, uintptr_t c = 0, uintptr_t d = 0);

inline FILE *fopen(const char *path, const char *mode) {
    return reinterpret_cast<FILE *>(run(Operation::open, reinterpret_cast<uintptr_t>(path), reinterpret_cast<uintptr_t>(mode)));
}
inline int fclose(FILE *file) {
    return run(Operation::close, reinterpret_cast<uintptr_t>(file));
}
inline size_t fread(void *data, size_t size, size_t count, FILE *file) {
    return run(Operation::read, reinterpret_cast<uintptr_t>(data), size, count, reinterpret_cast<uintptr_t>(file));
}
inline size_t fwrite(const void *data, size_t size, size_t count, FILE *file) {
    return run(Operation::write, reinterpret_cast<uintptr_t>(data), size, count, reinterpret_cast<uintptr_t>(file));
}
inline int fseek(FILE *file, long offset, int origin) {
    return run(Operation::seek, reinterpret_cast<uintptr_t>(file), static_cast<uintptr_t>(offset), static_cast<uintptr_t>(origin));
}
inline int fflush(FILE *file) {
    return run(Operation::flush, reinterpret_cast<uintptr_t>(file));
}
inline int fsync(int descriptor) {
    return run(Operation::sync, static_cast<uintptr_t>(descriptor));
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
