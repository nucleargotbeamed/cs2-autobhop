#pragma once

#include <Windows.h>

struct settings {
    bool bhop = false;
    int togglekey = VK_F1;
    int menukey = VK_INSERT;
    bool menuopen = true;
};

inline settings config;
