#include "bhop.h"
#include "hooks.h"

#include <Windows.h>
#include <MinHook.h>

static HMODULE self = nullptr;

static DWORD WINAPI mainthread(LPVOID) {
    MH_Initialize();
    bhop::init();

    if (!hooks::init()) {
        hooks::shutdown();
        FreeLibraryAndExitThread(self, 1);
        return 1;
    }

    while (!(GetAsyncKeyState(VK_END) & 1))
        Sleep(50);

    hooks::shutdown();
    FreeLibraryAndExitThread(self, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        self = instance;

        HANDLE thread = CreateThread(nullptr, 0, mainthread, nullptr, 0, nullptr);
        if (thread)
            CloseHandle(thread);
    }
    return true;
}
