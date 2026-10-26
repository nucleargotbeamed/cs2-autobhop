#pragma once

#include <cmath>

using u8 = unsigned char;
using u32 = unsigned int;
using u64 = unsigned long long;
using i64 = long long;

struct vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    vec3 operator+(const vec3& other) const {
        return {x + other.x, y + other.y, z + other.z};
    }

    vec3 operator*(float scale) const {
        return {x * scale, y * scale, z * scale};
    }
};
