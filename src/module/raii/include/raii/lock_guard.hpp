/// @file
#pragma once

#include <utils/uncopyable.hpp>

/// Like std::lock_guard but doesn't bring in 7kB of std::crap
/// This actually matters on some build targets.
template <class Mutex>
class LockGuard : Uncopyable {
private:
    Mutex &mutex;

public:
    explicit LockGuard(Mutex &mutex_)
        : mutex { mutex_ } { mutex.lock(); }
    ~LockGuard() { mutex.unlock(); }
};
