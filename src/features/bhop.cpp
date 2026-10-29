#include "features/bhop.h"
#include "core/config.h"
#include "game/offsets.h"
#include "utils/pattern.h"
#include "game/subtick.h"
#include "game/trace.h"
#include "game/usercmd.h"

#include <MinHook.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <span>

namespace logicblop {

constexpr float grid = 1.0f / 64.0f;
constexpr float interval = 1.0f / 64.0f;
constexpr float standable = 0.7f;
constexpr int capacity = 32;

struct stamp {
    i64 tick = 0;
    float frac = 0.0f;

    bool operator==(const stamp&) const = default;
};

struct hull {
    vec3 mins;
    vec3 maxs;
};

struct motion {
    vec3 origin;
    vec3 velocity;
    hull box;
    float gravity = 0.0f;
};

struct frame {
    u32 command = 0;
    i64 tick = 0;
    bool readable = false;
    bool enabled = false;
    bool held = false;
    bool canmove = false;
    bool grounded = false;
    bool falling = false;
    float penalty = 0.0f;
    std::optional<stamp> pressed;
    std::optional<stamp> landed;
    std::optional<stamp> predicted;
};

struct nativestep {
    float when = 0.0f;
    bool jump = false;
};

struct state {
    std::optional<stamp> lastpress;
    std::optional<stamp> first;
    std::optional<stamp> followed;
};

struct decision {
    bool ownsjump = false;
    std::optional<float> when;
    state after;
};

struct step {
    int source = -1;
    float when = 0.0f;
    bool down = false;
};

struct batch {
    std::array<step, capacity> steps{};
    int count = 0;
    bool ownsjump = false;
    bool jumpdown = false;
};

static bool finite(const vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

static bool valid(const stamp& s) {
    return s.tick >= 0 && std::isfinite(s.frac) && s.frac >= 0.0f && s.frac < 1.0f;
}

static float quantize(float frac) {
    return std::round(frac * 64.0f) * grid;
}

static float firstwhen(float contact) {
    if (contact + grid <= 62.0f * grid)
        return std::min(contact + 3.0f * grid, 62.0f * grid);
    return std::min(contact - grid, 62.0f * grid);
}

static bool offcooldown(const stamp& now, const std::optional<stamp>& previous, float penalty) {
    if (!previous)
        return true;
    if (now.tick < previous->tick)
        return false;

    float elapsed = (static_cast<float>(now.tick - previous->tick) + now.frac - previous->frac) * interval;
    return elapsed > penalty;
}

static bool usable(const frame& f) {
    return f.readable && f.tick >= 0 && std::isfinite(f.penalty) && f.penalty >= 0.0f
        && (!f.pressed || valid(*f.pressed))
        && (!f.landed || valid(*f.landed));
}

template <class tracefn>
static std::optional<stamp> predict(i64 tick, const motion& m, tracefn&& tracer) {
    if (tick < 0 || !finite(m.origin) || !finite(m.velocity))
        return std::nullopt;

    vec3 velocity = m.velocity;
    velocity.z -= m.gravity * interval;
    vec3 end = m.origin + velocity * interval;
    if (!(velocity.z < 0.0f) || !finite(end))
        return std::nullopt;

    auto hit = tracer(m.origin, end, m.box);
    if (!hit || hit->startsolid || !finite(hit->normal))
        return std::nullopt;
    if (!(hit->fraction > 0.0f && hit->fraction < 1.0f) || hit->normal.z < standable)
        return std::nullopt;

    return stamp{tick, hit->fraction};
}

static decision decide(const state& current, const frame& f) {
    decision result{false, std::nullopt, current};
    if (!f.enabled || !f.held || !f.canmove)
        return result;
    result.ownsjump = true;

    enum class kind { press, first, followup };

    auto offer = [&](float when, kind k = kind::press) {
        stamp at{f.tick, when};
        if (!offcooldown(at, f.pressed, f.penalty) || !offcooldown(at, current.lastpress, f.penalty))
            return false;

        result.when = when;
        result.after.lastpress = at;
        if (k == kind::first)
            result.after.first = at;
        if (k == kind::followup)
            result.after.followed = f.landed;
        return true;
    };

    bool hasfirst = f.landed && current.first && current.first->tick == f.landed->tick;

    auto followup = [&] {
        float first = hasfirst ? current.first->frac : 0.0f;
        float when = std::clamp(quantize(std::max(f.landed->frac, first) + grid), grid, 63.0f * grid);
        return offer(when, kind::followup);
    };

    i64 age = f.landed ? f.tick - f.landed->tick : -1;

    if (f.landed) {
        float contact = quantize(f.landed->frac);
        if (age == 0 && offer(firstwhen(contact), kind::first))
            return result;

        if (f.grounded && age == 1) {
            if (!hasfirst && contact > 49.0f * grid && offer(grid, kind::followup))
                return result;
            if (followup())
                return result;
        }
    }

    if (!f.grounded && f.falling && f.predicted && valid(*f.predicted)) {
        float contact = quantize(f.predicted->frac);
        if (f.predicted->tick == f.tick && contact + grid <= 62.0f * grid && offer(firstwhen(contact), kind::first))
            return result;
    }

    if (f.grounded && !(age >= 0 && age <= 1) && offer(0.0f))
        return result;

    if (f.grounded && age >= 2 && age <= 5 && current.followed != f.landed)
        followup();

    return result;
}

static bool merge(std::span<const nativestep> native, const decision& d, batch& out) {
    batch b;
    b.ownsjump = d.ownsjump;
    b.jumpdown = d.when.has_value();

    int count = static_cast<int>(native.size());
    if (count > capacity)
        return false;

    for (int i = 0; i < count; i++) {
        const nativestep& s = native[i];
        if (!std::isfinite(s.when) || s.when < 0.0f || s.when > 1.0f)
            return false;
        if (d.ownsjump && s.jump)
            continue;
        b.steps[b.count++] = {i, s.when, false};
    }

    if (d.when) {
        if (b.count + 2 > capacity)
            return false;
        b.steps[b.count++] = {-1, *d.when, false};
        b.steps[b.count++] = {-1, *d.when, true};
    }

    for (int i = 1; i < b.count; i++) {
        step value = b.steps[i];
        int j = i;
        while (j > 0 && b.steps[j - 1].when > value.when) {
            b.steps[j] = b.steps[j - 1];
            j--;
        }
        b.steps[j] = value;
    }

    out = b;
    return true;
}

class controller {
public:
    void reset() {
        current = {};
        command.reset();
    }

    template <class tracefn, class writefn>
    void run(frame f, const motion& m, std::span<const nativestep> native, tracefn&& tracer, writefn&& writer) {
        if (command && f.command < *command) {
            reset();
            return;
        }

        if (!command || f.command != *command) {
            if (!usable(f))
                return;
            if (f.enabled && f.held && f.canmove && !f.grounded && f.falling)
                f.predicted = predict(f.tick, m, tracer);
            command = f.command;
            pending = decide(current, f);
        }

        batch b;
        if (!merge(native, pending, b) || !writer(b))
            return;

        current = pending.after;
    }

private:
    state current;
    decision pending;
    std::optional<u32> command;
};

} // namespace logicblop

namespace bhop {

constexpr int onground = 1 << 0;
constexpr int ducking = 1 << 1;
constexpr u8 walking = 2;

using createmovefn = void(*)(void* input, int slot, usercmd* cmd);

static createmovefn original = nullptr;
static u64 client = 0;
static HWND window = nullptr;
static u64 lastpawn = 0;
static logicblop::controller jumper;

template <class t>
static t read(u64 address) {
    if (IsBadReadPtr(reinterpret_cast<void*>(address), sizeof(t)))
        return t{};
    return *reinterpret_cast<t*>(address);
}

static bool focused() {
    return !window || GetForegroundWindow() == window;
}

static std::optional<logicblop::stamp> readstamp(u64 services, u64 tickoffset, u64 fracoffset) {
    int tick = read<int>(services + tickoffset);
    float frac = read<float>(services + fracoffset);
    if (tick <= 0 || !std::isfinite(frac))
        return std::nullopt;

    if (frac >= 1.0f) {
        tick++;
        frac -= 1.0f;
    }
    return logicblop::stamp{tick, frac};
}

static void run(usercmd* cmd) {
    if (!subtick::ready())
        return;

    basecmd* base = getbase(cmd);
    u64 pawn = read<u64>(client + offsets::pawnoffset());
    if (pawn != lastpawn) {
        jumper.reset();
        lastpawn = pawn;
    }
    if (!base || !pawn)
        return;

    u64 services = read<u64>(pawn + offsets::pawn::movementservices);
    u64 node = read<u64>(pawn + offsets::pawn::scenenode);
    int flags = read<int>(pawn + offsets::pawn::flags);
    vec3 velocity = read<vec3>(pawn + offsets::pawn::velocity);
    vec3 origin = node ? read<vec3>(node + offsets::node::origin) : vec3{};

    logicblop::frame f;
    f.command = cmd->number;
    f.tick = base->clienttick;
    f.readable = services && node;
    f.enabled = config.bhop && focused();
    f.held = GetAsyncKeyState(VK_SPACE) & 0x8000;
    f.canmove = read<u8>(pawn + offsets::pawn::movetype) == walking;
    f.grounded = flags & onground;
    f.falling = velocity.z < 0.0f;
    f.penalty = offsets::defaults::jumppenalty;
    if (services) {
        f.pressed = readstamp(services, offsets::movement::presstick, offsets::movement::pressfrac);
        f.landed = readstamp(services, offsets::movement::landedtick, offsets::movement::landedfrac);
    }

    float scale = read<float>(pawn + offsets::pawn::gravityscale);
    if (!std::isfinite(scale) || scale <= 0.0f)
        scale = 1.0f;

    logicblop::motion m;
    m.origin = origin;
    m.velocity = velocity;
    m.box = {{-16.0f, -16.0f, 0.0f}, {16.0f, 16.0f, (flags & ducking) ? 54.0f : 72.0f}};
    m.gravity = offsets::defaults::gravity * scale;

    subtick::entry entries[subtick::maxsteps];
    int count = subtick::read(base, entries, subtick::maxsteps);
    if (count < 0)
        return;

    logicblop::nativestep native[subtick::maxsteps];
    for (int i = 0; i < count; i++)
        native[i] = {entries[i].when, entries[i].button == injump};

    auto tracer = [&](const vec3& start, const vec3& end, const logicblop::hull& box) {
        return trace::worldbox(pawn, start, end, box.mins, box.maxs);
    };

    auto writer = [&](const logicblop::batch& b) {
        if (!b.ownsjump)
            return true;

        subtick::step steps[subtick::maxsteps];
        for (int i = 0; i < b.count; i++) {
            const logicblop::step& s = b.steps[i];
            if (s.source >= 0)
                steps[i] = {s.source};
            else
                steps[i] = {-1, injump, s.down, s.when};
        }

        return subtick::rewrite(base, steps, b.count)
            && subtick::setbuttons(cmd, base, injump, b.jumpdown ? injump : 0);
    };

    jumper.run(f, m, std::span<const logicblop::nativestep>(native, count), tracer, writer);
}

static void hookedcreatemove(void* input, int slot, usercmd* cmd) {
    original(input, slot, cmd);
    run(cmd);
}

void setwindow(HWND hwnd) {
    window = hwnd;
}

bool init() {
    HMODULE dll = nullptr;
    for (int i = 0; i < 300 && !dll; i++) {
        dll = GetModuleHandleA("client.dll");
        if (!dll)
            Sleep(100);
    }
    if (!dll)
        return false;

    client = reinterpret_cast<u64>(dll);
    offsets::pawnoffset();
    trace::init();
    subtick::init();

    u64 target = findpattern(dll, patterns::createmove);
    if (!target)
        return false;

    if (MH_CreateHook(reinterpret_cast<void*>(target), reinterpret_cast<void*>(&hookedcreatemove),
            reinterpret_cast<void**>(&original)) != MH_OK)
        return false;

    return MH_EnableHook(reinterpret_cast<void*>(target)) == MH_OK;
}

} // namespace bhop
