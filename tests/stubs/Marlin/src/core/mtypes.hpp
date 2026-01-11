#pragma once

#include <core/types.h>

struct abce_pos_t {
    union {
        struct {
            float a, b, c, e;
        };
        float pos[4];
    };

    constexpr abce_pos_t()
        : a(0)
        , b(0)
        , c(0)
        , e(0) {}

    constexpr abce_pos_t(float a_in, float b_in, float c_in, float e_in)
        : a(a_in)
        , b(b_in)
        , c(c_in)
        , e(e_in) {}

    constexpr float &operator[](int n) { return pos[n]; }
    constexpr const float &operator[](int n) const { return pos[n]; }
};
