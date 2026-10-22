#pragma once
#include <mutex>
namespace buddy {
// Host counterpart of single-core interrupt/task exclusion, including tests
// that interleave the loadcell producer with capture stop/release.
class InterruptDisabler {
    inline static std::recursive_mutex mutex;
    std::lock_guard<std::recursive_mutex> guard { mutex };
};
} // namespace buddy
