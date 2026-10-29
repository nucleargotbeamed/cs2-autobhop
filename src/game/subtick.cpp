#include "game/subtick.h"
#include "game/offsets.h"
#include "utils/pattern.h"

namespace subtick {

using allocfn = subtickstep*(*)(void* arena);
using addfn = subtickstep*(*)(void* field, void* element);

static allocfn allocstep = nullptr;
static addfn addstep = nullptr;

struct saved {
    u32 hasbits;
    u64 button;
    bool pressed;
    float when;
    float forward;
    float left;
    float pitch;
    float yaw;
};

bool init() {
    HMODULE client = GetModuleHandleA("client.dll");
    if (!client)
        return false;

    u64 address = findpattern(client, patterns::subtick);
    if (address) {
        allocstep = reinterpret_cast<allocfn>(resolverip(address + 3, 1, 5));
        addstep = reinterpret_cast<addfn>(resolverip(address + 14, 1, 5));
    }
    return ready();
}

bool ready() {
    return allocstep && addstep;
}

int read(basecmd* base, entry* out, int cap) {
    __try {
        auto& moves = base->moves;
        int count = moves.size;
        if (count < 0 || count > cap)
            return -1;
        if (count > 0 && !moves.rep)
            return -1;

        for (int i = 0; i < count; i++) {
            subtickstep* s = moves.rep->elements[i];
            if (!s)
                return -1;
            out[i].button = s->button;
            out[i].when = s->when;
        }
        return count;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return -1;
    }
}

static subtickstep* nextstep(basecmd* base) {
    auto& moves = base->moves;
    if (moves.rep && moves.size < moves.rep->allocated)
        return moves.rep->elements[moves.size++];

    subtickstep* fresh = allocstep(moves.arena);
    if (!fresh)
        return nullptr;
    return addstep(&moves, fresh);
}

static bool writesteps(basecmd* base, const step* steps, int count) {
    auto& moves = base->moves;
    int existing = moves.size;
    if (existing < 0 || existing > maxsteps)
        return false;

    saved old[maxsteps];
    for (int i = 0; i < existing; i++) {
        subtickstep* s = moves.rep->elements[i];
        old[i] = {s->hasbits, s->button, s->pressed, s->when, s->forward, s->left, s->pitch, s->yaw};
    }

    moves.size = 0;
    for (int i = 0; i < count; i++) {
        subtickstep* s = nextstep(base);
        if (!s)
            return false;

        const step& in = steps[i];
        if (in.source >= 0) {
            if (in.source >= existing)
                return false;
            const saved& o = old[in.source];
            s->hasbits = o.hasbits;
            s->button = o.button;
            s->pressed = o.pressed;
            s->when = o.when;
            s->forward = o.forward;
            s->left = o.left;
            s->pitch = o.pitch;
            s->yaw = o.yaw;
        } else {
            s->hasbits = stepbutton | steppressed | stepwhen;
            s->button = in.button;
            s->pressed = in.pressed;
            s->when = in.when;
            s->forward = 0.0f;
            s->left = 0.0f;
            s->pitch = 0.0f;
            s->yaw = 0.0f;
        }
    }
    return true;
}

bool rewrite(basecmd* base, const step* steps, int count) {
    if (!ready() || !base || count > maxsteps)
        return false;

    __try {
        return writesteps(base, steps, count);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool setbuttons(usercmd* cmd, basecmd* base, u64 mask, u64 held) {
    __try {
        cmd->buttons = (cmd->buttons & ~mask) | (held & mask);
        cmd->changed |= mask;
        if (base->buttons) {
            base->buttons->state1 = (base->buttons->state1 & ~mask) | (held & mask);
            base->buttons->hasbits |= 0x1;
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

} // namespace subtick
