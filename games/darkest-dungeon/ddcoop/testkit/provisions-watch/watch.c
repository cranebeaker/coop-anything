/* Standalone external READ-ONLY observer. No hooks, injection, network, save access or game writes. */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <tlhelp32.h>
#include <bcrypt.h>
#include <shlobj.h>
#include <conio.h>
#include <fcntl.h>
#include <io.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "dd_provisions.h"

#define GAME_HASH "f9d914ca1d7d9ad3ff4f151a46e4853e532ce1f5f109f700b0c1381a3f7e0d12"
#define RUNTIME_HASH "1f12d320623ee3eaf82e41cdfe2e289ff9b7b1bfd1bd0527df66a86d865f3473"
static volatile LONG stopped;
static FILE *log_file;
static int log_failed;
static BOOL WINAPI ctrl(DWORD event) {
    if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT || event == CTRL_CLOSE_EVENT) {
        InterlockedExchange(&stopped, 1);
        return TRUE;
    }
    return FALSE;
}
static void record(const char *format, ...) {
    if (!log_file || log_failed) return;
    va_list args;
    va_start(args, format);
    int n = vfprintf(log_file, format, args);
    va_end(args);
    if (n < 0 || fflush(log_file)) log_failed = 1;
}
static int read_process(void *ctx, uint64_t at, void *out, size_t n) {
    SIZE_T got = 0;
    return ReadProcessMemory((HANDLE)ctx, (const void *)(uintptr_t)at, out, n, &got) && got == n;
}
static int file_hash(const wchar_t *path, char hex[65]) {
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    unsigned char data[65536], digest[32];
    int ok = BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0) >= 0;
    ok = ok && BCryptCreateHash(alg, &hash, NULL, 0, NULL, 0, 0) >= 0;
    DWORD count = 0;
    while (ok) {
        if (!ReadFile(file, data, sizeof data, &count, NULL)) {
            ok = 0;
            break;
        }
        if (!count) break;
        ok = BCryptHashData(hash, data, count, 0) >= 0;
    }
    ok = ok && BCryptFinishHash(hash, digest, sizeof digest, 0) >= 0;
    if (ok) {
        for (unsigned i = 0; i < 32; i++)
            sprintf_s(hex + i * 2, 65 - i * 2, "%02x", digest[i]);
        hex[64] = 0;
    }
    if (hash) BCryptDestroyHash(hash);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    CloseHandle(file);
    return ok;
}
static int guest_config(const wchar_t *path) {
    wchar_t role[32];
    DWORD n = GetPrivateProfileStringW(L"net", L"role", L"", role, 32, path);
    return n == 5 && !_wcsicmp(role, L"guest");
}
static int config_path(const wchar_t *exe, wchar_t config[MAX_PATH]) {
    static const wchar_t suffix[] = L"\\_windows\\win64\\Darkest.exe";
    size_t length = wcslen(exe), tail = wcslen(suffix);
    if (length < tail || _wcsicmp(exe + length - tail, suffix)) return 0;
    if (length - tail + wcslen(L"\\mods\\DDCoop\\ddcoop.ini") >= MAX_PATH) return 0;
    wmemcpy(config, exe, length - tail);
    config[length - tail] = 0;
    return wcscat_s(config, MAX_PATH, L"\\mods\\DDCoop\\ddcoop.ini") == 0;
}
/* No logs of full paths, PID, Steam identity, addresses or configuration credentials. */
static int find_game(DWORD *pid) {
    HANDLE list = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (list == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W entry = {0};
    entry.dwSize = sizeof entry;
    unsigned found = 0;
    for (BOOL more = Process32FirstW(list, &entry); more; more = Process32NextW(list, &entry)) {
        if (!_wcsicmp(entry.szExeFile, L"Darkest.exe")) {
            *pid = entry.th32ProcessID;
            found++;
        }
    }
    CloseHandle(list);
    return found == 1;
}
static int modules(DWORD pid, uint64_t *image, wchar_t exe[MAX_PATH], wchar_t runtime[MAX_PATH]) {
    HANDLE list = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (list == INVALID_HANDLE_VALUE) return 0;
    MODULEENTRY32W entry = {0};
    entry.dwSize = sizeof entry;
    unsigned games = 0, runtimes = 0;
    for (BOOL more = Module32FirstW(list, &entry); more; more = Module32NextW(list, &entry)) {
        if (!_wcsicmp(entry.szModule, L"Darkest.exe")) {
            *image = (uint64_t)(uintptr_t)entry.modBaseAddr;
            if (wcscpy_s(exe, MAX_PATH, entry.szExePath)) break;
            games++;
        } else if (!_wcsicmp(entry.szModule, L"ddcoop-runtime.dll")) {
            if (wcscpy_s(runtime, MAX_PATH, entry.szExePath)) break;
            runtimes++;
        }
    }
    CloseHandle(list);
    return games == 1 && runtimes == 1;
}
static int open_log(void) {
    PWSTR desktop = NULL;
    wchar_t dir[MAX_PATH], path[MAX_PATH];
    if (FAILED(SHGetKnownFolderPath(&FOLDERID_Desktop, 0, NULL, &desktop))) return 0;
    int ok = _snwprintf_s(dir, MAX_PATH, _TRUNCATE, L"%s\\DDCoop-metingen", desktop) >= 0;
    CoTaskMemFree(desktop);
    if (!ok || (!CreateDirectoryW(dir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)) return 0;
    SYSTEMTIME time;
    GetLocalTime(&time);
    if (_snwprintf_s(path, MAX_PATH, _TRUNCATE, L"%s\\DDCoop-voorraad-%04u%02u%02u-%02u%02u%02u-%03u.log",
                     dir, time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond,
                     time.wMilliseconds) < 0)
        return 0;
    HANDLE file =
        CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    int fd = _open_osfhandle((intptr_t)file, _O_WRONLY | _O_TEXT);
    if (fd == -1) {
        CloseHandle(file);
        return 0;
    }
    log_file = _fdopen(fd, "w");
    if (!log_file) {
        _close(fd);
        return 0;
    }
    return 1;
}
static void snapshot(unsigned n, ULONGLONG elapsed, const dd_provisions_snapshot *s) {
    record("snapshot=%u time_ms=%llu visible=%u stock_view=%u bag_view=%u context_1838=%08x\n", n,
           (unsigned long long)elapsed, s->panel_visible, s->stock_view, s->bag_view, s->context_1838);
    record("stock slots=%u occupied=%u\n", s->stock_slots, s->stock_occupied);
    for (uint32_t i = 0; i < s->stock_slots; i++)
        if (s->stock[i].quantity)
            record(" stock[%u] type=%08x id=%08x qty=%d\n", i, s->stock[i].type, s->stock[i].id,
                   s->stock[i].quantity);
    record("bag slots=%u occupied=%u\n", s->bag_slots, s->bag_occupied);
    for (uint32_t i = 0; i < s->bag_slots; i++)
        if (s->bag[i].quantity)
            record(" bag[%u] type=%08x id=%08x qty=%d\n", i, s->bag[i].type, s->bag[i].id,
                   s->bag[i].quantity);
    for (uint32_t i = 0; i < s->currencies; i++)
        record(" wallet[%u] type=%08x qty=%d\n", i, s->wallet[i].type, s->wallet[i].quantity);
    printf("Meting %u opgeslagen (voorraadscherm %s).\n", n, s->panel_visible ? "open" : "dicht");
}
static int pause_error(const char *message) {
    puts(message);
    puts("Er is niets in de game of saves gewijzigd. Druk op Enter om te sluiten.");
    (void)getchar();
    return 1;
}
int main(void) {
    DWORD pid = 0;
    uint64_t image = 0;
    wchar_t exe[MAX_PATH], runtime[MAX_PATH], config[MAX_PATH];
    char hash[65];
    SetConsoleTitleW(L"DDCoop voorraadmeting - alleen lezen");
    puts("DDCoop voorraadmeting test14b - alleen lezen, naast test13b.\n");
    puts("Start dit op de GAST-PC nadat de geleende campagne in de Hamlet geladen is.");
    puts("Host blijft in de Hamlet. Geen missie starten. Lees LEESMIJ.txt voor de stappen.\n");
    if (!find_game(&pid))
        return pause_error("Start precies een Darkest Dungeon via DDCoop test13b en probeer opnieuw.");
    HANDLE process =
        OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid);
    if (!process)
        return pause_error(
            "Kan het spel niet alleen-lezen openen. Start beide programma's als dezelfde Windows-gebruiker.");
    if (!modules(pid, &image, exe, runtime) || !config_path(exe, config) || !file_hash(exe, hash) ||
        strcmp(hash, GAME_HASH) || !file_hash(runtime, hash) || strcmp(hash, RUNTIME_HASH)) {
        CloseHandle(process);
        return pause_error(
            "Deze meting vereist de ondersteunde game28385 en de geladen DDCoop-runtime test13b.");
    }
    if (!guest_config(config)) {
        CloseHandle(process);
        return pause_error(
            "Deze meting is uitsluitend voor de gast. Laat de host met rust en start dit op de gast-PC.");
    }
    if (!open_log()) {
        CloseHandle(process);
        return pause_error("Kan de log niet maken in de map DDCoop-metingen op het bureaublad.");
    }
    record("DDCoop provisions read-only watch test14b\ngame_sha256=%s\nruntime_sha256=%s\n", GAME_HASH,
           RUNTIME_HASH);
    record("scope=guest slot3 normal-town supplies-only; no hooks, writes, network or saves\n");
    record("stable=two equal consecutive polls; observational, not atomic; no item names or secret fields\n");
    SetConsoleCtrlHandler(ctrl, TRUE);
    puts("Log staat op het bureaublad in DDCoop-metingen. Laat dit venster open.");
    puts("Doe de koop/terugverkoop/teruggaan-test. Wacht steeds twee seconden tussen acties.");
    puts("Kom daarna hier terug en druk op Q. Maximum meetduur: 20 minuten.\n");
    dd_provisions_snapshot current, candidate, last;
    memset(&candidate, 0, sizeof candidate);
    memset(&last, 0, sizeof last);
    int candidate_valid = 0, last_valid = 0, status = -1;
    const char *last_stage = "";
    unsigned captures = 0, rejected = 0;
    ULONGLONG start = GetTickCount64(), last_role = start;
    while (!InterlockedCompareExchange(&stopped, 0, 0) && GetTickCount64() - start < 20 * 60 * 1000) {
        if (WaitForSingleObject(process, 0) != WAIT_TIMEOUT) {
            record("stop=game-exited\n");
            break;
        }
        if (_kbhit()) {
            int key = _getch();
            if (key == 'q' || key == 'Q') {
                record("stop=user\n");
                break;
            }
        }
        ULONGLONG now = GetTickCount64();
        if (now - last_role >= 1000) {
            if (!guest_config(config)) {
                record("stop=guest-role-ended\n");
                break;
            }
            last_role = now;
        }
        const char *stage = NULL;
        dd_provisions_result result =
            dd_provisions_capture_ex(read_process, process, image, &current, &stage);
        if (result != DD_PROVISIONS_OK) {
            candidate_valid = 0;
            rejected++;
            if ((int)result != status || strcmp(last_stage, stage)) {
                record("time_ms=%llu status=%s stage=%s\n", (unsigned long long)(now - start),
                       dd_provisions_result_name(result), stage);
                printf("Status: %s (%s)\n", dd_provisions_result_name(result), stage);
                status = result;
                last_stage = stage;
            }
            if (result == DD_PROVISIONS_SLOT || result == DD_PROVISIONS_OTHER_MODE) {
                puts("Meting gestopt: alleen de normale geleende campagne in slot 3 is toegestaan.");
                record("stop=scope-refused\n");
                break;
            }
        } else {
            if (status != DD_PROVISIONS_OK) {
                record("time_ms=%llu status=ready\n", (unsigned long long)(now - start));
                status = DD_PROVISIONS_OK;
            }
            if (candidate_valid && !memcmp(&candidate, &current, sizeof current) &&
                (!last_valid || memcmp(&last, &current, sizeof current))) {
                snapshot(++captures, now - start, &current);
                last = current;
                last_valid = 1;
            }
            candidate = current;
            candidate_valid = 1;
        }
        if (log_failed) {
            puts("Schrijven van de log mislukt; meting gestopt.");
            break;
        }
        Sleep(50);
    }
    record("end snapshots=%u rejected_polls=%u elapsed_ms=%llu\n", captures, rejected,
           (unsigned long long)(GetTickCount64() - start));
    CloseHandle(process);
    SetConsoleCtrlHandler(ctrl, FALSE);
    int failed = log_failed;
    if (fclose(log_file)) failed = 1;
    log_file = NULL;
    printf("\n%u metingen. %s\n", captures,
           failed ? "Log kon niet volledig worden bewaard."
                  : "Log bewaard in DDCoop-metingen op het bureaublad.");
    puts("Sluit nu beide games en laat DDCoop de oorspronkelijke gast-save herstellen.");
    puts("Druk op Enter om dit venster te sluiten.");
    (void)getchar();
    return failed || !captures ? 1 : 0;
}
