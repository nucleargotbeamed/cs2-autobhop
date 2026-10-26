#include "hooks.h"
#include "bhop.h"
#include "config.h"
#include "menu.h"

#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <MinHook.h>

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

namespace hooks {

using presentfn = HRESULT(WINAPI*)(IDXGISwapChain* swapchain, UINT sync, UINT flags);

static presentfn originalpresent = nullptr;
static WNDPROC originalwndproc = nullptr;
static HWND window = nullptr;

static ID3D11Device* device = nullptr;
static ID3D11DeviceContext* context = nullptr;
static ID3D11RenderTargetView* target = nullptr;

static bool ready = false;

static void createtarget(IDXGISwapChain* swapchain) {
    ID3D11Texture2D* backbuffer = nullptr;
    if (SUCCEEDED(swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backbuffer)))) {
        device->CreateRenderTargetView(backbuffer, nullptr, &target);
        backbuffer->Release();
    }
}

static void releasetarget() {
    if (target) {
        target->Release();
        target = nullptr;
    }
}

static int pressedkey(UINT msg, WPARAM wparam) {
    switch (msg) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        return static_cast<int>(wparam);
    case WM_MBUTTONDOWN:
        return VK_MBUTTON;
    case WM_XBUTTONDOWN:
        return GET_XBUTTON_WPARAM(wparam) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2;
    default:
        return 0;
    }
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    int key = pressedkey(msg, wparam);

    if (key && key == config.menukey) {
        config.menuopen = !config.menuopen;
        return 0;
    }

    if (key && key == config.togglekey) {
        config.bhop = !config.bhop;
        return 0;
    }

    if (config.menuopen && ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
        return 0;

    return CallWindowProcA(originalwndproc, hwnd, msg, wparam, lparam);
}

static void setup(IDXGISwapChain* swapchain) {
    if (FAILED(swapchain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&device))))
        return;
    device->GetImmediateContext(&context);

    DXGI_SWAP_CHAIN_DESC desc;
    swapchain->GetDesc(&desc);
    window = desc.OutputWindow;
    bhop::setwindow(window);

    createtarget(swapchain);

    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    ImGui_ImplWin32_Init(window);
    ImGui_ImplDX11_Init(device, context);
    ImGui::StyleColorsDark();

    originalwndproc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrA(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(wndproc)));

    ready = true;
}

static HRESULT WINAPI present(IDXGISwapChain* swapchain, UINT sync, UINT flags) {
    if (!ready)
        setup(swapchain);

    if (ready) {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ImGui::GetIO().MouseDrawCursor = config.menuopen;

        menu::render();

        ImGui::EndFrame();
        ImGui::Render();
        context->OMSetRenderTargets(1, &target, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    return originalpresent(swapchain, sync, flags);
}

static void* findpresent() {
    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "bhopdummy";
    RegisterClassExA(&wc);

    HWND dummy = CreateWindowExA(0, wc.lpszClassName, "", WS_OVERLAPPED,
        0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);

    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferCount = 1;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = dummy;
    desc.SampleDesc.Count = 1;
    desc.Windowed = true;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    IDXGISwapChain* swapchain = nullptr;
    ID3D11Device* tempdevice = nullptr;
    D3D_FEATURE_LEVEL level;

    void* address = nullptr;
    if (SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            nullptr, 0, D3D11_SDK_VERSION, &desc, &swapchain, &tempdevice, &level, nullptr))) {
        void** vtable = *reinterpret_cast<void***>(swapchain);
        address = vtable[8];
    }

    if (swapchain)
        swapchain->Release();
    if (tempdevice)
        tempdevice->Release();
    DestroyWindow(dummy);
    UnregisterClassA(wc.lpszClassName, wc.hInstance);

    return address;
}

bool init() {
    void* address = findpresent();
    if (!address)
        return false;

    if (MH_CreateHook(address, reinterpret_cast<void*>(&present), reinterpret_cast<void**>(&originalpresent)) != MH_OK)
        return false;

    return MH_EnableHook(address) == MH_OK;
}

void shutdown() {
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    if (!ready)
        return;

    if (originalwndproc && window)
        SetWindowLongPtrA(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(originalwndproc));

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    releasetarget();
    if (context) {
        context->Release();
        context = nullptr;
    }
    if (device) {
        device->Release();
        device = nullptr;
    }
}

} // namespace hooks
