// The mod core, loaded by the proxy. Stage 1 of the hub's build order: prove that our code
// runs inside the game, ticks, and can read the local player's position.
//
// Nothing here writes to the game. File I/O and threads start from DllMain only by handing
// work to a thread: opening a file under the loader lock has hung a game before (The Witness).
#include <windows.h>

#include <cstdio>

namespace {

FILE* g_log = nullptr;
volatile LONG g_ticks = 0;
char g_notes[4096];
size_t g_notes_length = 0;

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

    // Stage 1 exit test: a steady tick inside the game. The per-frame hook (D3D9 Present)
    // and the player position read come next; this proves the load path first.
    for (;;) {
        ++g_ticks;
        if ((g_ticks % 60) == 0) {
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
