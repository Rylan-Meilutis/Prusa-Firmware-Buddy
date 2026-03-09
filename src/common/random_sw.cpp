/// @file
/// @brief Software implementation of random.h

#include <random/random.h>
#include <random_sw/random_sw.h>

#include <stdlib.h>

RAND_DECL uint32_t rand_u() {
    return rand_u_sw();
}

// Do not implement this function - we cannot provide secure rand with just SW implementation
// RAND_DECL bool rand_u_secure(uint32_t *out) {}
