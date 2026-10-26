#pragma once

#include "types.h"

#include <Windows.h>
#include <limits>

constexpr u64 injump = 0x2;

template <class t>
struct repeatedfield {
    struct storage {
        int allocated;
        t* elements[(std::numeric_limits<int>::max() - 2 * sizeof(int)) / sizeof(void*)];
    };

    void* arena;
    int size;
    int capacity;
    storage* rep;
};

struct protobuf {
    void* vtable;
    void* metadata;
};

struct buttonpb : protobuf {
    u32 hasbits;
    u32 cachedsize;
    u64 state1;
    u64 state2;
    u64 state3;
};

enum stepbits : u32 {
    stepbutton = 0x1,
    steppressed = 0x2,
    stepwhen = 0x4,
};

struct subtickstep : protobuf {
    u32 hasbits;
    u32 cachedsize;
    u64 button;
    bool pressed;
    float when;
    float forward;
    float left;
    float pitch;
    float yaw;
};

struct basecmd : protobuf {
    u32 hasbits;
    u32 cachedsize;
    repeatedfield<subtickstep> moves;
    void* movecrc;
    buttonpb* buttons;
    void* viewangles;
    void* notes;
    int commandnumber;
    int clienttick;
    float forwardmove;
    float leftmove;
    float upmove;
    int impulse;
    int weaponselect;
    int randomseed;
    int mousedx;
    int mousedy;
    u32 predictionoffset;
    u32 serverangles;
    int cmdflags;
    u32 pawnhandle;
};

struct csgocmd : protobuf {
    u32 hasbits;
    u32 cachedsize;
    repeatedfield<void> history;
    basecmd* base;
    bool lefthand;
    bool bodyshot;
    bool headshot;
    bool ragdolls;
    int attack1index;
    int attack2index;
};

struct usercmd {
    void* vtable;
    u32 number;
    csgocmd pb;
    void* buttonsvtable;
    u64 buttons;
    u64 changed;
};

inline basecmd* getbase(usercmd* cmd) {
    if (!cmd || IsBadReadPtr(cmd, sizeof(usercmd)))
        return nullptr;

    basecmd* base = cmd->pb.base;
    if (!base || IsBadReadPtr(base, sizeof(basecmd)))
        return nullptr;

    return base;
}
