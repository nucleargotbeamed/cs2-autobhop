#pragma once

#include "utils/pattern.h"
#include "utils/types.h"

namespace patterns {
constexpr const char* createmove = "48 8b c4 4c 89 40 ? 48 89 48 ? 55 53 57";
constexpr const char* subtick = "48 8b 0f e8 ? ? ? ? 48 8b d0 48 8b cf e8 ? ? ? ? 48 8b d0";
constexpr const char* tracemanager = "48 8b 0d ? ? ? ? ? ? ? ? 48 03 f6";
constexpr const char* traceshape = "48 89 54 24 ? 48 89 4c 24 ? 55 53 56 57 41 54 41 56 41 57 48 8d ac 24 ? ? ? ? b8";
constexpr const char* initfilter = "48 89 5c 24 ? 48 89 74 24 ? 57 48 83 ec ? 0f b6 41 ? 33 ff 24";
constexpr const char* pawnbase = "48 8d 05 ? ? ? ? c3 cc cc cc cc cc cc cc cc 40 53 56 41 54";
constexpr const char* pawndelta = "4c 39 b6 ? ? ? ? 74 ? 44 88 be";
} // namespace patterns

namespace offsets {

namespace client {
constexpr u64 localpawn = 0x25698c8;
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

namespace defaults {
constexpr float gravity = 800.0f;
constexpr float jumppenalty = 0.015625f;
} // namespace defaults

inline u64 pawnoffset() {
    static const u64 resolved = [] {
        HMODULE dll = GetModuleHandleA("client.dll");
        if (!dll)
            return client::localpawn;
        u64 base = findpattern(dll, patterns::pawnbase);
        u64 hit = findpattern(dll, patterns::pawndelta);
        if (!base || !hit)
            return client::localpawn;
        u64 found = resolverip(base, 3, 7) - reinterpret_cast<u64>(dll) + *reinterpret_cast<int*>(hit + 3);
        return found < 0x10000000 ? found : client::localpawn;
    }();
    return resolved;
}

} // namespace offsets
