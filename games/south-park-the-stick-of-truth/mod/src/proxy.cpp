// dinput8.dll proxy: the game imports DirectInput8Create statically, so a DLL with this
// name in the game folder is loaded before any game code runs. It forwards the one export
// to the real system DLL and carries the mod.
#include <windows.h>
#include <unknwn.h>

typedef HRESULT(WINAPI* DirectInput8CreateFn)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);

extern "C" void coop_start_worker();
extern "C" void coop_note(const char* text);

static DirectInput8CreateFn real_create = nullptr;
static HMODULE real_module = nullptr;

static void load_real_dinput8()
{
    wchar_t path[MAX_PATH];
    UINT length = GetSystemDirectoryW(path, MAX_PATH);
    if (length == 0 || length + 16 >= MAX_PATH) {
        return;
    }
    lstrcatW(path, L"\\dinput8.dll");
    real_module = LoadLibraryW(path);
    if (real_module != nullptr) {
        real_create = reinterpret_cast<DirectInput8CreateFn>(
            reinterpret_cast<void*>(GetProcAddress(real_module, "DirectInput8Create")));
    }
}

extern "C" __declspec(dllexport) HRESULT WINAPI DirectInput8Create(HINSTANCE instance,
                                                                   DWORD version, REFIID riid,
                                                                   LPVOID* out, LPUNKNOWN outer)
{
    if (real_create == nullptr) {
        load_real_dinput8();
    }
    if (real_create == nullptr) {
        coop_note("DirectInput8Create: real dinput8.dll NOT loaded");
        return E_FAIL;
    }
    coop_note("DirectInput8Create: real dinput8.dll loaded");
    coop_start_worker();
    HRESULT result = real_create(instance, version, riid, out, outer);
    char line[96];
    wsprintfA(line, "DirectInput8Create returned 0x%08lx", static_cast<unsigned long>(result));
    coop_note(line);
    return result;
}
