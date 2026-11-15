// The mod core, loaded by the proxy. Stage 1 of the hub's build order: our code runs inside
// the game, ticks once per rendered frame, and can read the local player's position.
//
// The frame hook patches the D3D9 device vtable, the way games/the-witness does it: a dummy
// device is created, its vtable is shared with the game's device, and slot 17 (Present) is
// replaced after the original pointer is saved. No code is overwritten, so there is no
// trampoline to get wrong, and the patch is visible to every device of the implementation.
#include <windows.h>

#include <d3d9.h>
#include <d3d11.h>
#include <dxgi1_2.h>

#include <cstdio>

#include "MinHook.h"

extern "C" void coop_note(const char* text);

namespace {

FILE* g_log = nullptr;
volatile LONG g_ticks = 0;
volatile LONG g_frames = 0;
volatile LONG g_frame_mark = 0;
DWORD g_frame_second = 0;
char g_notes[4096];
size_t g_notes_length = 0;

typedef HRESULT(WINAPI* PresentFn)(IDirect3DDevice9*, const RECT*, const RECT*, HWND,
                                   const RGNDATA*);
PresentFn g_present = nullptr;
PresentFn g_present_ex = nullptr;
typedef HRESULT(WINAPI* PresentExFn)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, DWORD,
                                     const RGNDATA*);
PresentExFn g_present_ex_slot = nullptr;
PresentFn g_present_chain = nullptr;

void note_frame();

HRESULT WINAPI swapchain_present_hook(IDirect3DSwapChain9* chain, const RECT* source,
                                      const RECT* dest, HWND window, const RGNDATA* dirty)
{
    note_frame();
    return g_present_chain(reinterpret_cast<IDirect3DDevice9*>(chain), source, dest, window, dirty);
}

void log_line(const char* text)
{
    if (g_log == nullptr) {
        return;
    }
    std::fputs(text, g_log);
    std::fputc('\n', g_log);
    std::fflush(g_log);
}

void log_value(const char* label, long long value)
{
    char line[160];
    std::snprintf(line, sizeof(line), "%s = %lld", label, value);
    log_line(line);
}

void note_frame()
{
    LONG frames = InterlockedIncrement(&g_frames);
    DWORD now = GetTickCount();
    if (g_frame_second == 0) {
        g_frame_second = now;
    } else if (now - g_frame_second >= 1000) {
        char line[96];
        std::snprintf(line, sizeof(line), "frames = %ld in %lu ms", frames - g_frame_mark,
                      static_cast<unsigned long>(now - g_frame_second));
        log_line(line);
        g_frame_mark = frames;
        g_frame_second = now;
    }
}

HRESULT WINAPI present_hook(IDirect3DDevice9* device, const RECT* source, const RECT* dest,
                            HWND window, const RGNDATA* dirty)
{
    note_frame();
    return g_present(device, source, dest, window, dirty);
}

HRESULT WINAPI present_hook_ex(IDirect3DDevice9Ex* device, const RECT* source, const RECT* dest,
                               HWND window, const RGNDATA* dirty)
{
    note_frame();
    return g_present_ex(reinterpret_cast<IDirect3DDevice9*>(device), source, dest, window, dirty);
}

HRESULT WINAPI present_ex_slot_hook(IDirect3DDevice9* device, const RECT* source,
                                    const RECT* dest, HWND window, DWORD flags,
                                    const RGNDATA* dirty)
{
    note_frame();
    return g_present_ex_slot(device, source, dest, window, flags, dirty);
}

void patch_swapchain(IDirect3DDevice9* device, const char* label)
{
    IDirect3DSwapChain9* chain = nullptr;
    if (device == nullptr || FAILED(device->GetSwapChain(0, &chain)) || chain == nullptr) {
        return;
    }
    void** vtable = *reinterpret_cast<void***>(chain);
    g_present_chain = reinterpret_cast<PresentFn>(vtable[3]);
    DWORD old_protect = 0;
    if (g_present_chain != nullptr &&
        VirtualProtect(&vtable[3], sizeof(void*), PAGE_READWRITE, &old_protect)) {
        vtable[3] = reinterpret_cast<void*>(&swapchain_present_hook);
        VirtualProtect(&vtable[3], sizeof(void*), old_protect, &old_protect);
        char line[96];
        std::snprintf(line, sizeof(line), "SwapChain Present hook installed (%s)", label);
        log_line(line);
    }
    chain->Release();
}

LRESULT CALLBACK dummy_window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    return DefWindowProcW(window, message, wparam, lparam);
}

bool install_present_hook()
{
    if (g_present != nullptr) {
        return true;
    }
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (d3d == nullptr) {
        log_line("Present hook: Direct3DCreate9 failed");
        return false;
    }
    WNDCLASSEXW klass = {};
    klass.cbSize = sizeof(klass);
    klass.lpfnWndProc = dummy_window_proc;
    klass.hInstance = GetModuleHandleW(nullptr);
    klass.lpszClassName = L"coop-mod-dummy";
    RegisterClassExW(&klass);
    HWND window = CreateWindowExW(0, klass.lpszClassName, L"coop", WS_POPUP, 0, 0, 8,
                                  8, nullptr, nullptr, klass.hInstance, nullptr);
    if (window == nullptr) {
        log_line("Present hook: dummy window failed");
        d3d->Release();
        return false;
    }
    D3DPRESENT_PARAMETERS params = {};
    params.Windowed = TRUE;
    params.SwapEffect = D3DSWAPEFFECT_DISCARD;
    params.BackBufferWidth = 8;
    params.BackBufferHeight = 8;
    params.BackBufferFormat = D3DFMT_X8R8G8B8;
    params.hDeviceWindow = window;
    IDirect3DDevice9* device = nullptr;
    HRESULT created = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
                                        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &params, &device);
    if (FAILED(created) || device == nullptr) {
        char line[96];
        std::snprintf(line, sizeof(line), "Present hook: CreateDevice failed 0x%08lx",
                      static_cast<unsigned long>(created));
        log_line(line);
        DestroyWindow(window);
        d3d->Release();
        return false;
    }
    void** vtable = *reinterpret_cast<void***>(device);
    g_present = reinterpret_cast<PresentFn>(vtable[17]);
    DWORD old_protect = 0;
    if (VirtualProtect(&vtable[17], sizeof(void*), PAGE_READWRITE, &old_protect)) {
        vtable[17] = reinterpret_cast<void*>(&present_hook);
        VirtualProtect(&vtable[17], sizeof(void*), old_protect, &old_protect);
        log_line("Present hook installed (vtable slot 17)");
    } else {
        log_line("Present hook: VirtualProtect failed");
    }
    // The engine may create an IDirect3DDevice9Ex: it has its own vtable and its own Present
    // implementation, so patching the plain device alone would never fire.
    typedef HRESULT(WINAPI* Direct3DCreate9ExFn)(UINT, IDirect3D9Ex**);
    HMODULE d3d9_module = GetModuleHandleW(L"d3d9.dll");
    if (d3d9_module == nullptr) {
        d3d9_module = LoadLibraryW(L"d3d9.dll");
    }
    if (d3d9_module != nullptr) {
        Direct3DCreate9ExFn create_ex = reinterpret_cast<Direct3DCreate9ExFn>(
            reinterpret_cast<void*>(GetProcAddress(d3d9_module, "Direct3DCreate9Ex")));
        if (create_ex != nullptr) {
            IDirect3D9Ex* d3d_ex = nullptr;
            if (SUCCEEDED(create_ex(D3D_SDK_VERSION, &d3d_ex)) && d3d_ex != nullptr) {
                IDirect3DDevice9* device_ex = nullptr;
                if (SUCCEEDED(d3d_ex->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
                                                   D3DCREATE_SOFTWARE_VERTEXPROCESSING, &params,
                                                   &device_ex))
                    && device_ex != nullptr) {
                    void** vtable_ex = *reinterpret_cast<void***>(device_ex);
                    g_present_ex = reinterpret_cast<PresentFn>(vtable_ex[17]);
                    DWORD old_ex = 0;
                    if (VirtualProtect(&vtable_ex[17], sizeof(void*), PAGE_READWRITE, &old_ex)) {
                        vtable_ex[17] = reinterpret_cast<void*>(&present_hook_ex);
                        VirtualProtect(&vtable_ex[17], sizeof(void*), old_ex, &old_ex);
                        log_line("Present hook (Ex) installed");
                    } else {
                        log_line("Present hook (Ex): VirtualProtect failed");
                    }
                    // IDirect3DDevice9Ex adds PresentEx at slot 121, and a game that uses the
                    // Ex device usually calls that one instead of Present.
                    void** present_ex_slot = &vtable_ex[121];
                    g_present_ex_slot = reinterpret_cast<PresentExFn>(*present_ex_slot);
                    DWORD old_present_ex = 0;
                    if (g_present_ex_slot != nullptr &&
                        VirtualProtect(present_ex_slot, sizeof(void*), PAGE_READWRITE,
                                       &old_present_ex)) {
                        *present_ex_slot = reinterpret_cast<void*>(&present_ex_slot_hook);
                        VirtualProtect(present_ex_slot, sizeof(void*), old_present_ex,
                                       &old_present_ex);
                        log_line("PresentEx hook (slot 121) installed");
                    }
                    patch_swapchain(device_ex, "ex");
                    device_ex->Release();
                } else {
                    log_line("Present hook (Ex): CreateDevice failed");
                }
                d3d_ex->Release();
            } else {
                log_line("Present hook (Ex): Direct3DCreate9Ex failed");
            }
        }
    }
    patch_swapchain(device, "plain");
    device->Release();
    DestroyWindow(window);
    d3d->Release();
    return g_present != nullptr || g_present_ex != nullptr;
}

// The game loads d3d11.dll and dxgi.dll (checked live), so the picture is presented by a
// DXGI swap chain, not by D3D9: the hooks above exist for builds that render through D3D9,
// and this one is the one that actually fires here. Slot 8 is IDXGISwapChain::Present.
typedef HRESULT(WINAPI* DxgiPresentFn)(IDXGISwapChain*, UINT, UINT);
typedef HRESULT(WINAPI* DxgiPresent1Fn)(IDXGISwapChain1*, UINT, UINT,
                                        const DXGI_PRESENT_PARAMETERS*);
DxgiPresentFn g_dxgi_present = nullptr;
DxgiPresent1Fn g_dxgi_present1 = nullptr;

HRESULT WINAPI dxgi_present_hook(IDXGISwapChain* chain, UINT interval, UINT flags)
{
    note_frame();
    return g_dxgi_present(chain, interval, flags);
}

HRESULT WINAPI dxgi_present1_hook(IDXGISwapChain1* chain, UINT interval, UINT flags,
                                  const DXGI_PRESENT_PARAMETERS* params)
{
    note_frame();
    return g_dxgi_present1(chain, interval, flags, params);
}

void install_dxgi_hook()
{
    if (g_dxgi_present != nullptr) {
        return;
    }
    WNDCLASSEXW klass = {};
    klass.cbSize = sizeof(klass);
    klass.lpfnWndProc = dummy_window_proc;
    klass.hInstance = GetModuleHandleW(nullptr);
    klass.lpszClassName = L"coop-mod-dxgi";
    RegisterClassExW(&klass);
    HWND window = CreateWindowExW(0, klass.lpszClassName, L"coop", WS_POPUP, 0, 0, 8, 8, nullptr,
                                  nullptr, klass.hInstance, nullptr);
    if (window == nullptr) {
        log_line("DXGI hook: dummy window failed");
        return;
    }
    DXGI_SWAP_CHAIN_DESC desc = {};
    desc.BufferCount = 1;
    desc.BufferDesc.Width = 8;
    desc.BufferDesc.Height = 8;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = window;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain* chain = nullptr;
    D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
    HRESULT created = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &desc,
        &chain, &device, &level, &context);
    if (FAILED(created) || chain == nullptr) {
        char line[96];
        std::snprintf(line, sizeof(line), "DXGI hook: CreateDeviceAndSwapChain 0x%08lx",
                      static_cast<unsigned long>(created));
        log_line(line);
        DestroyWindow(window);
        return;
    }
    void** vtable = *reinterpret_cast<void***>(chain);
    g_dxgi_present = reinterpret_cast<DxgiPresentFn>(vtable[8]);
    DWORD old_protect = 0;
    if (g_dxgi_present != nullptr &&
        VirtualProtect(&vtable[8], sizeof(void*), PAGE_READWRITE, &old_protect)) {
        vtable[8] = reinterpret_cast<void*>(&dxgi_present_hook);
        VirtualProtect(&vtable[8], sizeof(void*), old_protect, &old_protect);
        log_line("DXGI Present hook installed (slot 8)");
    }
    // A flip-model chain presents through IDXGISwapChain1::Present1, slot 22.
    void** present1_slot = &vtable[22];
    g_dxgi_present1 = reinterpret_cast<DxgiPresent1Fn>(*present1_slot);
    DWORD old_present1 = 0;
    if (g_dxgi_present1 != nullptr &&
        VirtualProtect(present1_slot, sizeof(void*), PAGE_READWRITE, &old_present1)) {
        *present1_slot = reinterpret_cast<void*>(&dxgi_present1_hook);
        VirtualProtect(present1_slot, sizeof(void*), old_present1, &old_present1);
        log_line("DXGI Present1 hook installed (slot 22)");
    }
    if (context != nullptr) {
        context->Release();
    }
    if (device != nullptr) {
        device->Release();
    }
    chain->Release();
    DestroyWindow(window);
}

// --- capture the engine's own swap chain -----------------------------------
//
// The exe imports no d3d/dxgi DLL: it resolves D3D11CreateDeviceAndSwapChain through
// GetProcAddress (measured). A dummy device therefore shares nothing with the game's chain.
// Patching the exe's import slot for GetProcAddress is a plain memory write, safe from
// DllMain, and it runs before the engine asks for anything.
typedef HRESULT(WINAPI* CreateSwapChainFn)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT,
                                           const D3D_FEATURE_LEVEL*, UINT, UINT,
                                           const DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**,
                                           ID3D11Device**, D3D_FEATURE_LEVEL*,
                                           ID3D11DeviceContext**);
typedef FARPROC(WINAPI* GetProcAddressFn)(HMODULE, LPCSTR);

CreateSwapChainFn g_real_create_swapchain = nullptr;
GetProcAddressFn g_real_get_proc_address = nullptr;

void patch_chain(IDXGISwapChain* chain, const char* label)
{
    if (chain == nullptr) {
        return;
    }
    void** vtable = *reinterpret_cast<void***>(chain);
    DWORD old_protect = 0;
    if (vtable[8] != reinterpret_cast<void*>(&dxgi_present_hook)) {
        g_dxgi_present = reinterpret_cast<DxgiPresentFn>(vtable[8]);
        if (VirtualProtect(&vtable[8], sizeof(void*), PAGE_READWRITE, &old_protect)) {
            vtable[8] = reinterpret_cast<void*>(&dxgi_present_hook);
            VirtualProtect(&vtable[8], sizeof(void*), old_protect, &old_protect);
        }
    }
    if (vtable[22] != reinterpret_cast<void*>(&dxgi_present1_hook)) {
        g_dxgi_present1 = reinterpret_cast<DxgiPresent1Fn>(vtable[22]);
        if (VirtualProtect(&vtable[22], sizeof(void*), PAGE_READWRITE, &old_protect)) {
            vtable[22] = reinterpret_cast<void*>(&dxgi_present1_hook);
            VirtualProtect(&vtable[22], sizeof(void*), old_protect, &old_protect);
        }
    }
    char line[96];
    std::snprintf(line, sizeof(line), "game swap chain captured and patched (%s)", label);
    coop_note(line);
}

HRESULT WINAPI create_swapchain_hook(IDXGIAdapter* adapter, D3D_DRIVER_TYPE driver_type,
                                     HMODULE software, UINT flags,
                                     const D3D_FEATURE_LEVEL* levels, UINT level_count,
                                     UINT sdk_version, const DXGI_SWAP_CHAIN_DESC* desc,
                                     IDXGISwapChain** chain, ID3D11Device** device,
                                     D3D_FEATURE_LEVEL* chosen, ID3D11DeviceContext** context)
{
    HRESULT result = E_FAIL;
    if (g_real_create_swapchain != nullptr) {
        result = g_real_create_swapchain(adapter, driver_type, software, flags, levels, level_count,
                                         sdk_version, desc, chain, device, chosen, context);
        if (SUCCEEDED(result) && chain != nullptr && *chain != nullptr) {
            patch_chain(*chain, "game");
        }
    }
    return result;
}

typedef HRESULT(WINAPI* CreateFactoryFn)(REFIID, void**);
typedef HRESULT(WINAPI* FactoryCreateSwapChainFn)(IDXGIFactory*, IUnknown*, DXGI_SWAP_CHAIN_DESC*,
                                                 IDXGISwapChain**);

CreateFactoryFn g_real_create_factory = nullptr;
FactoryCreateSwapChainFn g_real_factory_create_swapchain = nullptr;

HRESULT WINAPI factory_create_swapchain_hook(IDXGIFactory* self, IUnknown* device,
                                             DXGI_SWAP_CHAIN_DESC* desc, IDXGISwapChain** out)
{
    HRESULT result = E_FAIL;
    if (g_real_factory_create_swapchain != nullptr) {
        result = g_real_factory_create_swapchain(self, device, desc, out);
        if (SUCCEEDED(result) && out != nullptr && *out != nullptr) {
            patch_chain(*out, "factory");
        }
    }
    return result;
}

HRESULT WINAPI create_factory_hook(REFIID riid, void** out)
{
    HRESULT result = E_FAIL;
    if (g_real_create_factory == nullptr) {
        return result;
    }
    result = g_real_create_factory(riid, out);
    if (SUCCEEDED(result) && out != nullptr && *out != nullptr) {
        auto* factory = reinterpret_cast<IDXGIFactory*>(*out);
        void** vtable = *reinterpret_cast<void***>(factory);
        if (vtable[10] != reinterpret_cast<void*>(&factory_create_swapchain_hook)) {
            g_real_factory_create_swapchain =
                reinterpret_cast<FactoryCreateSwapChainFn>(vtable[10]);
            DWORD old_protect = 0;
            if (VirtualProtect(&vtable[10], sizeof(void*), PAGE_READWRITE, &old_protect)) {
                vtable[10] = reinterpret_cast<void*>(&factory_create_swapchain_hook);
                VirtualProtect(&vtable[10], sizeof(void*), old_protect, &old_protect);
                coop_note("DXGI factory hooked (CreateSwapChain, slot 10)");
            }
        }
    }
    return result;
}

FARPROC WINAPI get_proc_address_hook(HMODULE module, LPCSTR name)
{
    FARPROC proc = nullptr;
    if (g_real_get_proc_address == nullptr) {
        return nullptr;
    }
    proc = g_real_get_proc_address(module, name);
    if (name != nullptr) {
        // Prove whether our slot is used at all: every name the engine resolves comes here.
        char line[160];
        std::snprintf(line, sizeof(line), "gpa: %s -> %s", name, proc == nullptr ? "null" : "ok");
        coop_note(line);
    }
    if (name != nullptr && proc != nullptr &&
        (lstrcmpA(name, "CreateDXGIFactory") == 0 || lstrcmpA(name, "CreateDXGIFactory1") == 0)) {
        g_real_create_factory = reinterpret_cast<CreateFactoryFn>(reinterpret_cast<void*>(proc));
        return reinterpret_cast<FARPROC>(&create_factory_hook);
    }
    if (name != nullptr && proc != nullptr &&
        lstrcmpA(name, "D3D11CreateDeviceAndSwapChain") == 0) {
        g_real_create_swapchain = reinterpret_cast<CreateSwapChainFn>(
            reinterpret_cast<void*>(proc));
        return reinterpret_cast<FARPROC>(&create_swapchain_hook);
    }
    return proc;
}

// Patch the exe's own import slot, so the engine gets our wrapper when it resolves the call.
void hook_get_proc_address()
{
    HMODULE exe = GetModuleHandleW(nullptr);
    if (exe == nullptr) {
        return;
    }
    auto* base = reinterpret_cast<BYTE*>(exe);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return;
    }
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return;
    }
    IMAGE_DATA_DIRECTORY directory =
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (directory.VirtualAddress == 0) {
        return;
    }
    auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + directory.VirtualAddress);
    for (; descriptor->Name != 0; ++descriptor) {
        const char* dll = reinterpret_cast<const char*>(base + descriptor->Name);
        if (lstrcmpiA(dll, "kernel32.dll") != 0 && lstrcmpiA(dll, "kernelbase.dll") != 0) {
            continue;
        }
        auto* lookup = reinterpret_cast<IMAGE_THUNK_DATA*>(base + descriptor->OriginalFirstThunk);
        auto* iat = reinterpret_cast<IMAGE_THUNK_DATA*>(base + descriptor->FirstThunk);
        for (; lookup->u1.Function != 0; ++lookup, ++iat) {
            if ((lookup->u1.Ordinal & IMAGE_ORDINAL_FLAG) != 0) {
                continue;
            }
            auto* by_name = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + lookup->u1.AddressOfData);
            if (lstrcmpA(reinterpret_cast<const char*>(by_name->Name), "GetProcAddress") != 0) {
                continue;
            }
            g_real_get_proc_address = reinterpret_cast<GetProcAddressFn>(
                reinterpret_cast<void*>(iat->u1.Function));
            DWORD old_protect = 0;
            if (VirtualProtect(&iat->u1.Function, sizeof(void*), PAGE_READWRITE, &old_protect)) {
                iat->u1.Function = reinterpret_cast<ULONG_PTR>(&get_proc_address_hook);
                VirtualProtect(&iat->u1.Function, sizeof(void*), old_protect, &old_protect);
            }
            return;
        }
    }
}

// --- Stage 1: find the live player object by its vtable ---------------------
//
// DS_Component_Player's vtable RVA comes from the static RTTI index (0x7d5684). An object of
// that class starts with a pointer to it, so a dword equal to module_base + rva in committed
// writable memory is a live instance. Reads go through ReadProcessMemory, so a bad page is an
// error code, never a crash.
const unsigned kPlayerVtableRva = 0x7d5684;
const unsigned kMoverVtableRva = 0x773748;

// Log plausible position triples inside an object: three consecutive finite floats in a sane
// range, at least two of them non-zero. Pointers, string ids and counters never pass this.
void log_triples(const char* label, uintptr_t address, size_t bytes)
{
    unsigned char raw[1024];
    size_t want = bytes < sizeof(raw) ? bytes : sizeof(raw);
    SIZE_T got = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), raw, want, &got)) {
        return;
    }
    int logged = 0;
    for (size_t offset = 0; offset + 12 <= got && logged < 6; offset += 4) {
        float values[3];
        memcpy(values, raw + offset, sizeof(values));
        bool finite = true;
        int non_zero = 0;
        for (int index = 0; index < 3; ++index) {
            if (!(values[index] == values[index])) {              // NaN
                finite = false;
            }
            if (values[index] > 1.0e5f || values[index] < -1.0e5f) {
                finite = false;
            }
            if (values[index] != 0.0f) {
                ++non_zero;
            }
        }
        if (!finite || non_zero < 2) {
            continue;
        }
        if (values[0] == values[1] && values[1] == values[2]) {
            continue;
        }
        char line[160];
        std::snprintf(line, sizeof(line), "  %s +0x%03x: %.3f %.3f %.3f", label,
                      static_cast<unsigned>(offset), values[0], values[1], values[2]);
        log_line(line);
        ++logged;
    }
}

void scan_vtable(const char* label, unsigned rva, int limit)
{
    HMODULE exe = GetModuleHandleW(nullptr);
    if (exe == nullptr) {
        return;
    }
    unsigned target = static_cast<unsigned>(reinterpret_cast<uintptr_t>(exe)) + rva;
    MEMORY_BASIC_INFORMATION info = {};
    unsigned char buffer[64 * 1024];
    int found = 0;
    uintptr_t address = 0x10000;
    for (;;) {
        if (VirtualQuery(reinterpret_cast<void*>(address), &info, sizeof(info)) == 0) {
            break;
        }
        uintptr_t start = reinterpret_cast<uintptr_t>(info.BaseAddress);
        uintptr_t end = start + info.RegionSize;
        const bool usable = info.State == MEM_COMMIT &&
                            (info.Protect & (PAGE_READWRITE | PAGE_WRITECOPY |
                                             PAGE_EXECUTE_READWRITE));
        if (usable) {
            for (uintptr_t at = start; at < end; at += sizeof(buffer)) {
                SIZE_T want = end - at > sizeof(buffer) ? sizeof(buffer) : end - at;
                SIZE_T got = 0;
                if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(at), buffer,
                                       want, &got)) {
                    break;
                }
                for (SIZE_T offset = 0; offset + 4 <= got; offset += 4) {
                    unsigned value = 0;
                    memcpy(&value, buffer + offset, 4);
                    if (value != target) {
                        continue;
                    }
                    char line[128];
                    std::snprintf(line, sizeof(line), "%s object at 0x%08llx", label,
                                  static_cast<unsigned long long>(at + offset));
                    log_line(line);
                    if (found == 0) {
                        // First hit: log the plausible position triples, so the offset can be
                        // read from the log instead of another blind scan.
                        log_triples(label, at + offset, 0x200);
                    }
                    if (++found >= limit) {
                        return;
                    }
                }
            }
        }
        if (end <= address) {
            break;
        }
        address = end;
    }
    char line[96];
    std::snprintf(line, sizeof(line), "%s objects found: %d", label, found);
    log_line(line);
}

// --- all device-creation entry points, inline -------------------------------
//
// The engine resolves the graphics entry points early and caches the pointers, so returning a
// wrapper from GetProcAddress is not enough. Each creation call is hooked in the function
// itself, logs that it ran, and patches the object it returns.
typedef IDirect3D9*(WINAPI* Direct3DCreate9Fn)(UINT);
typedef HRESULT(WINAPI* Direct3DCreate9ExFn2)(UINT, IDirect3D9Ex**);
typedef HRESULT(WINAPI* CreateDeviceFn)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD,
                                        D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
typedef HRESULT(WINAPI* CreateSwapChainForHwndFn)(IDXGIFactory2*, IUnknown*, HWND,
                                                  const DXGI_SWAP_CHAIN_DESC1*,
                                                  const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*,
                                                  IDXGIOutput*, IDXGISwapChain1**);

Direct3DCreate9Fn g_real_d3d9_create = nullptr;
Direct3DCreate9ExFn2 g_real_d3d9_create_ex = nullptr;
CreateDeviceFn g_real_d3d9_create_device = nullptr;
CreateSwapChainForHwndFn g_real_create_swapchain_hwnd = nullptr;

HRESULT WINAPI d3d9_create_device_hook(IDirect3D9* self, UINT adapter, D3DDEVTYPE type,
                                       HWND window, DWORD flags, D3DPRESENT_PARAMETERS* params,
                                       IDirect3DDevice9** device)
{
    HRESULT result = E_FAIL;
    coop_note("called: IDirect3D9::CreateDevice");
    if (g_real_d3d9_create_device != nullptr) {
        result = g_real_d3d9_create_device(self, adapter, type, window, flags, params, device);
        if (SUCCEEDED(result) && device != nullptr && *device != nullptr) {
            patch_swapchain(*device, "d3d9-device");
        }
    }
    return result;
}

IDirect3D9* WINAPI direct3d_create9_hook(UINT sdk)
{
    coop_note("called: Direct3DCreate9");
    IDirect3D9* d3d = g_real_d3d9_create != nullptr ? g_real_d3d9_create(sdk) : nullptr;
    if (d3d != nullptr) {
        void** vtable = *reinterpret_cast<void***>(d3d);
        if (vtable[15] != reinterpret_cast<void*>(&d3d9_create_device_hook)) {
            g_real_d3d9_create_device = reinterpret_cast<CreateDeviceFn>(vtable[15]);
            DWORD old_protect = 0;
            if (VirtualProtect(&vtable[15], sizeof(void*), PAGE_READWRITE, &old_protect)) {
                vtable[15] = reinterpret_cast<void*>(&d3d9_create_device_hook);
                VirtualProtect(&vtable[15], sizeof(void*), old_protect, &old_protect);
            }
        }
    }
    return d3d;
}

HRESULT WINAPI create_swapchain_hwnd_hook(IDXGIFactory2* self, IUnknown* device, HWND window,
                                          const DXGI_SWAP_CHAIN_DESC1* desc,
                                          const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreen,
                                          IDXGIOutput* restrict, IDXGISwapChain1** out)
{
    coop_note("called: CreateSwapChainForHwnd");
    HRESULT result = E_FAIL;
    if (g_real_create_swapchain_hwnd != nullptr) {
        result = g_real_create_swapchain_hwnd(self, device, window, desc, fullscreen, restrict, out);
        if (SUCCEEDED(result) && out != nullptr && *out != nullptr) {
            patch_chain(reinterpret_cast<IDXGISwapChain*>(*out), "hwnd");
        }
    }
    return result;
}

void inline_hook(const wchar_t* module_name, const char* name, void* detour, void** original)
{
    HMODULE module = LoadLibraryW(module_name);
    if (module == nullptr) {
        return;
    }
    FARPROC target = GetProcAddress(module, name);
    if (target == nullptr) {
        return;
    }
    if (MH_CreateHook(reinterpret_cast<void*>(target), detour, original) == MH_OK &&
        MH_EnableHook(reinterpret_cast<void*>(target)) == MH_OK) {
        char line[128];
        std::snprintf(line, sizeof(line), "inline hook: %s", name);
        coop_note(line);
    }
}

void install_inline_hooks()
{
    // Which hooks to enable is read from SP_COOP_INLINE, so each one can be tested alone
    // (2026-11-15: enabling them together killed the game during startup): "d3d11",
    // "factory", "d3d9", or "all". Unset or empty means none.
    wchar_t wanted[64] = {};
    DWORD length = GetEnvironmentVariableW(L"SP_COOP_INLINE", wanted, 64);
    if (length == 0) {
        coop_note("inline hooks: none requested (SP_COOP_INLINE unset)");
        return;
    }
    const bool all = lstrcmpiW(wanted, L"all") == 0;
    if (MH_Initialize() != MH_OK) {
        log_line("MinHook: initialize failed");
        return;
    }
    if (all || lstrcmpiW(wanted, L"d3d11") == 0) {
        inline_hook(L"d3d11.dll", "D3D11CreateDeviceAndSwapChain",
                    reinterpret_cast<void*>(&create_swapchain_hook),
                    reinterpret_cast<void**>(&g_real_create_swapchain));
    }
    if (all || lstrcmpiW(wanted, L"d3d9") == 0) {
        inline_hook(L"d3d9.dll", "Direct3DCreate9",
                    reinterpret_cast<void*>(&direct3d_create9_hook),
                    reinterpret_cast<void**>(&g_real_d3d9_create));
    }
    if (all || lstrcmpiW(wanted, L"factory") == 0) {
        inline_hook(L"dxgi.dll", "CreateDXGIFactory",
                    reinterpret_cast<void*>(&create_factory_hook),
                    reinterpret_cast<void**>(&g_real_create_factory));
    }
    inline_hook(L"dxgi.dll", "CreateDXGIFactory1", reinterpret_cast<void*>(&create_factory_hook),
                reinterpret_cast<void**>(&g_real_create_factory));
}

DWORD WINAPI worker(LPVOID)
{
    wchar_t path[MAX_PATH];
    DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return 0;
    }
    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash == nullptr) {
        return 0;
    }
    *(slash + 1) = L'\0';
    lstrcatW(path, L"coop-mod.log");
    g_log = _wfopen(path, L"a");
    log_line("--- coop mod attached ---");
    log_value("pid", static_cast<long long>(GetCurrentProcessId()));
    if (g_notes_length > 0) {
        g_notes[g_notes_length] = '\0';
        log_line(g_notes);
    }
    install_present_hook();
    install_dxgi_hook();
    install_inline_hooks();

    // The thread still ticks, so a run is observable even before the first frame hook fires.
    LONG next_scan = 600;
    for (;;) {
        ++g_ticks;
        if ((g_ticks % 300) == 0) {
            log_value("ticks", g_ticks);
        }
        if (g_ticks >= next_scan) {
            next_scan = g_ticks + 600;
            scan_vtable("player", kPlayerVtableRva, 3);
            scan_vtable("mover", kMoverVtableRva, 3);
        }
        Sleep(16);
    }
}

}  // namespace

// Notes written before the worker opens its file are buffered and flushed by the worker.
extern "C" void coop_note(const char* text)
{
    if (g_log != nullptr) {
        log_line(text);
        return;
    }
    size_t length = 0;
    while (text[length] != '\0') {
        ++length;
    }
    if (g_notes_length + length + 2 >= sizeof(g_notes)) {
        return;
    }
    for (size_t index = 0; index < length; ++index) {
        g_notes[g_notes_length++] = text[index];
    }
    g_notes[g_notes_length++] = '\n';
}

// Called by the proxy export, i.e. after the loader has released its lock.
extern "C" void coop_start_worker()
{
    static volatile LONG started = 0;
    if (InterlockedCompareExchange(&started, 1, 0) != 0) {
        return;
    }
    HANDLE thread = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr);
    if (thread != nullptr) {
        CloseHandle(thread);
    }
}

BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        // Nothing but bookkeeping here: creating threads or opening files under the loader
        // lock deadlocks the process (and Frida then cannot even attach). The worker starts
        // from DirectInput8Create, which the game calls after the loader is done.
        DisableThreadLibraryCalls(self);
        // Memory-only: patch the exe's GetProcAddress slot before any game code runs, so the
        // D3D11 device the engine creates can be captured. No threads, no file I/O here.
        hook_get_proc_address();
    }
    return TRUE;
}
