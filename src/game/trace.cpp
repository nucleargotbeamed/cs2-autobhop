#include "game/trace.h"
#include "game/offsets.h"
#include "utils/pattern.h"

#include <cstring>

namespace trace {

struct alignas(16) ray {
    vec3 mins;
    vec3 maxs;
    u8 pad[0x10];
    u8 type;
    u8 extra[0x40];
};

struct alignas(16) filter {
    u8 data[0x200];
};

struct gametrace {
    void* surface;
    void* entity;
    void* hitbox;
    u8 pad0[0x60];
    vec3 start;
    vec3 end;
    vec3 normal;
    vec3 position;
    u8 pad1[0x4];
    float fraction;
    u8 pad2[0xb];
    bool startsolid;
    u8 pad3[0x54];
    u8 extra[0x100];
};

using traceshapefn = bool(*)(void* manager, ray* r, const vec3* start, const vec3* end, filter* f, gametrace* out);
using initfilterfn = void*(*)(void* f, void* skip, u64 mask, u8 layer, u8 flags);

static u64 manager = 0;
static traceshapefn traceshape = nullptr;
static initfilterfn initfilter = nullptr;

constexpr u64 worldmask = 0x3011;

bool init() {
    HMODULE client = GetModuleHandleA("client.dll");
    if (!client)
        return false;

    u64 address = findpattern(client, patterns::tracemanager);
    if (address)
        manager = resolverip(address, 3, 7);

    traceshape = reinterpret_cast<traceshapefn>(findpattern(client, patterns::traceshape));
    initfilter = reinterpret_cast<initfilterfn>(findpattern(client, patterns::initfilter));

    return ready();
}

bool ready() {
    return manager && traceshape && initfilter;
}

static bool traceray(u64 pawn, const vec3& start, const vec3& end, const vec3& mins, const vec3& maxs, gametrace* out) {
    __try {
        void* mgr = *reinterpret_cast<void**>(manager);
        if (!mgr)
            return false;

        filter f{};
        initfilter(&f, reinterpret_cast<void*>(pawn), worldmask, 4, 11);

        ray r{};
        r.mins = mins;
        r.maxs = maxs;
        r.type = 2;

        traceshape(mgr, &r, &start, &end, &f, out);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

std::optional<hit> worldbox(u64 pawn, const vec3& start, const vec3& end, const vec3& mins, const vec3& maxs) {
    if (!ready() || !pawn)
        return std::nullopt;

    static gametrace result;
    memset(&result, 0, sizeof(result));
    if (!traceray(pawn, start, end, mins, maxs, &result))
        return std::nullopt;

    return hit{result.fraction, result.normal, result.startsolid};
}

} // namespace trace
