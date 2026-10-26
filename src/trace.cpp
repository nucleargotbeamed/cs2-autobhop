#include "trace.h"
#include "pattern.h"

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

    u64 address = findpattern(client, "48 8b 0d ? ? ? ? ? ? ? ? 48 03 f6");
    if (address)
        manager = resolverip(address, 3, 7);

    traceshape = reinterpret_cast<traceshapefn>(findpattern(client,
        "48 89 54 24 ? 48 89 4c 24 ? 55 53 56 57 41 54 41 56 41 57 48 8d ac 24 ? ? ? ? b8"));

    initfilter = reinterpret_cast<initfilterfn>(findpattern(client,
        "48 89 5c 24 ? 48 89 74 24 ? 57 48 83 ec ? 0f b6 41 ? 33 ff 24"));

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
