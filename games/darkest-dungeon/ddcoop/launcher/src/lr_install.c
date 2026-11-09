#define COBJMACROS
#include "lr_install.h"

#include <windows.h>
#include <bcrypt.h>
#include <objbase.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <tlhelp32.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lr_core.h"
#include "lr_files.h"
#include "lr_hashes.h"

#define TITLE L"DDCoop installation"
#define REG_KEY L"Software\\DDCoop"

static char g_report[8192];

static void note(const char *fmt, ...) {
    size_t n = strlen(g_report);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_report + n, sizeof g_report - n, fmt, ap);
    va_end(ap);
    n = strlen(g_report);
    if (n + 3 < sizeof g_report) strcat(g_report, "\r\n");
}

static wchar_t *W(const char *s) {
    static wchar_t buf[4][8192];
    static int next;
    wchar_t *w = buf[next++ & 3];
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w, 8192);
    return w;
}

static void to_utf8(const wchar_t *w, char *out, size_t cap) { WideCharToMultiByte(CP_UTF8, 0, w, -1, out, (int)cap, NULL, NULL); }

static int fail(const char *fmt, ...) {
    char text[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(text, sizeof text, fmt, ap);
    va_end(ap);
    char all[12000];
    snprintf(all, sizeof all, "%s%sERROR: %s\r\n\r\nNothing was damaged; you can try again.", g_report,
             g_report[0] ? "\r\n" : "", text);
    MessageBoxW(NULL, W(all), TITLE, MB_OK | MB_ICONERROR);
    return 1;
}

int lr_sha256_file(const char *path, char hex[65]) {
    uint8_t *data;
    size_t len;
    if (!lr_read_file(path, 512u * 1024 * 1024, &data, &len)) return 0;
    uint8_t digest[32];
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    int ok = BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0) == 0 &&
             BCryptCreateHash(alg, &hash, NULL, 0, NULL, 0, 0) == 0 && BCryptHashData(hash, data, (ULONG)len, 0) == 0 &&
             BCryptFinishHash(hash, digest, sizeof digest, 0) == 0;
    if (hash) BCryptDestroyHash(hash);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    free(data);
    if (!ok) return 0;
    for (int i = 0; i < 32; i++) sprintf(hex + 2 * i, "%02x", digest[i]);
    return 1;
}

static int has_hash(const char *path, const char *want) {
    char hex[65];
    return lr_sha256_file(path, hex) && strcmp(hex, want) == 0;
}

static int process_running(const wchar_t *exe) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe = {.dwSize = sizeof pe};
    int found = 0;
    DWORD me = GetCurrentProcessId(); /* the installer is DDCoop.exe itself */
    for (BOOL more = Process32FirstW(snap, &pe); more; more = Process32NextW(snap, &pe))
        if (_wcsicmp(pe.szExeFile, exe) == 0 && pe.th32ProcessID != me) found = 1;
    CloseHandle(snap);
    return found;
}

static int copy_file(const char *from, const char *to) {
    wchar_t a[LR_PATH_MAX], b[LR_PATH_MAX];
    wcscpy(a, W(from));
    wcscpy(b, W(to));
    if (!CopyFileW(a, b, FALSE)) return 0;
    /* CopyFile carries the "downloaded from the internet" mark along; without it SmartScreen stays quiet */
    wcscat(b, L":Zone.Identifier");
    DeleteFileW(b);
    return 1;
}

static int move_file(const char *from, const char *to) {
    wchar_t a[LR_PATH_MAX], b[LR_PATH_MAX];
    wcscpy(a, W(from));
    wcscpy(b, W(to));
    return MoveFileExW(a, b, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH) != 0;
}

int lr_repair_bootstrap(const char *game, const char *moddir) {
    char bin[LR_PATH_MAX], sdl[LR_PATH_MAX], orig[LR_PATH_MAX], seed[LR_PATH_MAX];
    if (process_running(L"Darkest.exe")) return 0;
    lr_join(bin, sizeof bin, game, "_windows\\win64");
    lr_join(sdl, sizeof sdl, bin, "SDL2.dll");
    lr_join(orig, sizeof orig, bin, "SDL2_orig.dll");
    lr_join(seed, sizeof seed, moddir, "SDL2-bootstrap.dll");
    int original=has_hash(sdl,LR_HASH_SDL2_ORIGINAL);
    if ((!original && !has_hash(sdl,LR_HASH_BOOTSTRAP_TEST11)) || !has_hash(seed, LR_HASH_BOOTSTRAP)) return 0;
    if (lr_file_exists(orig) && !has_hash(orig, LR_HASH_SDL2_ORIGINAL)) return -1;
    if(!lr_file_exists(orig) && !original)return -1; /* Never preserve our old proxy as the Steam original. */
    if (!lr_file_exists(orig) && !copy_file(sdl, orig)) return -1;
    return copy_file(seed, sdl) && has_hash(sdl, LR_HASH_BOOTSTRAP) ? 1 : -1;
}

/* ---- finding the game ---- */

static int is_game_dir(const char *dir) {
    char exe[LR_PATH_MAX];
    lr_join(exe, sizeof exe, dir, "_windows\\win64\\Darkest.exe");
    return lr_file_exists(exe);
}

static int steam_path(char *out, size_t cap) {
    wchar_t path[LR_PATH_MAX];
    DWORD size = sizeof path;
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath", RRF_RT_REG_SZ, NULL, path, &size) != 0)
        return 0;
    to_utf8(path, out, cap);
    for (char *c = out; *c; c++)
        if (*c == '/') *c = '\\';
    return 1;
}

/* Every Steam library listed in steamapps\libraryfolders.vdf ("path" "D:\\SteamLibrary"), plus Steam itself. */
static int find_in_steam(char *out, size_t cap) {
    char steam[LR_PATH_MAX], vdf[LR_PATH_MAX], cand[LR_PATH_MAX];
    if (!steam_path(steam, sizeof steam)) return 0;
    lr_join(cand, sizeof cand, steam, "steamapps\\common\\DarkestDungeon");
    if (is_game_dir(cand)) {
        snprintf(out, cap, "%s", cand);
        return 1;
    }
    lr_join(vdf, sizeof vdf, steam, "steamapps\\libraryfolders.vdf");
    uint8_t *data;
    size_t len;
    if (!lr_read_file(vdf, 4u * 1024 * 1024, &data, &len)) return 0;
    char *text = realloc(data, len + 1);
    text[len] = 0;
    int found = 0;
    for (char *p = strstr(text, "\"path\""); p && !found; p = strstr(p + 6, "\"path\"")) {
        char *q = strchr(p + 6, '"');
        if (!q) break;
        char lib[LR_PATH_MAX];
        size_t n = 0;
        for (q++; *q && *q != '"' && n + 1 < sizeof lib; q++) {
            if (q[0] == '\\' && q[1] == '\\') q++;
            lib[n++] = *q;
        }
        lib[n] = 0;
        lr_join(cand, sizeof cand, lib, "steamapps\\common\\DarkestDungeon");
        if (is_game_dir(cand)) {
            snprintf(out, cap, "%s", cand);
            found = 1;
        }
    }
    free(text);
    return found;
}

static int ask_folder(char *out, size_t cap) {
    MessageBoxW(NULL, L"Darkest Dungeon was not found automatically.\r\n\r\nSelect the game folder in the next window "
                      L"(the folder containing '_windows'). In Steam: right-click Darkest Dungeon > Manage > Browse local "
                      L"files.", TITLE, MB_OK | MB_ICONINFORMATION);
    CoInitialize(NULL);
    BROWSEINFOW bi = {0};
    bi.lpszTitle = L"Select the Darkest Dungeon folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return 0;
    wchar_t path[MAX_PATH];
    int ok = SHGetPathFromIDListW(pidl, path);
    CoTaskMemFree(pidl);
    if (!ok) return 0;
    to_utf8(path, out, cap);
    return is_game_dir(out);
}

static int find_game(const char *kit, char *out, size_t cap) {
    /* Prefer a game folder containing the extracted kit. This lets a test kit target
     * a copied install instead of a previously registered Steam install. */
    char p[LR_PATH_MAX];
    snprintf(p, sizeof p, "%s", kit);
    for (int up = 0; up < 4; up++) {
        if (is_game_dir(p)) {
            snprintf(out, cap, "%s", p);
            return 1;
        }
        char *s = strrchr(p, '\\');
        if (!s) break;
        *s = 0;
    }
    wchar_t saved[LR_PATH_MAX];
    DWORD size = sizeof saved;
    if (RegGetValueW(HKEY_CURRENT_USER, REG_KEY, L"GameDir", RRF_RT_REG_SZ, NULL, saved, &size) == 0) {
        to_utf8(saved, out, cap);
        if (is_game_dir(out)) return 1;
    }
    return find_in_steam(out, cap) || ask_folder(out, cap);
}

/* ---- shortcut ---- */

static void desktop_shortcut(const char *target, const char *workdir, int create) {
    wchar_t desktop[MAX_PATH], lnk[MAX_PATH];
    if (SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desktop) != S_OK) return;
    swprintf(lnk, MAX_PATH, L"%ls\\DDCoop.lnk", desktop);
    if (!create) {
        DeleteFileW(lnk);
        return;
    }
    CoInitialize(NULL);
    IShellLinkW *link = NULL;
    if (FAILED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&link))) return;
    IShellLinkW_SetPath(link, W(target));
    IShellLinkW_SetWorkingDirectory(link, W(workdir));
    IShellLinkW_SetDescription(link, L"DDCoop - Darkest Dungeon co-op");
    IPersistFile *file = NULL;
    if (SUCCEEDED(IShellLinkW_QueryInterface(link, &IID_IPersistFile, (void **)&file))) {
        IPersistFile_Save(file, lnk, TRUE);
        IPersistFile_Release(file);
    }
    IShellLinkW_Release(link);
}

/* ---- modes ---- */

static int install(const char *kit, int repair) {
    char game[LR_PATH_MAX];
    if (process_running(L"Darkest.exe")) return fail("Darkest Dungeon is still running. Close it and try again.");
    if (process_running(L"DDCoop.exe"))
        return fail("DDCoop is still running. Close its window and try again.");
    char files[LR_PATH_MAX], src_boot[LR_PATH_MAX], src_rt[LR_PATH_MAX], src_exe[LR_PATH_MAX];
    snprintf(files, sizeof files, "%s", kit);
    lr_join(src_boot, sizeof src_boot, files, "SDL2.dll");
    lr_join(src_rt, sizeof src_rt, files, "ddcoop-runtime.dll");
    lr_join(src_exe, sizeof src_exe, files, "DDCoop.exe");
    if (!has_hash(src_boot, LR_HASH_BOOTSTRAP) || !has_hash(src_rt, LR_HASH_RUNTIME))
        return fail("Files in this ZIP are missing or damaged. Extract the entire ZIP again, "
                    "including all files, and try again.");
    if (!find_game(kit, game, sizeof game)) return fail("Darkest Dungeon was not found. Is it installed through Steam?");
    note("Game found: %s", game);

    char bin[LR_PATH_MAX], exe[LR_PATH_MAX], sdl[LR_PATH_MAX], orig[LR_PATH_MAX];
    lr_join(bin, sizeof bin, game, "_windows\\win64");
    lr_join(exe, sizeof exe, bin, "Darkest.exe");
    lr_join(sdl, sizeof sdl, bin, "SDL2.dll");
    lr_join(orig, sizeof orig, bin, "SDL2_orig.dll");
    if (!has_hash(exe, LR_HASH_GAME))
        return fail("This Darkest Dungeon version is not supported (possibly a Steam update). An updated DDCoop "
                    "build is required; nothing was changed.");
    note("OK  game version verified");

    /* SDL2: the original becomes SDL2_orig.dll, the bootstrap takes its place. */
    if (has_hash(sdl, LR_HASH_SDL2_ORIGINAL)) {
        if (!copy_file(sdl, orig)) return fail("Could not create SDL2_orig.dll in %s (permissions?).", bin);
        if (!copy_file(src_boot, sdl)) return fail("Could not replace SDL2.dll in %s.", bin);
        note("OK  bootstrap installed (original kept as SDL2_orig.dll)");
    } else if (has_hash(sdl, LR_HASH_TOWN_PROBE_V12) || has_hash(sdl,LR_HASH_BOOTSTRAP_TEST11)) {
        if (!has_hash(orig, LR_HASH_SDL2_ORIGINAL))
            return fail("SDL2.dll still contains a research probe, but SDL2_orig.dll is missing or unknown. Restore "
                        "the original game files first; nothing was changed.");
        if (!copy_file(src_boot, sdl)) return fail("Could not replace the research probe with the DDCoop bootstrap.");
        note("OK  bekend vorig opstartbestand vervangen; SDL2_orig.dll is intact");
    } else if (has_hash(sdl, LR_HASH_BOOTSTRAP)) {
        if (!has_hash(orig, LR_HASH_SDL2_ORIGINAL))
            return fail("SDL2_orig.dll is missing. Ask Steam to verify game files (right-click Darkest Dungeon > "
                        "Properties > Installed Files > Verify integrity), then run Repair.");
        note("OK  opstartbestand stond er al");
    } else {
        return fail("The game's SDL2.dll is unknown (another mod?). Ask Steam to verify game files and "
                    "try again.");
    }

    char mods[LR_PATH_MAX], launcher[LR_PATH_MAX], dst[LR_PATH_MAX];
    lr_join(mods, sizeof mods, game, "mods\\DDCoop");
    lr_join(launcher, sizeof launcher, mods, "launcher");
    if (!lr_make_dirs(launcher)) return fail("Could not create folder %s.", launcher);
    lr_join(dst, sizeof dst, mods, "SDL2-bootstrap.dll");
    if (!copy_file(src_boot, dst) || !has_hash(dst, LR_HASH_BOOTSTRAP))
        return fail("Could not prepare automatic bootstrap repair.");
    lr_join(dst, sizeof dst, mods, "ddcoop-runtime.dll");
    if (!copy_file(src_rt, dst)) return fail("Could not write %s.", dst);
    lr_join(dst, sizeof dst, launcher, "DDCoop.exe");
    if (_stricmp(src_exe, dst) != 0 && !copy_file(src_exe, dst)) return fail("Could not write %s.", dst);
    char steam_src[LR_PATH_MAX];
    lr_join(steam_src, sizeof steam_src, bin, "steam_api64.dll");
    lr_join(dst, sizeof dst, launcher, "steam_api64.dll");
    if (!copy_file(steam_src, dst)) return fail("Could not copy steam_api64.dll.");
    lr_join(dst, sizeof dst, launcher, "steam_appid.txt");
    if (!lr_write_file(dst, (const uint8_t *)"480", 3)) return fail("Could not write steam_appid.txt.");
    lr_join(dst, sizeof dst, mods, "ddcoop.ini");
    if (!lr_write_ini(dst, "off", LR_DEFAULT_GAME_PORT, "unused-until-ddcoop-starts", 3))
        return fail("Could not write ddcoop.ini.");
    note("OK  DDCoop geplaatst in %s", mods);

    HKEY key;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL) == 0) {
        wchar_t w[LR_PATH_MAX];
        wcscpy(w, W(game));
        RegSetValueExW(key, L"GameDir", 0, REG_SZ, (const BYTE *)w, (DWORD)((wcslen(w) + 1) * sizeof(wchar_t)));
        RegCloseKey(key);
    }
    char target[LR_PATH_MAX];
    lr_join(target, sizeof target, launcher, "DDCoop.exe");
    desktop_shortcut(target, launcher, 1);
    note("OK  DDCoop desktop shortcut created");
    note("");
    note("Starting normally through Steam still works as before (co-op is then off).");
    note("For co-op, open DDCoop from the desktop. Open it now?");
    if (MessageBoxW(NULL, W(g_report), repair ? L"DDCoop gerepareerd" : L"DDCoop geïnstalleerd", MB_YESNO | MB_ICONINFORMATION) == IDYES)
        ShellExecuteW(NULL, L"open", W(target), NULL, W(launcher), SW_SHOWNORMAL);
    return 0;
}

static int uninstall(const char *kit) {
    char game[LR_PATH_MAX];
    if (process_running(L"Darkest.exe")) return fail("Darkest Dungeon is still running. Close it and try again.");
    if (process_running(L"DDCoop.exe")) return fail("DDCoop is still running. Close its window and try again.");
    if (!find_game(kit, game, sizeof game)) return fail("Darkest Dungeon was not found.");
    char bin[LR_PATH_MAX], sdl[LR_PATH_MAX], orig[LR_PATH_MAX], mods[LR_PATH_MAX], gone[LR_PATH_MAX];
    lr_join(mods, sizeof mods, game, "mods\\DDCoop");
    lr_join(gone, sizeof gone, mods, "ddcoop-guest-original.bin");
    if (lr_file_exists(gone))
        return fail("A guest campaign still needs recovery. Open DDCoop first; it will restore your own "
                    "campaign automatically.");
    lr_join(bin, sizeof bin, game, "_windows\\win64");
    lr_join(sdl, sizeof sdl, bin, "SDL2.dll");
    lr_join(orig, sizeof orig, bin, "SDL2_orig.dll");
    if (has_hash(orig, LR_HASH_SDL2_ORIGINAL)) {
        if (!move_file(orig, sdl)) return fail("Could not restore SDL2.dll.");
        note("OK  original SDL2.dll restored");
    } else if (has_hash(sdl, LR_HASH_SDL2_ORIGINAL)) {
        note("OK  SDL2.dll was already the original");
    } else {
        return fail("The original SDL2.dll is missing. Ask Steam to verify game files to restore it.");
    }
    lr_join(mods, sizeof mods, game, "mods\\DDCoop");
    lr_join(gone, sizeof gone, game, "mods\\DDCoop.verwijderd");
    if (lr_dir_exists(mods)) {
        wchar_t a[LR_PATH_MAX], b[LR_PATH_MAX];
        wcscpy(a, W(mods));
        wcscpy(b, W(gone));
        SHFILEOPSTRUCTW op = {0};
        wchar_t from[LR_PATH_MAX + 2] = {0};
        wcscpy(from, b);
        op.wFunc = FO_DELETE;
        op.pFrom = from; /* an older DDCoop.verwijderd goes first */
        op.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
        if (lr_dir_exists(gone)) SHFileOperationW(&op);
        if (!MoveFileW(a, b)) return fail("Could not move mods\\DDCoop (is a window still open?).");
        note("OK  mods\\DDCoop moved to mods\\DDCoop.verwijderd (logs retained there)");
    }
    desktop_shortcut(NULL, NULL, 0);
    RegDeleteKeyW(HKEY_CURRENT_USER, REG_KEY);
    note("OK  desktop shortcut removed");
    note("");
    note("Darkest Dungeon is back to its Steam state. Saves were not changed; co-op backups "
         "are in DDCoop-backups under your user folder.");
    MessageBoxW(NULL, W(g_report), L"DDCoop removed", MB_OK | MB_ICONINFORMATION);
    return 0;
}

static int start(const char *kit) {
    char game[LR_PATH_MAX], target[LR_PATH_MAX], dir[LR_PATH_MAX];
    if (!find_game(kit, game, sizeof game)) return fail("Darkest Dungeon was not found. Run Install DDCoop.bat first.");
    lr_join(dir, sizeof dir, game, "mods\\DDCoop\\launcher");
    lr_join(target, sizeof target, dir, "DDCoop.exe");
    if (!lr_file_exists(target)) return fail("DDCoop is not installed. Run Install DDCoop.bat first.");
    ShellExecuteW(NULL, L"open", W(target), NULL, W(dir), SW_SHOWNORMAL);
    return 0;
}

int lr_install_main(const wchar_t *mode) {
    wchar_t self[LR_PATH_MAX];
    GetModuleFileNameW(NULL, self, LR_PATH_MAX);
    wchar_t *s = wcsrchr(self, L'\\');
    if (s) *s = 0;
    char kit[LR_PATH_MAX];
    to_utf8(self, kit, sizeof kit);
    g_report[0] = 0;
    if (wcscmp(mode, L"--install") == 0) return install(kit, 0);
    if (wcscmp(mode, L"--repair") == 0) return install(kit, 1);
    if (wcscmp(mode, L"--uninstall") == 0) return uninstall(kit);
    if (wcscmp(mode, L"--start") == 0) return start(kit);
    return fail("Onbekende opdracht.");
}
