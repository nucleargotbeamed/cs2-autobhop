#pragma once

#include "utils/types.h"

#include <optional>

namespace trace {

struct hit {
    float fraction = 1.0f;
    vec3 normal;
    bool startsolid = false;
};

bool init();
bool ready();
std::optional<hit> worldbox(u64 pawn, const vec3& start, const vec3& end, const vec3& mins, const vec3& maxs);

} // namespace trace
