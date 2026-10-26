#pragma once

#include "types.h"

namespace offsets {

namespace client {
constexpr u64 localpawn = 0x25676d8;
} // namespace client

namespace pawn {
constexpr u64 scenenode = 0x330;
constexpr u64 flags = 0x3f4;
constexpr u64 velocity = 0x430;
constexpr u64 movetype = 0x525;
constexpr u64 gravityscale = 0x564;
constexpr u64 movementservices = 0x1330;
} // namespace pawn

namespace node {
constexpr u64 origin = 0xc8;
} // namespace node

namespace movement {
constexpr u64 presstick = 0x6c8 + 0x10;
constexpr u64 pressfrac = 0x6c8 + 0x14;
constexpr u64 landedtick = 0x6c8 + 0x20;
constexpr u64 landedfrac = 0x6c8 + 0x24;
} // namespace movement

} // namespace offsets
