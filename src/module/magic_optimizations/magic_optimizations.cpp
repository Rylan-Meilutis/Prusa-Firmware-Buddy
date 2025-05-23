#include <cstdlib>
#include <bits/functexcept.h>

#if __GLIBCXX__ != 20231009
    #error Please check that this necromancy still works
#endif

// Magically (potentially) save several kB of flash and hundreds of bytes of ram
// The default __throw_system_error implementation brings in std::error_category,
// which brings in std::string (through std::error_category::message)
// Thanks to a basic implementation, the linker can throw all that away if you don't use it elsewhere
// __throw_system_error is marked as noreturn, so we are not removing anything important
void std::__throw_system_error(int) {
    std::abort();
}
