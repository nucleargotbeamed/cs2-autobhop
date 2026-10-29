#pragma once

#include "game/usercmd.h"

namespace subtick {

constexpr int maxsteps = 32;

struct entry {
    u64 button = 0;
    float when = 0.0f;
};

struct step {
    int source = -1;
    u64 button = 0;
    bool pressed = false;
    float when = 0.0f;
};

bool init();
bool ready();
int read(basecmd* base, entry* out, int cap);
bool rewrite(basecmd* base, const step* steps, int count);
bool setbuttons(usercmd* cmd, basecmd* base, u64 mask, u64 held);

} // namespace subtick
