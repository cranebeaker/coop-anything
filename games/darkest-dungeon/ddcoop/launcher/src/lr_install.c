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

#define TITLE L"DDCoop installatie"
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
    snprintf(all, sizeof all, "%s%sFOUT: %s\r\n\r\nEr is niets kapotgemaakt; je kunt het gewoon opnieuw proberen.", g_report,
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
    MessageBoxW(NULL, L"Darkest Dungeon is niet automatisch gevonden.\r\n\r\nKies in het volgende venster de map van het "
                      L"spel (de map met daarin '_windows'). In Steam: rechtsklik Darkest Dungeon > Beheren > Lokale "
                      L"bestanden doorbladeren.", TITLE, MB_OK | MB_ICONINFORMATION);
    CoInitialize(NULL);
    BROWSEINFOW bi = {0};
    bi.lpszTitle = L"Kies de map van Darkest Dungeon";
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
    if (process_running(L"Darkest.exe")) return fail("Darkest Dungeon draait nog. Sluit het spel en probeer opnieuw.");
    if (process_running(L"DDCoop.exe"))
        return fail("DDCoop is nog open. Sluit het DDCoop-venster en probeer opnieuw.");
    char files[LR_PATH_MAX], src_boot[LR_PATH_MAX], src_rt[LR_PATH_MAX], src_exe[LR_PATH_MAX];
    snprintf(files, sizeof files, "%s", kit);
    lr_join(src_boot, sizeof src_boot, files, "SDL2.dll");
    lr_join(src_rt, sizeof src_rt, files, "ddcoop-runtime.dll");
    lr_join(src_exe, sizeof src_exe, files, "DDCoop.exe");
    if (!has_hash(src_boot, LR_HASH_BOOTSTRAP) || !has_hash(src_rt, LR_HASH_RUNTIME))
        return fail("De bestanden in deze zip zijn niet compleet of beschadigd. Pak de zip opnieuw uit (alles, niet "
                    "alleen de .bat) en probeer opnieuw.");
    if (!find_game(kit, game, sizeof game)) return fail("Darkest Dungeon niet gevonden. Is het geïnstalleerd via Steam?");
    note("Spel gevonden: %s", game);

    char bin[LR_PATH_MAX], exe[LR_PATH_MAX], sdl[LR_PATH_MAX], orig[LR_PATH_MAX];
    lr_join(bin, sizeof bin, game, "_windows\\win64");
    lr_join(exe, sizeof exe, bin, "Darkest.exe");
    lr_join(sdl, sizeof sdl, bin, "SDL2.dll");
    lr_join(orig, sizeof orig, bin, "SDL2_orig.dll");
    if (!has_hash(exe, LR_HASH_GAME))
        return fail("Deze Darkest Dungeon-versie kent DDCoop niet (misschien een Steam-update). Er is een nieuwe DDCoop "
                    "nodig; er is niets veranderd.");
    note("OK  spelversie klopt");

    /* SDL2: the original becomes SDL2_orig.dll, the bootstrap takes its place. */
    if (has_hash(sdl, LR_HASH_SDL2_ORIGINAL)) {
        if (!copy_file(sdl, orig)) return fail("Kon SDL2_orig.dll niet maken in %s (rechten?).", bin);
        if (!copy_file(src_boot, sdl)) return fail("Kon SDL2.dll niet vervangen in %s.", bin);
        note("OK  opstartbestand geplaatst (origineel bewaard als SDL2_orig.dll)");
    } else if (has_hash(sdl, LR_HASH_TOWN_PROBE_V12) || has_hash(sdl,LR_HASH_BOOTSTRAP_TEST11)) {
        if (!has_hash(orig, LR_HASH_SDL2_ORIGINAL))
            return fail("De onderzoeksprobe staat nog in SDL2.dll, maar SDL2_orig.dll mist of is onbekend. Herstel eerst "
                        "de originele gamebestanden; er is niets veranderd.");
        if (!copy_file(src_boot, sdl)) return fail("Kon de onderzoeksprobe niet door het DDCoop-opstartbestand vervangen.");
        note("OK  bekend vorig opstartbestand vervangen; SDL2_orig.dll is intact");
    } else if (has_hash(sdl, LR_HASH_BOOTSTRAP)) {
        if (!has_hash(orig, LR_HASH_SDL2_ORIGINAL))
            return fail("SDL2_orig.dll ontbreekt. Laat Steam de bestanden controleren (rechtsklik Darkest Dungeon > "
                        "Eigenschappen > Geïnstalleerde bestanden > Integriteit controleren) en draai daarna Repair.");
        note("OK  opstartbestand stond er al");
    } else {
        return fail("SDL2.dll in de spelmap is onbekend (een andere mod?). Laat Steam de bestanden controleren en "
                    "probeer opnieuw.");
    }

    char mods[LR_PATH_MAX], launcher[LR_PATH_MAX], dst[LR_PATH_MAX];
    lr_join(mods, sizeof mods, game, "mods\\DDCoop");
    lr_join(launcher, sizeof launcher, mods, "launcher");
    if (!lr_make_dirs(launcher)) return fail("Kon de map %s niet maken.", launcher);
    lr_join(dst, sizeof dst, mods, "SDL2-bootstrap.dll");
    if (!copy_file(src_boot, dst) || !has_hash(dst, LR_HASH_BOOTSTRAP))
        return fail("Kon automatische bootstrap-reparatie niet klaarzetten.");
    lr_join(dst, sizeof dst, mods, "ddcoop-runtime.dll");
    if (!copy_file(src_rt, dst)) return fail("Kon %s niet schrijven.", dst);
    lr_join(dst, sizeof dst, launcher, "DDCoop.exe");
    if (_stricmp(src_exe, dst) != 0 && !copy_file(src_exe, dst)) return fail("Kon %s niet schrijven.", dst);
    char steam_src[LR_PATH_MAX];
    lr_join(steam_src, sizeof steam_src, bin, "steam_api64.dll");
    lr_join(dst, sizeof dst, launcher, "steam_api64.dll");
    if (!copy_file(steam_src, dst)) return fail("Kon steam_api64.dll niet kopiëren.");
    lr_join(dst, sizeof dst, launcher, "steam_appid.txt");
    if (!lr_write_file(dst, (const uint8_t *)"480", 3)) return fail("Kon steam_appid.txt niet schrijven.");
    lr_join(dst, sizeof dst, mods, "ddcoop.ini");
    if (!lr_write_ini(dst, "off", LR_DEFAULT_GAME_PORT, "unused-until-ddcoop-starts", 3))
        return fail("Kon ddcoop.ini niet schrijven.");
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
    note("OK  snelkoppeling 'DDCoop' op het bureaublad");
    note("");
    note("Gewoon spelen via Steam blijft werken zoals altijd (dan is co-op uit).");
    note("Co-op: open DDCoop (bureaublad). Wil je DDCoop nu openen?");
    if (MessageBoxW(NULL, W(g_report), repair ? L"DDCoop gerepareerd" : L"DDCoop geïnstalleerd", MB_YESNO | MB_ICONINFORMATION) == IDYES)
        ShellExecuteW(NULL, L"open", W(target), NULL, W(launcher), SW_SHOWNORMAL);
    return 0;
}

static int uninstall(const char *kit) {
    char game[LR_PATH_MAX];
    if (process_running(L"Darkest.exe")) return fail("Darkest Dungeon draait nog. Sluit het spel en probeer opnieuw.");
    if (process_running(L"DDCoop.exe")) return fail("DDCoop is nog open. Sluit het DDCoop-venster en probeer opnieuw.");
    if (!find_game(kit, game, sizeof game)) return fail("Darkest Dungeon niet gevonden.");
    char bin[LR_PATH_MAX], sdl[LR_PATH_MAX], orig[LR_PATH_MAX], mods[LR_PATH_MAX], gone[LR_PATH_MAX];
    lr_join(mods, sizeof mods, game, "mods\\DDCoop");
    lr_join(gone, sizeof gone, mods, "ddcoop-guest-original.bin");
    if (lr_file_exists(gone))
        return fail("Er staat nog een gastcampagne klaar om te herstellen. Open DDCoop eerst; het zet je eigen "
                    "campagne automatisch terug.");
    lr_join(bin, sizeof bin, game, "_windows\\win64");
    lr_join(sdl, sizeof sdl, bin, "SDL2.dll");
    lr_join(orig, sizeof orig, bin, "SDL2_orig.dll");
    if (has_hash(orig, LR_HASH_SDL2_ORIGINAL)) {
        if (!move_file(orig, sdl)) return fail("Kon SDL2.dll niet terugzetten.");
        note("OK  originele SDL2.dll teruggezet");
    } else if (has_hash(sdl, LR_HASH_SDL2_ORIGINAL)) {
        note("OK  SDL2.dll was al het origineel");
    } else {
        return fail("Het origineel van SDL2.dll ontbreekt. Laat Steam de bestanden controleren; dat zet het terug.");
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
        if (!MoveFileW(a, b)) return fail("Kon mods\\DDCoop niet weghalen (staat er een venster open?).");
        note("OK  mods\\DDCoop weggezet als mods\\DDCoop.verwijderd (logs blijven daar)");
    }
    desktop_shortcut(NULL, NULL, 0);
    RegDeleteKeyW(HKEY_CURRENT_USER, REG_KEY);
    note("OK  snelkoppeling verwijderd");
    note("");
    note("Darkest Dungeon is weer helemaal zoals Steam het leverde. Saves zijn niet aangeraakt; backups van co-op "
         "sessies staan in je gebruikersmap onder DDCoop-backups.");
    MessageBoxW(NULL, W(g_report), L"DDCoop verwijderd", MB_OK | MB_ICONINFORMATION);
    return 0;
}

static int start(const char *kit) {
    char game[LR_PATH_MAX], target[LR_PATH_MAX], dir[LR_PATH_MAX];
    if (!find_game(kit, game, sizeof game)) return fail("Darkest Dungeon niet gevonden. Draai eerst Install DDCoop.bat.");
    lr_join(dir, sizeof dir, game, "mods\\DDCoop\\launcher");
    lr_join(target, sizeof target, dir, "DDCoop.exe");
    if (!lr_file_exists(target)) return fail("DDCoop is nog niet geïnstalleerd. Draai eerst Install DDCoop.bat.");
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
