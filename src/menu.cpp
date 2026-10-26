#include "menu.h"
#include "config.h"

#include <imgui.h>
#include <cstdio>

namespace menu {

static const char* keynames[] = {
    "none", "lmb", "rmb", "cancel", "mmb", "mouse4", "mouse5", "?",
    "backspace", "tab", "?", "?", "clear", "enter", "?", "?",
    "shift", "ctrl", "alt", "pause", "capslock",
    "?", "?", "?", "?", "?", "?",
    "esc", "?", "?", "?", "?",
    "space", "pgup", "pgdn", "end", "home",
    "left", "up", "right", "down",
    "?", "?", "?", "?",
    "insert", "delete", "help",
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    "?", "?", "?", "?", "?", "?", "?",
    "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m",
    "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
    "lwin", "rwin", "apps", "?", "sleep",
    "num0", "num1", "num2", "num3", "num4", "num5", "num6", "num7", "num8", "num9",
    "num*", "num+", "?", "num-", "num.", "num/",
    "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9", "f10", "f11", "f12"
};

static bool waitingtoggle = false;
static bool waitingmenu = false;
static bool wasdown[256]{};

static const char* keyname(int key) {
    if (key >= 0 && key < static_cast<int>(sizeof(keynames) / sizeof(keynames[0])))
        return keynames[key];
    return "?";
}

static void keybind(const char* label, int& key, bool& waiting) {
    char text[64];
    if (waiting)
        snprintf(text, sizeof(text), "[press a key]##%s", label);
    else
        snprintf(text, sizeof(text), "%s: %s", label, keyname(key));

    if (ImGui::Button(text, ImVec2(200, 0))) {
        waiting = true;
        for (int vk = 1; vk < 256; vk++)
            wasdown[vk] = GetAsyncKeyState(vk) & 0x8000;
    }

    if (!waiting)
        return;

    for (int vk = 1; vk < 256; vk++) {
        bool down = GetAsyncKeyState(vk) & 0x8000;
        bool pressed = down && !wasdown[vk];
        wasdown[vk] = down;

        if (!pressed || vk == VK_LBUTTON || vk == VK_RBUTTON)
            continue;

        if (vk != VK_ESCAPE)
            key = vk;
        waiting = false;
        break;
    }
}

void render() {
    if (!config.menuopen)
        return;

    ImGui::Begin("bhop", &config.menuopen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Checkbox("bhop toggle", &config.bhop);
    ImGui::Text("status: %s", config.bhop ? "on" : "off");
    keybind("toggle key", config.togglekey, waitingtoggle);
    keybind("menu key", config.menukey, waitingmenu);
    ImGui::End();
}

} // namespace menu
