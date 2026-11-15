// The mod core, loaded by the proxy. Stage 1 of the hub's build order: our code runs inside
// the game, ticks once per rendered frame, and can read the local player's position.
//
// The frame hook patches the D3D9 device vtable, the way games/the-witness does it: a dummy
// device is created, its vtable is shared with the game's device, and slot 17 (Present) is
// replaced after the original pointer is saved. No code is overwritten, so there is no
// trampoline to get wrong, and the patch is visible to every device of the implementation.
#include <windows.h>

#include <d3d9.h>

#include <cstdio>

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

HRESULT WINAPI present_hook(IDirect3DDevice9* device, const RECT* source, const RECT* dest,
                            HWND window, const RGNDATA* dirty)
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
    return g_present(device, source, dest, window, dirty);
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
    device->Release();
    DestroyWindow(window);
    d3d->Release();
    return g_present != nullptr;
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

    // The thread still ticks, so a run is observable even before the first frame hook fires.
    for (;;) {
        ++g_ticks;
        if ((g_ticks % 300) == 0) {
            log_value("ticks", g_ticks);
        }
        Sleep(16);
    }
}

}  // namespace

// Notes written before the worker opens its file are buffered and flushed by the worker.
extern "C" void coop_note(const char* text)
{
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
    }
    return TRUE;
}
