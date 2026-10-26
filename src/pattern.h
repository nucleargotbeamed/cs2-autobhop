#pragma once

#include "types.h"

#include <Windows.h>
#include <Psapi.h>
#include <cstdlib>

#pragma comment(lib, "psapi.lib")

inline u64 findpattern(HMODULE dll, const char* pattern) {
    MODULEINFO info{};
    GetModuleInformation(GetCurrentProcess(), dll, &info, sizeof(info));
    u8* base = static_cast<u8*>(info.lpBaseOfDll);
    DWORD size = info.SizeOfImage;

    u8 bytes[64]{};
    bool wildcard[64]{};
    int length = 0;

    for (const char* p = pattern; *p && length < 64;) {
        if (*p == ' ') {
            p++;
            continue;
        }
        if (*p == '?') {
            wildcard[length++] = true;
            p++;
            if (*p == '?')
                p++;
            continue;
        }
        char hex[3] = {p[0], p[1], 0};
        bytes[length++] = static_cast<u8>(strtoul(hex, nullptr, 16));
        p += 2;
    }

    for (DWORD i = 0; i + length <= size; i++) {
        bool found = true;
        for (int j = 0; j < length; j++) {
            if (!wildcard[j] && base[i + j] != bytes[j]) {
                found = false;
                break;
            }
        }
        if (found)
            return reinterpret_cast<u64>(base + i);
    }
    return 0;
}

inline u64 resolverip(u64 address, int offset, int length) {
    int relative = *reinterpret_cast<int*>(address + offset);
    return address + length + relative;
}
