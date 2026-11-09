/* Real Windows API checks, using only isolated files and this test's own memory. */
#define main observer_main_not_run
#include "../testkit/provisions-watch/watch.c"
#undef main

static int failures;
#define CHECK(c)                                                                                             \
    do {                                                                                                     \
        if (!(c)) {                                                                                          \
            printf("FAIL line %d: %s\n", __LINE__, #c);                                                      \
            failures++;                                                                                      \
        }                                                                                                    \
    } while (0)
int wmain(int argc, wchar_t **argv) {
    if (argc != 2) return 2;
    wchar_t path[MAX_PATH], config[MAX_PATH];
    CHECK(config_path(L"D:\\some game\\_windows\\win64\\Darkest.exe", config));
    CHECK(!wcscmp(config, L"D:\\some game\\mods\\DDCoop\\ddcoop.ini"));
    CHECK(config_path(L"D:\\_WINDOWS\\WIN64\\DARKEST.EXE", config));
    CHECK(!config_path(L"D:\\some game\\Darkest.exe", config));
    CHECK(!config_path(L"D:\\some game\\_windows\\win32\\Darkest.exe", config));
    CHECK(_snwprintf_s(path, MAX_PATH, _TRUNCATE, L"%s\\isolated-hash.txt", argv[1]) >= 0);
    FILE *file = NULL;
    CHECK(!_wfopen_s(&file, path, L"wb") && file);
    if (!file) return 1;
    CHECK(fwrite("abc", 1, 3, file) == 3);
    CHECK(!fclose(file));
    char hash[65];
    CHECK(file_hash(path, hash));
    CHECK(!strcmp(hash, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    CHECK(!file_hash(L"Z:\\__ddcoop_test_missing__\\never-created", hash));
    CHECK(_snwprintf_s(config, MAX_PATH, _TRUNCATE, L"%s\\isolated-config.ini", argv[1]) >= 0);
    CHECK(DeleteFileW(config) || GetLastError() == ERROR_FILE_NOT_FOUND);
    CHECK(!guest_config(config));
    CHECK(WritePrivateProfileStringW(L"net", L"role", L"host", config));
    CHECK(!guest_config(config));
    CHECK(WritePrivateProfileStringW(L"net", L"role", L"guest", config));
    CHECK(guest_config(config));
    CHECK(WritePrivateProfileStringW(L"net", L"role", L"GUEST", config));
    CHECK(guest_config(config));
    CHECK(WritePrivateProfileStringW(L"net", L"role", L"guest-more", config));
    CHECK(!guest_config(config));
    CHECK(DeleteFileW(config));
    CHECK(DeleteFileW(path));
    uint64_t sentinel = 0x1234567812345678ULL, result = 0;
    CHECK(read_process(GetCurrentProcess(), (uint64_t)(uintptr_t)&sentinel, &result, sizeof result));
    CHECK(result == sentinel);
    CHECK(!read_process(GetCurrentProcess(), 1, &result, sizeof result));
    printf("provisions watch: CNG SHA256 vector, config path/guest gate, real self-process read only: %s\n",
           failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
