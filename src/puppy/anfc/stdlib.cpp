#include <sys/reent.h>
#include <sys/stat.h>

#include "hal.hpp"

extern "C" {

void __assert_func(const char *, int, const char *, const char *) {
    hal::panic();
}

#ifdef STM32H5
int _close_r(struct _reent *, int) {
    hal::panic();
}

_off_t _lseek_r(struct _reent *, int, _off_t, int) {
    hal::panic();
}

_ssize_t _read_r(struct _reent *, int, void *, size_t) {
    hal::panic();
}

_ssize_t _write_r(struct _reent *, int, const void *, size_t) {
    return 0;
}

int __attribute__((used)) _kill_r(struct _reent *, int, int) {
    hal::panic();
}

int __attribute__((used)) _getpid_r(struct _reent *) {
    return -1;
}

int _fstat(int, struct stat *) {
    hal::panic();
}

int _isatty(int) {
    hal::panic();
}

#elifdef STM32C0
int _close(struct _reent *, int) {
    hal::panic();
}

_off_t _lseek(struct _reent *, int, _off_t, int) {
    hal::panic();
}

_ssize_t _read(struct _reent *, int, void *, size_t) {
    hal::panic();
}

_ssize_t _write(struct _reent *, int, const void *, size_t) {
    hal::panic();
}

int __attribute__((used)) _getpid() {
    return -1;
}

int __attribute__((used)) _kill(int, int) {
    hal::panic();
}
#endif
}
