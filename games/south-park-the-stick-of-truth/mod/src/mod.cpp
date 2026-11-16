// The mod core, loaded by the proxy. Stage 1 of the hub's build order: our code runs inside
// the game, ticks once per rendered frame, and can read the local player's position.
//
// The frame hook patches the D3D9 device vtable, the way games/the-witness does it: a dummy
// device is created and slot 17 (Present) is replaced after the original pointer is saved. No
// code is overwritten. NOTE: for this title the dummy device's vtable is NOT the engine's (none
// of these hooks fires), so the frame hook is groundwork for engines that share it, not a proven
// path here - see mod/README.md.
#include <windows.h>
#include <d3d9.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cmath>
#include "MinHook.h"
#include "coopbridge/server.h"
#include "net.h"
#include <string>
#include <vector>
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
// --- debug probe: tags [OK] / [ADDR] / [ERR], safe reads only -----------------
//
// Every read goes through a pointer check (VirtualQuery) and ReadProcessMemory. MinGW has
// no __try/__except (an MSVC extension); ReadProcessMemory is the equivalent guarantee: a
// bad pointer comes back as an error code, never as an exception. Nothing here writes to
// the game.
FILE* g_debug = nullptr;

void dbg(const char* tag, const char* format, ...)
{
    if (g_debug == nullptr) {
        return;
    }
    std::fprintf(g_debug, "%s ", tag);
    va_list args;
    va_start(args, format);
    std::vfprintf(g_debug, format, args);
    va_end(args);
    std::fputc('\n', g_debug);
    std::fflush(g_debug);
}

// The pointer check: every byte of [address, address+size) must be committed and readable.
// A null, temporary or freed pointer therefore becomes an [ERR] line and a skipped frame,
// never a crash. ReadProcessMemory below is the second line of defence.
bool checked_pointer(uintptr_t address, size_t size)
{
    if (address < 0x10000 || size == 0 || address + size < address) {
        return false;
    }
    uintptr_t at = address;
    const uintptr_t end = address + size;
    while (at < end) {
        MEMORY_BASIC_INFORMATION info = {};
        if (VirtualQuery(reinterpret_cast<void*>(at), &info, sizeof(info)) == 0) {
            return false;
        }
        if (info.State != MEM_COMMIT || (info.Protect & PAGE_NOACCESS) != 0 ||
            (info.Protect & PAGE_GUARD) != 0) {
            return false;
        }
        const uintptr_t region_end =
            reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
        if (region_end <= at) {
            return false;
        }
        at = region_end;
    }
    return true;
}

bool safe_read_quiet(uintptr_t address, void* out, size_t size)
{
    if (out == nullptr || !checked_pointer(address, size)) {
        return false;
    }
    SIZE_T got = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), out, size,
                             &got) != 0 && got == size;
}

bool safe_read(uintptr_t address, void* out, size_t size)
{
    if (out == nullptr || !checked_pointer(address, size)) {
        dbg("[ERR]", "read refused: address 0x%08llx size %u failed the pointer check",
            static_cast<unsigned long long>(address), static_cast<unsigned>(size));
        return false;
    }
    SIZE_T got = 0;
    const BOOL ok = ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), out,
                                      size, &got);
    if (!ok || got != size) {
        dbg("[ERR]", "read failed at 0x%08llx (got %u of %u)",
            static_cast<unsigned long long>(address), static_cast<unsigned>(got),
            static_cast<unsigned>(size));
        return false;
    }
    return true;
}

bool safe_read_u32(uintptr_t address, unsigned* out)
{
    return out != nullptr && safe_read(address, out, sizeof(*out));
}

bool finite_triple(const float* v)
{
    if (!(v[0] == v[0]) || !(v[1] == v[1]) || !(v[2] == v[2])) {
        return false;
    }
    int axes = 0;
    for (int i = 0; i < 3; ++i) {
        if (v[i] > 1.0e4f || v[i] < -1.0e4f) {
            return false;
        }
        if (v[i] >= 1.0f || v[i] <= -1.0f) {
            ++axes;
        }
    }
    return axes >= 2 && !(v[0] == v[1] && v[1] == v[2]);
}

// The first plausible triple in a raw block; no logging, so pointer chasing can try many
// candidates without filling the log with guesses.
bool find_triple(const unsigned char* raw, size_t size, float out[3], unsigned* offset_out)
{
    for (size_t offset = 4; offset + 12 <= size; offset += 4) {
        float v[3];
        memcpy(v, raw + offset, sizeof(v));
        if (!finite_triple(v)) {
            continue;
        }
        out[0] = v[0];
        out[1] = v[1];
        out[2] = v[2];
        if (offset_out != nullptr) {
            *offset_out = static_cast<unsigned>(offset);
        }
        return true;
    }
    return false;
}

// The best position triple in an object, with the offset it was found at.
bool probe_position(const char* who, uintptr_t object, float out[3], unsigned* offset_out)
{
    unsigned char raw[0x300];
    if (!safe_read(object, raw, sizeof(raw))) {
        return false;
    }
    if (find_triple(raw, sizeof(raw), out, offset_out)) {
        dbg("[OK]", "%s position at +0x%x: %.3f %.3f %.3f", who, *offset_out, out[0], out[1],
            out[2]);
        return true;
    }
    dbg("[ERR]", "%s: no plausible position triple in the first 0x300 bytes", who);
    return false;
}

// A yaw candidate: a float in [-180, 180] that is not part of the position triple. It is
// labelled a candidate because only a turn-and-watch test can prove which float is yaw.
void probe_yaw(const char* who, uintptr_t object, unsigned skip_offset)
{
    unsigned char raw[0x300];
    if (!safe_read(object, raw, sizeof(raw))) {
        return;
    }
    int logged = 0;
    for (size_t offset = 0; offset + 4 <= sizeof(raw) && logged < 4; offset += 4) {
        if (offset >= skip_offset && offset < skip_offset + 12) {
            continue;
        }
        float value = 0.0f;
        memcpy(&value, raw + offset, sizeof(value));
        if (!(value == value) || value < -180.0f || value > 180.0f || value == 0.0f) {
            continue;
        }
        dbg("[ADDR]", "%s yaw candidate at +0x%x = %.3f (rad or deg unproven)", who,
            static_cast<unsigned>(offset), value);
        ++logged;
    }
    if (logged == 0) {
        dbg("[ERR]", "%s: no yaw candidate in the first 0x300 bytes", who);
    }
}

// --- runtime address discovery: PE sections + byte patterns -------------------
//
// The static RTTI index (tools/rtti_index.json) supplies expected RVAs. Each is checked at
// runtime against the loaded image: the class-name string and the COL pointer are located
// with find_pattern(), and every live object's own RTTI is then resolved from its vtable.
// A mismatch is logged as [ERR]; the index value is only used as a labelled fallback.
struct ImageLayout {
    uintptr_t base = 0;
    size_t size = 0;
    uintptr_t rdata = 0;
    size_t rdata_size = 0;
    uintptr_t data = 0;
    size_t data_size = 0;
    uintptr_t text = 0;
    size_t text_size = 0;
    bool valid = false;
};
ImageLayout g_image;

bool in_range(uintptr_t address, size_t size, uintptr_t start, size_t range)
{
    return size != 0 && address >= start && address + size >= address &&
           address + size <= start + range;
}

bool load_image_layout()
{
    HMODULE exe = GetModuleHandleW(nullptr);
    if (exe == nullptr) {
        return false;
    }
    auto* base = reinterpret_cast<unsigned char*>(exe);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return false;
    }
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return false;
    }
    g_image.base = reinterpret_cast<uintptr_t>(exe);
    g_image.size = nt->OptionalHeader.SizeOfImage;
    IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
    for (WORD index = 0; index < nt->FileHeader.NumberOfSections; ++index, ++section) {
        if (memcmp(section->Name, ".rdata", 6) == 0) {
            g_image.rdata = g_image.base + section->VirtualAddress;
            g_image.rdata_size = section->Misc.VirtualSize;
        } else if (memcmp(section->Name, ".data", 5) == 0) {
            g_image.data = g_image.base + section->VirtualAddress;
            g_image.data_size = section->Misc.VirtualSize;
        } else if (memcmp(section->Name, ".text", 5) == 0) {
            g_image.text = g_image.base + section->VirtualAddress;
            g_image.text_size = section->Misc.VirtualSize;
        }
    }
    g_image.valid = g_image.rdata != 0 && g_image.rdata_size != 0 && g_image.text != 0;
    return g_image.valid;
}

// Small exact/wildcard byte-pattern scan over one mapped range. `mask` may be null (all
// bytes exact) or a string of 'x' / '?' per pattern byte. Reads are checked per chunk.
bool find_pattern(const void* pattern, const char* mask, size_t length, uintptr_t start,
                  size_t size, uintptr_t* found)
{
    if (pattern == nullptr || found == nullptr || length == 0 || length > 4096) {
        return false;
    }
    const auto* needle = static_cast<const unsigned char*>(pattern);
    unsigned char buffer[64 * 1024];
    size_t offset = 0;
    while (offset + length <= size) {
        const size_t want = size - offset < sizeof(buffer) ? size - offset : sizeof(buffer);
        if (!checked_pointer(start + offset, want) ||
            !safe_read_quiet(start + offset, buffer, want)) {
            dbg("[ERR]", "pattern scan: unreadable range at 0x%08llx size %u",
                static_cast<unsigned long long>(start + offset), static_cast<unsigned>(want));
            return false;
        }
        const size_t limit = want - length;
        for (size_t index = 0; index <= limit; ++index) {
            bool match = true;
            for (size_t byte = 0; byte < length; ++byte) {
                if (mask != nullptr && mask[byte] == '?') {
                    continue;
                }
                if (buffer[index + byte] != needle[byte]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                *found = start + offset + index;
                return true;
            }
        }
        const size_t step = want > length ? want - length + 1 : want;
        if (step == 0) {
            break;
        }
        offset += step;
    }
    return false;
}

struct ClassSpec {
    const char* label;
    const char* mangled;
    unsigned type_descriptor_rva;
    unsigned vtable_rva;
    unsigned col_rva;
};
const ClassSpec kClassSpecs[] = {
    {"player", ".?AVDS_Component_Player@Game_Component@OEI@@", 0x927f88, 0x7d5684, 0x893eb4},
    {"party", ".?AVComponent_Party_Member@Game_Component@OEI@@", 0x8f6384, 0x7b89ec,
     0x89185c},
    {"buddy_ai", ".?AVCompanion_Basic@Follower@Package@State@AI@OEI@@", 0x99709c, 0x7f88b4,
     0x897a3c},
    {"mover", ".?AVMoverImpl@bfx@@", 0x8d95d0, 0x773748, 0x88a818},
    {"anim", ".?AVComponent_Anim_Controller@Game_Component@OEI@@", 0x8f5b14, 0x7b6eac,
     0x891480},
    {"anim2", ".?AVComponent_Animation_Controller_2@Game_Component@OEI@@", 0x8f5e90, 0x7b7aa4,
     0x8915fc},
    {"anim3", ".?AVNY_Component_Anim_Controller@Game_Component@OEI@@", 0x97e450, 0x7f2f44,
     0x89656c},
};
const int kClassCount = static_cast<int>(sizeof(kClassSpecs) / sizeof(kClassSpecs[0]));
uintptr_t g_class_vtable[kClassCount] = {};
bool g_class_ready[kClassCount] = {};
uintptr_t g_class_hits[kClassCount][8] = {};
int g_class_counts[kClassCount] = {};

// Follow this_offset from a vtable to its COL, then the COL to the type descriptor name.
// This is the full single-inheritance COL walk for the classes used here (all have
// this_offset 0 in tools/rtti_index.json); multiple-inheritance vtables would need mdisp.
bool resolve_object_class(uintptr_t object, char* name_out, size_t name_size,
                          uintptr_t* vtable_out)
{
    if (name_out == nullptr || name_size == 0) {
        return false;
    }
    name_out[0] = '\0';
    uintptr_t vtable = 0;
    if (!safe_read(object, &vtable, sizeof(vtable))) {
        return false;
    }
    if (vtable_out != nullptr) {
        *vtable_out = vtable;
    }
    if (!in_range(vtable, 4, g_image.rdata, g_image.rdata_size)) {
        dbg("[ERR]", "RTTI: object 0x%08llx vtable 0x%08llx is outside .rdata",
            static_cast<unsigned long long>(object), static_cast<unsigned long long>(vtable));
        return false;
    }
    uintptr_t col = 0;
    if (!safe_read(vtable - 4, &col, sizeof(col))) {
        return false;
    }
    if (!in_range(col, 20, g_image.rdata, g_image.rdata_size)) {
        dbg("[ERR]", "RTTI: vtable 0x%08llx COL 0x%08llx is outside .rdata",
            static_cast<unsigned long long>(vtable), static_cast<unsigned long long>(col));
        return false;
    }
    unsigned signature = 0;
    unsigned offset = 0;
    unsigned cd_offset = 0;
    unsigned type_descriptor = 0;
    unsigned class_descriptor = 0;
    if (!safe_read(col, &signature, 4) || !safe_read(col + 4, &offset, 4) ||
        !safe_read(col + 8, &cd_offset, 4) || !safe_read(col + 12, &type_descriptor, 4) ||
        !safe_read(col + 16, &class_descriptor, 4)) {
        return false;
    }
    if (signature != 0 || offset != 0) {
        dbg("[ERR]", "RTTI: COL 0x%08llx has signature %u offset %u; not a this_offset-0 COL",
            static_cast<unsigned long long>(col), signature, offset);
        return false;
    }
    if (!in_range(type_descriptor, 8, g_image.base, g_image.size)) {
        dbg("[ERR]", "RTTI: COL 0x%08llx type descriptor 0x%08x is outside the image",
            static_cast<unsigned long long>(col), type_descriptor);
        return false;
    }
    char raw[192];
    if (!safe_read(type_descriptor + 8, raw, sizeof(raw) - 1)) {
        return false;
    }
    raw[sizeof(raw) - 1] = '\0';
    if (strncmp(raw, ".?AV", 4) != 0) {
        dbg("[ERR]", "RTTI: type descriptor 0x%08x name is not a class name: %.32s",
            type_descriptor, raw);
        return false;
    }
    strncpy(name_out, raw, name_size - 1);
    name_out[name_size - 1] = '\0';
    return true;
}

// Pattern-verify each class against the loaded image and remember the vtable address.
void verify_rtti_classes()
{
    if (!load_image_layout()) {
        dbg("[ERR]", "image layout: PE headers or .rdata/.text not found; address checks off");
        return;
    }
    dbg("[ADDR]", "image base=0x%08llx size=0x%x .rdata=0x%08llx+0x%x .data=0x%08llx+0x%x "
                  ".text=0x%08llx+0x%x",
        static_cast<unsigned long long>(g_image.base), static_cast<unsigned>(g_image.size),
        static_cast<unsigned long long>(g_image.rdata),
        static_cast<unsigned>(g_image.rdata_size),
        static_cast<unsigned long long>(g_image.data),
        static_cast<unsigned>(g_image.data_size),
        static_cast<unsigned long long>(g_image.text),
        static_cast<unsigned>(g_image.text_size));
    for (int index = 0; index < kClassCount; ++index) {
        const ClassSpec& spec = kClassSpecs[index];
        const uintptr_t expected_vtable = g_image.base + spec.vtable_rva;
        g_class_vtable[index] = expected_vtable;
        g_class_ready[index] = true;

        uintptr_t name_at = 0;
        // MSVC type descriptors (and their names) live in .data in this build, not .rdata;
        // the vtables and COLs are in .rdata. Search the right section and say which one.
        bool name_found = false;
        const char* name_section = ".data";
        if (g_image.data != 0) {
            name_found = find_pattern(spec.mangled, nullptr, strlen(spec.mangled) + 1,
                                      g_image.data, g_image.data_size, &name_at);
        }
        if (!name_found) {
            name_section = "whole image";
            name_found = find_pattern(spec.mangled, nullptr, strlen(spec.mangled) + 1,
                                      g_image.base, g_image.size, &name_at);
        }
        if (name_found) {
            const uintptr_t expected_name = g_image.base + spec.type_descriptor_rva + 8;
            if (name_at == expected_name) {
                dbg("[OK]", "RTTI %s class-name at 0x%08llx in %s (index 0x%x matches)",
                    spec.label, static_cast<unsigned long long>(name_at), name_section,
                    spec.type_descriptor_rva);
            } else {
                dbg("[ERR]", "RTTI %s class-name at 0x%08llx in %s but index says 0x%08llx",
                    spec.label, static_cast<unsigned long long>(name_at), name_section,
                    static_cast<unsigned long long>(expected_name));
            }
        } else {
            dbg("[ERR]", "RTTI %s class-name '%s' not found in .data or the image", spec.label,
                spec.mangled);
        }

        const unsigned col_value = static_cast<unsigned>(g_image.base + spec.col_rva);
        uintptr_t col_slot = 0;
        if (find_pattern(&col_value, nullptr, sizeof(col_value), g_image.rdata,
                         g_image.rdata_size, &col_slot)) {
            const uintptr_t vtable = col_slot + 4;
            if (vtable == expected_vtable) {
                g_class_vtable[index] = vtable;
                dbg("[OK]", "RTTI %s COL pointer at 0x%08llx -> vtable 0x%08llx "
                            "(index 0x%x matches)",
                    spec.label, static_cast<unsigned long long>(col_slot),
                    static_cast<unsigned long long>(vtable), spec.vtable_rva);
            } else {
                dbg("[ERR]", "RTTI %s COL pointer at 0x%08llx -> vtable 0x%08llx but index "
                             "says 0x%08llx; using index value",
                    spec.label, static_cast<unsigned long long>(col_slot),
                    static_cast<unsigned long long>(vtable),
                    static_cast<unsigned long long>(expected_vtable));
            }
        } else {
            dbg("[ERR]", "RTTI %s COL pointer 0x%08x not found in .rdata; using index vtable "
                         "0x%08llx",
                spec.label, col_value, static_cast<unsigned long long>(expected_vtable));
        }

        int text_slots = 0;
        for (int slot = 0; slot < 6; ++slot) {
            uintptr_t function = 0;
            if (safe_read_quiet(g_class_vtable[index] + slot * 4, &function,
                                sizeof(function)) &&
                in_range(function, 1, g_image.text, g_image.text_size)) {
                ++text_slots;
            }
        }
        if (text_slots >= 2) {
            dbg("[OK]", "RTTI %s vtable 0x%08llx has %d/6 slots inside .text", spec.label,
                static_cast<unsigned long long>(g_class_vtable[index]), text_slots);
        } else {
            dbg("[ERR]", "RTTI %s vtable 0x%08llx has only %d/6 slots inside .text", spec.label,
                static_cast<unsigned long long>(g_class_vtable[index]), text_slots);
        }
    }
}

// A position reached by following a pointer out of an object, which is where the live
// transform sits for the player component (its own triple at +0x5c is a cached copy).
bool find_chased_position(uintptr_t object, float out[3], unsigned* via_out,
                          uintptr_t* target_out, unsigned* offset_out)
{
    unsigned char head[0x100];
    if (!safe_read_quiet(object, head, sizeof(head))) {
        return false;
    }
    for (size_t slot = 4; slot + 4 <= sizeof(head); slot += 4) {
        unsigned value = 0;
        memcpy(&value, head + slot, sizeof(value));
        if (value == 0) {
            continue;
        }
        const uintptr_t target = value;
        if (target < 0x10000) {
            dbg("[ERR]", "chase +0x%x: pointer 0x%08x refused (null/temporary)",
                static_cast<unsigned>(slot), value);
            continue;
        }
        if (in_range(target, 4, g_image.base, g_image.size)) {
            continue;  // vtable or other image data, not a heap object
        }
        MEMORY_BASIC_INFORMATION info = {};
        if (VirtualQuery(reinterpret_cast<void*>(target), &info, sizeof(info)) == 0 ||
            info.State != MEM_COMMIT ||
            (info.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE)) == 0) {
            continue;
        }
        unsigned char raw[0x120];
        if (!safe_read_quiet(target, raw, sizeof(raw))) {
            continue;
        }
        float v[3];
        unsigned offset = 0;
        if (!find_triple(raw, sizeof(raw), v, &offset)) {
            continue;
        }
        out[0] = v[0];
        out[1] = v[1];
        out[2] = v[2];
        if (via_out != nullptr) {
            *via_out = static_cast<unsigned>(slot);
        }
        if (target_out != nullptr) {
            *target_out = target;
        }
        if (offset_out != nullptr) {
            *offset_out = offset;
        }
        return true;
    }
    return false;
}

void log_triple_at(const char* who, uintptr_t object, unsigned offset)
{
    unsigned char raw[12];
    if (!safe_read(object + offset, raw, sizeof(raw))) {
        return;
    }
    float v[3];
    memcpy(v, raw, sizeof(v));
    if (!finite_triple(v)) {
        dbg("[ERR]", "%s +0x%x is not a plausible triple", who, offset);
        return;
    }
    dbg("[OK]", "%s +0x%x: %.3f %.3f %.3f (offset candidate, semantics unproven)", who, offset,
        v[0], v[1], v[2]);
}

bool read_triple_at(uintptr_t object, unsigned offset, float out[3])
{
    unsigned char raw[12];
    if (!safe_read(object + offset, raw, sizeof(raw))) {
        return false;
    }
    memcpy(out, raw, sizeof(raw));
    return out[0] == out[0] && out[1] == out[1] && out[2] == out[2];
}

// The world-position test from the verified write run (28c1ea9e...): Clyde's Fort characters
// stand at a small positive height (0.4) and a real horizontal distance from the origin.
// Unit axes, matrix rows and counters fail this; a real (x, y, z) passes.
bool world_position(const float v[3])
{
    const double height = v[1] < 0.0f ? -static_cast<double>(v[1]) : static_cast<double>(v[1]);
    const double horizontal =
        std::sqrt(static_cast<double>(v[0]) * v[0] + static_cast<double>(v[2]) * v[2]);
    return height >= 0.05 && height <= 50.0 && horizontal >= 5.0 && horizontal <= 5000.0;
}

double distance3(const float a[3], const float b[3])
{
    const double dx = static_cast<double>(a[0]) - b[0];
    const double dy = static_cast<double>(a[1]) - b[1];
    const double dz = static_cast<double>(a[2]) - b[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

bool find_triple_at_object(uintptr_t object, float out[3], unsigned* offset_out);

// The live probe from the verified movement run: chase every pointer in the player
// component's head, keep only triples that pass world_position(), and pick the one nearest
// the component's own (cached) triple. This is the anchor that identifies the hero's mover.
bool find_live_probe(uintptr_t player, float out[3], unsigned* slot_out, uintptr_t* target_out,
                     unsigned* offset_out)
{
    float anchor[3] = {0.0f, 0.0f, 0.0f};
    unsigned anchor_offset = 0;
    const bool have_anchor = find_triple_at_object(player, anchor, &anchor_offset);
    if (have_anchor) {
        dbg("[ADDR]", "component cache anchor 0x%08llx +0x%x = %.3f %.3f %.3f",
            static_cast<unsigned long long>(player), anchor_offset, anchor[0], anchor[1],
            anchor[2]);
    }
    unsigned char head[0x100];
    if (!safe_read_quiet(player, head, sizeof(head))) {
        return false;
    }
    bool found = false;
    double best = 0.0;
    for (size_t slot = 4; slot + 4 <= sizeof(head); slot += 4) {
        unsigned value = 0;
        memcpy(&value, head + slot, sizeof(value));
        if (value == 0) {
            continue;
        }
        const uintptr_t target = value;
        if (target < 0x10000) {
            dbg("[ERR]", "probe chase +0x%x: pointer 0x%08x refused (null/temporary)",
                static_cast<unsigned>(slot), value);
            continue;
        }
        if (in_range(target, 4, g_image.base, g_image.size)) {
            continue;
        }
        MEMORY_BASIC_INFORMATION info = {};
        if (VirtualQuery(reinterpret_cast<void*>(target), &info, sizeof(info)) == 0 ||
            info.State != MEM_COMMIT ||
            (info.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE)) == 0) {
            continue;
        }
        unsigned char raw[0x120];
        if (!safe_read_quiet(target, raw, sizeof(raw))) {
            continue;
        }
        for (size_t offset = 4; offset + 12 <= sizeof(raw); offset += 4) {
            float v[3];
            memcpy(v, raw + offset, sizeof(v));
            if (!world_position(v)) {
                continue;
            }
            const double gap = have_anchor ? distance3(v, anchor) : 0.0;
            dbg("[ADDR]", "probe chase +0x%x -> 0x%08llx +0x%x = %.3f %.3f %.3f (d=%.3f)",
                static_cast<unsigned>(slot), static_cast<unsigned long long>(target),
                static_cast<unsigned>(offset), v[0], v[1], v[2], gap);
            if (!found || gap < best) {
                found = true;
                best = gap;
                out[0] = v[0];
                out[1] = v[1];
                out[2] = v[2];
                if (slot_out != nullptr) {
                    *slot_out = static_cast<unsigned>(slot);
                }
                if (target_out != nullptr) {
                    *target_out = target;
                }
                if (offset_out != nullptr) {
                    *offset_out = static_cast<unsigned>(offset);
                }
            }
        }
    }
    return found;
}

// The hero transform is mover+0x12c. Each of the three movers has a world-position-looking
// triple there, so "first mover" is not enough: the hero mover is the one whose +0x12c
// matches the live position reached by chasing the player component's pointer (slot 0x0c ->
// object+0x10). Motion was proven on 2026-11-16, run 28c1ea9e..., all validity flags true:
// writing +3 on X changed 32.380 -> 35.380 and the frame changed; the component's own +0x5c
// triple (0, 7, 45) never changed, so it is a cache.
bool resolve_hero_position(float out[3], uintptr_t* mover_out, unsigned* offset_out)
{
    if (g_class_counts[0] == 0) {
        dbg("[ERR]", "hero position: no DS_Component_Player object; cannot resolve the mover");
        return false;
    }
    float probe[3] = {0.0f, 0.0f, 0.0f};
    unsigned probe_slot = 0;
    uintptr_t probe_object = 0;
    unsigned probe_offset = 0;
    const bool have_probe = find_live_probe(g_class_hits[0][0], probe, &probe_slot,
                                            &probe_object, &probe_offset);
    if (have_probe) {
        dbg("[ADDR]", "hero probe: component 0x%08llx +0x%x -> 0x%08llx +0x%x = %.3f %.3f %.3f",
            static_cast<unsigned long long>(g_class_hits[0][0]), probe_slot,
            static_cast<unsigned long long>(probe_object), probe_offset, probe[0], probe[1],
            probe[2]);
    } else {
        dbg("[ERR]", "hero probe: no chased live triple from the player component");
    }

    bool found = false;
    double best = 0.0;
    uintptr_t chosen = 0;
    for (int index = 0; index < g_class_counts[3]; ++index) {
        const uintptr_t mover = g_class_hits[3][index];
        float v[3] = {0.0f, 0.0f, 0.0f};
        if (!read_triple_at(mover, 0x12c, v)) {
            dbg("[ERR]", "mover[%d] 0x%08llx +0x12c refused; skipped", index,
                static_cast<unsigned long long>(mover));
            continue;
        }
        if (!world_position(v)) {
            dbg("[ERR]", "mover[%d] 0x%08llx +0x12c %.3f %.3f %.3f fails the world-position test",
                index, static_cast<unsigned long long>(mover), v[0], v[1], v[2]);
            continue;
        }
        const double gap = have_probe ? distance3(v, probe) : 0.0;
        if (have_probe && gap > 1.0) {
            dbg("[ADDR]", "mover[%d] 0x%08llx +0x12c %.3f %.3f %.3f is not the hero (gap %.3f "
                          "from probe)",
                index, static_cast<unsigned long long>(mover), v[0], v[1], v[2], gap);
            continue;
        }
        if (!found || gap < best) {
            found = true;
            best = gap;
            chosen = mover;
            out[0] = v[0];
            out[1] = v[1];
            out[2] = v[2];
            if (mover_out != nullptr) {
                *mover_out = mover;
            }
            if (offset_out != nullptr) {
                *offset_out = 0x12c;
            }
        }
    }
    if (!found) {
        dbg("[ERR]", "hero position: no mover +0x12c passed the world-position test%s",
            have_probe ? " within 1.0 of the component probe" : "");
        return false;
    }
    dbg("[OK]", "hero position = mover 0x%08llx +0x12c: %.3f %.3f %.3f (motion-proven "
                "2026-11-16 run 28c1ea9e; gap from component probe %.3f)",
        static_cast<unsigned long long>(chosen), out[0], out[1], out[2], best);
    return true;
}

// Best position for a party/AI object: its own triple first, then a chased pointer target.
bool find_triple_at_object(uintptr_t object, float out[3], unsigned* offset_out)
{
    unsigned char raw[0x300];
    if (!safe_read_quiet(object, raw, sizeof(raw))) {
        return false;
    }
    return find_triple(raw, sizeof(raw), out, offset_out);
}

bool position_of_object(uintptr_t object, float out[3], unsigned* via_out, uintptr_t* target_out,
                        unsigned* offset_out)
{
    if (find_chased_position(object, out, via_out, target_out, offset_out)) {
        return true;
    }
    unsigned offset = 0;
    if (find_triple_at_object(object, out, &offset)) {
        if (via_out != nullptr) {
            *via_out = 0xffffffffu;
        }
        if (target_out != nullptr) {
            *target_out = object;
        }
        if (offset_out != nullptr) {
            *offset_out = offset;
        }
        return true;
    }
    return false;
}

void log_object(const char* role, uintptr_t object)
{
    if (object == 0 || !checked_pointer(object, 4)) {
        dbg("[ERR]", "%s object 0x%08llx refused; frame skipped", role,
            static_cast<unsigned long long>(object));
        return;
    }
    char name[192] = {};
    uintptr_t vtable = 0;
    if (!resolve_object_class(object, name, sizeof(name), &vtable)) {
        dbg("[ERR]", "%s object 0x%08llx: RTTI resolution failed; frame skipped", role,
            static_cast<unsigned long long>(object));
        return;
    }
    dbg("[ADDR]", "%s object 0x%08llx class=%s vtable=0x%08llx", role,
        static_cast<unsigned long long>(object), name,
        static_cast<unsigned long long>(vtable));

    float direct[3] = {0.0f, 0.0f, 0.0f};
    unsigned direct_offset = 0;
    const bool has_direct = probe_position(role, object, direct, &direct_offset);
    if (has_direct) {
        probe_yaw(role, object, direct_offset);
    }

    float live[3] = {0.0f, 0.0f, 0.0f};
    unsigned via = 0;
    uintptr_t target = 0;
    unsigned live_offset = 0;
    if (find_chased_position(object, live, &via, &target, &live_offset)) {
        if (!has_direct || target != object) {
            dbg("[OK]", "%s live position via +0x%x -> 0x%08llx at +0x%x: %.3f %.3f %.3f",
                role, via, static_cast<unsigned long long>(target), live_offset, live[0],
                live[1], live[2]);
            probe_yaw(role, target, live_offset);
        }
    } else if (!has_direct) {
        dbg("[ERR]", "%s: no position triple direct or via pointers; frame skipped", role);
    }
}

// Integers small enough to be an animation id or an AI state enum. They are logged as
// candidates with their offsets, never as a value of the field.
void log_int_candidates(const char* label, const char* field, uintptr_t object, bool detailed)
{
    unsigned char raw[0x200];
    if (!safe_read(object, raw, sizeof(raw))) {
        return;
    }
    int inspected = 0;
    int logged = 0;
    for (size_t offset = 4; offset + 4 <= sizeof(raw); offset += 4) {
        int value = 0;
        memcpy(&value, raw + offset, sizeof(value));
        ++inspected;
        if (!detailed || value <= 0 || value > 4096 || logged >= 8) {
            continue;
        }
        dbg("[ADDR]", "%s %s candidate int at +0x%x = %d (semantic unproven)", label, field,
            static_cast<unsigned>(offset), value);
        ++logged;
    }
    dbg("[UNKNOWN]", "%s %s: offset not proven; inspected +0x004..+0x%x at 4-byte stride "
                      "(%d ints), %d small-int candidates above",
        label, field, static_cast<unsigned>(sizeof(raw) - 4), inspected, logged);
}

void log_snapshot(const char* reason)
{
    dbg("[ADDR]", "snapshot %s tick=%ld player=%d party=%d buddy_ai=%d mover=%d anim=%d/%d/%d",
        reason, static_cast<long>(g_ticks), g_class_counts[0], g_class_counts[1],
        g_class_counts[2], g_class_counts[3], g_class_counts[4], g_class_counts[5],
        g_class_counts[6]);
    if (g_class_counts[0] == 0) {
        dbg("[ERR]", "hero: no DS_Component_Player object in this scan; frame skipped");
    }
    for (int index = 0; index < g_class_counts[0]; ++index) {
        log_object("hero", g_class_hits[0][index]);
    }
    float hero[3] = {0.0f, 0.0f, 0.0f};
    const bool have_hero = resolve_hero_position(hero, nullptr, nullptr);
    for (int index = 0; index < g_class_counts[3]; ++index) {
        const uintptr_t mover = g_class_hits[3][index];
        log_object("mover", mover);
        // Verified 2026-11-16 (run 28c1ea9e...): mover+0x12c is the live transform; +0x10 is a
        // unit-scale triple. Both are logged, and "hero position" above names the one that
        // matches the component's chased live triple.
        log_triple_at("mover", mover, 0x10);
        log_triple_at("mover", mover, 0x12c);
        if (have_hero) {
            float v[3] = {0.0f, 0.0f, 0.0f};
            if (read_triple_at(mover, 0x12c, v)) {
                dbg("[ADDR]", "mover[%d] 0x%08llx +0x12c distance from hero = %.3f", index,
                    static_cast<unsigned long long>(mover), distance3(v, hero));
            }
        }
    }
    for (int index = 0; index < g_class_counts[1]; ++index) {
        const uintptr_t party = g_class_hits[1][index];
        log_object("party", party);
        if (have_hero) {
            float p[3] = {0.0f, 0.0f, 0.0f};
            unsigned via = 0;
            uintptr_t target = 0;
            unsigned offset = 0;
            if (position_of_object(party, p, &via, &target, &offset)) {
                const double gap = distance3(p, hero);
                dbg("[OK]", "party[%d] 0x%08llx position %.3f %.3f %.3f, distance from hero "
                            "%.3f%s",
                    index, static_cast<unsigned long long>(party), p[0], p[1], p[2], gap,
                    gap > 2.0 ? " (companion candidate)" : " (same spot as hero)");
            }
        }
        dbg("[UNKNOWN]", "party[%d] BuddyState: Component_Party_Member has no proven AI-state "
                          "offset; see buddy_ai candidates", index);
    }
    for (int index = 0; index < g_class_counts[2]; ++index) {
        const uintptr_t buddy = g_class_hits[2][index];
        log_object("buddy_ai", buddy);
        log_int_candidates("buddy_ai", "BuddyState", buddy, index == 0);
    }
    for (int kind = 4; kind < kClassCount; ++kind) {
        for (int index = 0; index < g_class_counts[kind] && index < 2; ++index) {
            const uintptr_t anim = g_class_hits[kind][index];
            log_object("anim", anim);
            log_int_candidates("anim", "animation-id", anim, kind == 4 && index == 0);
        }
        if (g_class_counts[kind] == 0) {
            dbg("[UNKNOWN]", "animation-id: no %s object in this scan; offset unknown",
                kClassSpecs[kind].label);
        }
    }
    dbg("[UNKNOWN]", "yaw: offset not proven; inspected +0x000..+0x2ff at 4-byte stride on "
                      "hero/mover/party objects; candidate lines above are floats in [-180,180]");
}

void scan_buffer(const unsigned char* buffer, size_t got, uintptr_t base, uintptr_t lowest,
                 uintptr_t highest)
{
    for (size_t offset = 0; offset + 4 <= got; offset += 4) {
        unsigned value = 0;
        memcpy(&value, buffer + offset, sizeof(value));
        if (value < lowest || value > highest) {
            continue;
        }
        for (int index = 0; index < kClassCount; ++index) {
            if (value != static_cast<unsigned>(g_class_vtable[index])) {
                continue;
            }
            const uintptr_t object = base + offset;
            char name[192] = {};
            uintptr_t vtable = 0;
            if (!resolve_object_class(object, name, sizeof(name), &vtable)) {
                dbg("[ERR]", "%s candidate 0x%08llx failed RTTI resolution",
                    kClassSpecs[index].label, static_cast<unsigned long long>(object));
                break;
            }
            if (strcmp(name, kClassSpecs[index].mangled) != 0) {
                dbg("[ERR]", "%s candidate 0x%08llx RTTI says %s", kClassSpecs[index].label,
                    static_cast<unsigned long long>(object), name);
                break;
            }
            dbg("[ADDR]", "%s object 0x%08llx (RTTI %s)", kClassSpecs[index].label,
                static_cast<unsigned long long>(object), name);
            if (g_class_counts[index] < 8) {
                g_class_hits[index][g_class_counts[index]++] = object;
            }
            break;
        }
    }
}

// One pass over committed writable memory. A dword equal to a verified vtable address is
// an object candidate; it is accepted only if its own COL resolves to the expected class.
void scan_objects()
{
    for (int index = 0; index < kClassCount; ++index) {
        g_class_counts[index] = 0;
    }
    if (!g_image.valid) {
        dbg("[ERR]", "scan skipped: image layout is not available");
        return;
    }
    uintptr_t lowest = g_class_vtable[0];
    uintptr_t highest = g_class_vtable[0];
    for (int index = 1; index < kClassCount; ++index) {
        if (g_class_vtable[index] < lowest) {
            lowest = g_class_vtable[index];
        }
        if (g_class_vtable[index] > highest) {
            highest = g_class_vtable[index];
        }
    }
    MEMORY_BASIC_INFORMATION info = {};
    unsigned char buffer[64 * 1024];
    uintptr_t address = 0x10000;
    int skipped_pages = 0;
    for (;;) {
        if (VirtualQuery(reinterpret_cast<void*>(address), &info, sizeof(info)) == 0) {
            break;
        }
        const uintptr_t start = reinterpret_cast<uintptr_t>(info.BaseAddress);
        const uintptr_t end = start + info.RegionSize;
        const bool usable = info.State == MEM_COMMIT &&
                            (info.Protect & (PAGE_READWRITE | PAGE_WRITECOPY |
                                             PAGE_EXECUTE_READWRITE)) != 0;
        if (usable) {
            for (uintptr_t at = start; at < end; at += sizeof(buffer)) {
                const size_t want = end - at > sizeof(buffer) ? sizeof(buffer) : end - at;
                bool scanned = false;
                if (checked_pointer(at, want)) {
                    SIZE_T got = 0;
                    if (ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(at),
                                          buffer, want, &got)) {
                        scan_buffer(buffer, got, at, lowest, highest);
                        scanned = true;
                    }
                }
                if (!scanned) {
                    // A region can mix readable pages with guard/no-access pages. Fall back
                    // to 4 KiB pages and count the unreadable ones in one [ERR] line.
                    for (size_t page = 0; page < want; page += 0x1000) {
                        const uintptr_t page_at = at + page;
                        if (page_at >= end) {
                            break;
                        }
                        const size_t page_want =
                            end - page_at > 0x1000 ? 0x1000 : end - page_at;
                        SIZE_T got = 0;
                        if (!checked_pointer(page_at, page_want) ||
                            !ReadProcessMemory(GetCurrentProcess(),
                                               reinterpret_cast<void*>(page_at), buffer,
                                               page_want, &got)) {
                            ++skipped_pages;
                            continue;
                        }
                        scan_buffer(buffer, got, page_at, lowest, highest);
                    }
                }
            }
        }
        if (end <= address) {
            break;
        }
        address = end;
    }
    if (skipped_pages > 0) {
        dbg("[ERR]", "scan: skipped %d unreadable page(s) in writable regions", skipped_pages);
    }
    dbg("[OK]", "scan complete: player=%d party=%d buddy_ai=%d mover=%d anim=%d/%d/%d",
        g_class_counts[0], g_class_counts[1], g_class_counts[2], g_class_counts[3],
        g_class_counts[4], g_class_counts[5], g_class_counts[6]);
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
// First plausible position triple in an object: the same rule the log uses, returned instead.
bool read_first_triple(uintptr_t address, float out[3], unsigned* offset_out)
{
    unsigned char raw[512];
    if (!safe_read_quiet(address, raw, sizeof(raw))) {
        return false;
    }
    return find_triple(raw, sizeof(raw), out, offset_out);
}
// Offset of the triple reported by `position`, so a test can nudge it and watch the character
// move (the only honest way to prove a float triple is the player's position).
unsigned g_position_offset = 0;
bool g_position_known = false;
// --- in-game bridge ---------------------------------------------------------
//
// The hub's tools/bridge server, embedded: a loopback RPC that runs game calls on the tick
// thread and answers `state` with what the scan found. This is the channel later tests and
// bots use (playbook/in-game-bridge.md); it does not depend on the frame hook.
uintptr_t g_player_hits[8];
int g_player_count = 0;
uintptr_t g_mover_hits[8];
int g_mover_count = 0;
uintptr_t g_party_hits[8];
int g_party_count = 0;
uintptr_t g_buddy_hits[8];
int g_buddy_count = 0;
uintptr_t g_anim_hits[8];
int g_anim_count = 0;
coop::Net* g_net = nullptr;

void sync_bridge_lists()
{
    g_player_count = g_class_counts[0];
    memcpy(g_player_hits, g_class_hits[0], sizeof(g_player_hits));
    g_party_count = g_class_counts[1];
    memcpy(g_party_hits, g_class_hits[1], sizeof(g_party_hits));
    g_buddy_count = g_class_counts[2];
    memcpy(g_buddy_hits, g_class_hits[2], sizeof(g_buddy_hits));
    g_mover_count = g_class_counts[3];
    memcpy(g_mover_hits, g_class_hits[3], sizeof(g_mover_hits));
    g_anim_count = 0;
    for (int kind = 4; kind < kClassCount; ++kind) {
        for (int index = 0; index < g_class_counts[kind] && g_anim_count < 8; ++index) {
            g_anim_hits[g_anim_count++] = g_class_hits[kind][index];
        }
    }
}

class CoopAdapter : public coopbridge::Adapter
{
public:
    coopbridge::Capabilities capabilities() const override
    {
        coopbridge::Capabilities caps;
        caps.state = true;
        caps.exec = true;
        caps.eval = true;
        return caps;
    }
    coopbridge::json::Value State(const std::vector<std::string>&) override
    {
        using coopbridge::json::Value;
        Value root = Value::object();
        root.set("pid", static_cast<double>(GetCurrentProcessId()));
        root.set("ticks", static_cast<double>(g_ticks));
        Value players = Value::array();
        for (int index = 0; index < g_player_count; ++index) {
            players.push(Value(static_cast<double>(g_player_hits[index])));
        }
        root.set("players", players);
        Value movers = Value::array();
        for (int index = 0; index < g_mover_count; ++index) {
            movers.push(Value(static_cast<double>(g_mover_hits[index])));
        }
        root.set("movers", movers);
        Value party = Value::array();
        for (int index = 0; index < g_party_count; ++index) {
            party.push(Value(static_cast<double>(g_party_hits[index])));
        }
        root.set("party", party);
        Value buddy_ai = Value::array();
        for (int index = 0; index < g_buddy_count; ++index) {
            buddy_ai.push(Value(static_cast<double>(g_buddy_hits[index])));
        }
        root.set("buddy_ai", buddy_ai);
        Value anims = Value::array();
        for (int index = 0; index < g_anim_count; ++index) {
            anims.push(Value(static_cast<double>(g_anim_hits[index])));
        }
        root.set("anim_controllers", anims);
        root.set("debug_log", "coop_debug.log");
        if (g_net != nullptr) {
            Value net = Value::object();
            net.set("sent", static_cast<double>(g_net->SentCount()));
            net.set("received", static_cast<double>(g_net->ReceivedCount()));
            net.set("has_peer", g_net->HasPeer());
            const DWORD now = GetTickCount();
            net.set("peer_age_ms", static_cast<double>(g_net->PeerAgeMs(now)));
            coop::PlayerState peer = g_net->Peer();
            Value peer_state = Value::object();
            peer_state.set("x", static_cast<double>(peer.x));
            peer_state.set("y", static_cast<double>(peer.y));
            peer_state.set("z", static_cast<double>(peer.z));
            peer_state.set("yaw", static_cast<double>(peer.yaw));
            net.set("peer", peer_state);
            root.set("net", net);
        }
        if (g_player_count > 0) {
            float values[3] = {0.0f, 0.0f, 0.0f};
            unsigned offset = 0;
            if (read_first_triple(g_player_hits[0], values, &offset)) {
                Value position = Value::object();
                position.set("x", static_cast<double>(values[0]));
                position.set("y", static_cast<double>(values[1]));
                position.set("z", static_cast<double>(values[2]));
                position.set("offset", static_cast<double>(offset));
                position.set("object", static_cast<double>(g_player_hits[0]));
                position.set("cached", true);
                root.set("position", position);
                g_position_offset = offset;
                g_position_known = true;
            }
            float live_values[3] = {0.0f, 0.0f, 0.0f};
            uintptr_t hero_mover = 0;
            unsigned live_offset = 0;
            if (resolve_hero_position(live_values, &hero_mover, &live_offset)) {
                Value live = Value::object();
                live.set("x", static_cast<double>(live_values[0]));
                live.set("y", static_cast<double>(live_values[1]));
                live.set("z", static_cast<double>(live_values[2]));
                live.set("offset", static_cast<double>(live_offset));
                live.set("mover", static_cast<double>(hero_mover));
                live.set("source", "mover+0x12c, motion-proven 2026-11-16 run 28c1ea9e");
                root.set("position_live", live);
            }
        }
        return root;
    }
    // A test hook, not a player feature: `tp dx dy dz` shifts the triple the scan reported, so
    // "is this really the player's position?" is answered by watching the character move.
    coopbridge::ExecResult Exec(const std::string& line, const std::string&) override
    {
        if (line.rfind("host ", 0) == 0) {
            if (g_net == nullptr) {
                g_net = new coop::Net();
            }
            const int port = std::atoi(line.c_str() + 5);
            if (port <= 0 || port > 65535) {
                return {false, "usage: host <port>"};
            }
            if (!g_net->Listen(static_cast<uint16_t>(port))) {
                return {false, g_net->LastError()};
            }
            return {true, "listening on " + std::to_string(port)};
        }
        if (line.rfind("join ", 0) == 0) {
            if (g_net == nullptr) {
                g_net = new coop::Net();
            }
            char host[64] = {};
            int port = 0;
            if (std::sscanf(line.c_str() + 5, "%63s %d", host, &port) != 2 || port <= 0 ||
                port > 65535) {
                return {false, "usage: join <host> <port>"};
            }
            if (!g_net->Connect(host, static_cast<uint16_t>(port))) {
                return {false, g_net->LastError()};
            }
            return {true, "sending to " + std::string(host) + ":" + std::to_string(port)};
        }
        if (line == "net") {
            if (g_net == nullptr) {
                return {false, "transport not started"};
            }
            return {true, "sent=" + std::to_string(g_net->SentCount()) +
                              " received=" + std::to_string(g_net->ReceivedCount())};
        }
        float dx = 0.0f;
        float dy = 0.0f;
        float dz = 0.0f;
        if (line == "hooks graphics") {
            install_present_hook();
            install_dxgi_hook();
            return {true, "graphics hooks requested"};
        }
        if (line == "hooks inline") {
            install_inline_hooks();
            return {true, "inline hooks requested"};
        }
        if (line == "probe") {
            scan_objects();
            sync_bridge_lists();
            log_snapshot("bridge probe");
            return {true, "scan and snapshot written to coop_debug.log"};
        }
        if (std::sscanf(line.c_str(), "tp %f %f %f", &dx, &dy, &dz) != 3) {
            return {false, "usage: tp dx dy dz | probe | hooks graphics | hooks inline"};
        }
        if (!g_position_known || g_player_count == 0) {
            return {false, "no player position yet"};
        }
        float current[3] = {0.0f, 0.0f, 0.0f};
        unsigned offset = 0;
        if (!read_first_triple(g_player_hits[0], current, &offset)) {
            return {false, "no plausible triple in the object"};
        }
        current[0] += dx;
        current[1] += dy;
        current[2] += dz;
        WriteProcessMemory(GetCurrentProcess(),
                           reinterpret_cast<void*>(g_player_hits[0] + offset), current,
                           sizeof(current), nullptr);
        return {true, "tp applied"};
    }
    // A few named expressions, so a test can ask for one number without a state dump.
    coopbridge::json::Value Eval(const std::string& expr, const std::string&) override
    {
        using coopbridge::json::Value;
        if (expr == "ticks") {
            return Value(static_cast<double>(g_ticks));
        }
        if (expr == "player_count") {
            return Value(static_cast<double>(g_player_count));
        }
        if (expr == "mover_count") {
            return Value(static_cast<double>(g_mover_count));
        }
        if (expr == "party_count") {
            return Value(static_cast<double>(g_party_count));
        }
        if (expr == "buddy_count") {
            return Value(static_cast<double>(g_buddy_count));
        }
        if (expr == "anim_count") {
            return Value(static_cast<double>(g_anim_count));
        }
        return Value();
    }
};
// Created in the worker, never at DLL load: a constructor that touches sockets or threads runs
// under the loader lock otherwise, and the process can hang before Frida can even attach.
CoopAdapter* g_adapter = nullptr;
coopbridge::Server* g_server = nullptr;
bool g_bridge_started = false;
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
    wchar_t debug_path[MAX_PATH];
    lstrcpyW(debug_path, path);
    wchar_t* debug_name = wcsrchr(debug_path, L'\\');
    if (debug_name != nullptr) {
        lstrcpyW(debug_name + 1, L"coop_debug.log");
        g_debug = _wfopen(debug_path, L"w");
    }
    dbg("[ADDR]", "--- coop debug probe: pid=%lu build=228353 exe=%ls ---",
        static_cast<unsigned long>(GetCurrentProcessId()), debug_path);
    dbg("[ADDR]", "safety: every read is checked with VirtualQuery and ReadProcessMemory; "
                  "MinGW has no __try/__except, a bad pointer is an [ERR] line, never a crash");
    verify_rtti_classes();
    // Nothing is hooked at startup: creating a dummy device while the engine initialises its
    // own graphics is exactly what killed the process on 2026-11-15. Hooks are installed on
    // demand from the bridge (`exec "hooks d3d11"`), once the game is already up.
    wchar_t hooks_env[32] = {};
    if (GetEnvironmentVariableW(L"SP_COOP_HOOKS", hooks_env, 32) > 0) {
        if (lstrcmpiW(hooks_env, L"all") == 0 || lstrcmpiW(hooks_env, L"graphics") == 0) {
            install_present_hook();
            install_dxgi_hook();
        }
        if (lstrcmpiW(hooks_env, L"all") == 0) {
            install_inline_hooks();
        }
    }
    coopbridge::Options options;
    options.game = "south-park";
    options.build = "228353";
    options.tcpPort = 27990;
    options.endpointFile = "coop-bridge.json";
    g_adapter = new CoopAdapter();
    g_server = new coopbridge::Server();
    if (g_server != nullptr && g_adapter != nullptr && g_server->Start(options, *g_adapter)) {
        g_bridge_started = true;
        log_line("bridge: tcp://127.0.0.1:27990");
    } else {
        log_line("bridge: start failed");
    }
    // The thread still ticks, so a run is observable even before the first frame hook fires.
    LONG next_scan = 60;
    for (;;) {
        ++g_ticks;
        if ((g_ticks % 300) == 0) {
            log_value("ticks", g_ticks);
        }
        if (g_ticks >= next_scan) {
            next_scan = g_ticks + 600;
            scan_objects();
            sync_bridge_lists();
            log_snapshot("periodic");
        }
        if (g_bridge_started && g_server != nullptr) {
            g_server->Pump();
        }
        if (g_net != nullptr) {
            const DWORD now = GetTickCount();
            g_net->Poll(now);
            coop::PlayerState state;
            if (g_player_count > 0) {
                float values[3] = {0.0f, 0.0f, 0.0f};
                unsigned offset = 0;
                if (read_first_triple(g_player_hits[0], values, &offset)) {
                    state.x = values[0];
                    state.y = values[1];
                    state.z = values[2];
                }
            }
            g_net->Publish(state, now);
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
        // Memory-only: patch the exe's GetProcAddress slot. OFF by default: it runs before the
        // engine's own init and a wrong wrapper there kills startup (2026-11-15: the game never
        // got as far as an attach). Enable with SP_COOP_IAT=1 when that path is being tested.
        wchar_t iat[8] = {};
        if (GetEnvironmentVariableW(L"SP_COOP_IAT", iat, 8) > 0 && lstrcmpW(iat, L"1") == 0) {
            hook_get_proc_address();
        }
    }
    return TRUE;
}
