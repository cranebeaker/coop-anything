/* DDCoop.exe: the co-op window for Darkest Dungeon (Windows). It runs next to the game, never inside it.
 *
 * It checks the install, lets you host or join a Steam friend, hands the session to lr_core (password,
 * campaign, ddcoop.ini), relays the co-op runtime's connection over Steam (lr_steam) and starts the game.
 * The hero list on the right is where each player ticks the heroes they command in the dungeon (lr_owners).
 * F9 brings the window up over the game. Everything runs on the window thread from one 10 ms timer. */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <bcrypt.h>
#include <commctrl.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tlhelp32.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lr_core.h"
#include "lr_hashes.h"
#include "lr_install.h"
#include "lr_snapshot.h"
#include "lr_prepare_win.h"
#include "lr_steam.h"

/* How long a session waits for Steam to serve a save the game just wrote (LR_PREPARE_UNREAD), trying every 3 s. */
#define LR_STEAM_WAIT_MS 90000u

#ifndef DDC_VERSION
#define DDC_VERSION "dev"
#endif

/* Optional research build only. Normal kits save host progress; guests always use -nosave. */
#ifndef DDC_TEST_NOSAVE
#define DDC_TEST_NOSAVE 0
#endif

enum {
    ID_HOST = 100, ID_JOIN, ID_INVITE, ID_JOIN_CODE, ID_COPY_CODE, ID_STOP, ID_LOGS, ID_FRIENDS, ID_CODE, ID_TIMER,
    ID_HOTKEY, ID_HEROES, ID_SLOT, ID_SAVE, ID_UNDO,
};

enum { CHK_GAME, CHK_VERSION, CHK_MOD, CHK_STEAM, CHK_SLOT, CHK_LOBBY, CHK_FRIEND, CHK_RUNTIME, CHK_COUNT };

static struct {
    HWND wnd, status, checks, friends, code, host, join, invite, join_code, copy_code, stop, logs, hint, slot;
    HWND save_box, undo;
    int saving;                 /* host: the game saves its progress in the chosen slot (guests never save) */
    int prepare_undo;           /* the running helper puts the chosen slot back as it was before a session */
    int chosen_slot, slot_busy; /* The player's own choice; a guest's session replaces cfg.coop_slot for its duration. */
    int new_campaign_slot;      /* The game is running alone, to make or finish the campaign in this slot. */
    HWND heroes, heroes_hint;
    HFONT font, big, party_font;
    char root[LR_PATH_MAX];      /* the game folder */
    char bin[LR_PATH_MAX];       /* _windows\win64 */
    char moddir[LR_PATH_MAX];    /* mods\DDCoop */
    char log_path[LR_PATH_MAX];
    FILE *log;
    int ok[CHK_COUNT];
    char check_text[CHK_COUNT][200];
    int install_ok, steam_ok;
    lr_core core;
    lr_friend friends_list[256];
    int friend_count;
    HANDLE game;
    HANDLE prepare;
    int prepare_restore, prepare_nosave, restore_failed, want_close;
    uint64_t prepare_since;
    char prepare_report[LR_PATH_MAX];
    char prepare_bundle[LR_PATH_MAX]; /* the session snapshot, for another try while Steam still takes in a save */
    uint64_t prepare_retry_at, prepare_wait_since;
    char auto_logdir[LR_PATH_MAX];
    int hosting_pending;         /* host: waiting for the lobby before starting the game */
    uint64_t hosting_since;
    char last_status[512];
    size_t host_log_start;
    char runtime_line[200];
    uint64_t last_friends, last_runtime, last_ui;
    int hotkey_ok;
    int in_tick;                 /* a message box inside the tick would re-enter Steam's callback loop */
    char deferred[512];
    UINT deferred_icon;
    /* hero list */
    uint32_t heroes_shown;       /* table revision on screen; 0xFFFFFFFF = redraw */
    int heroes_role, heroes_filling, party_count;
    int live_party_count;
    uint32_t live_party_ids[4];
    uint8_t party_flags[LR_MAX_HEROES];
    char heroes_hint_text[300];
    uint64_t heroes_hint_until, heroes_recheck_at;
    uint32_t roster_crc;         /* host: last persist.roster.json read from disk */
    uint64_t last_roster;
} G;

/* ---- helpers ---- */

static uint64_t now_ms(void) { return GetTickCount64(); }

static int g_dpi = 96;
static int D(int px) { return MulDiv(px, g_dpi, 96); } /* layout is in 96-dpi pixels */

static wchar_t *W(const char *s) {
    static wchar_t buf[4][2048];
    static int next;
    wchar_t *w = buf[next++ & 3];
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w, 2048);
    return w;
}

static void to_utf8(const wchar_t *w, char *out, size_t cap) {
    WideCharToMultiByte(CP_UTF8, 0, w, -1, out, (int)cap, NULL, NULL);
}

static void logline(const char *fmt, ...) {
    if (!G.log) return;
    char line[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    SYSTEMTIME t;
    GetLocalTime(&t);
    fprintf(G.log, "%02d:%02d:%02d.%03d %s\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, line);
    fflush(G.log);
}

/* Watchdog. The window thread marks every step with PHASE(); if no step is marked for 4 s the watchdog
 * thread suspends it for a moment, copies its registers and stack, and logs the phase plus the code
 * addresses (module + offset) found there, so a hang on someone else's PC shows exactly where it is. */
static volatile LONG64 g_beat;
static const char *volatile g_phase = "start";
static HANDLE g_gui_thread;
#define PHASE(name)                                                                                                    \
    do {                                                                                                               \
        g_phase = (name);                                                                                              \
        g_beat = (LONG64)GetTickCount64();                                                                             \
    } while (0)

static void log_address(const char *what, DWORD64 addr) {
    HMODULE mod = NULL;
    wchar_t wname[MAX_PATH] = L"?";
    char name[MAX_PATH];
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)(uintptr_t)addr, &mod)) {
        GetModuleFileNameW(mod, wname, MAX_PATH);
        wchar_t *base = wcsrchr(wname, L'\\');
        to_utf8(base ? base + 1 : wname, name, sizeof name);
        logline("    %s %s+0x%llx", what, name, (unsigned long long)(addr - (DWORD64)(uintptr_t)mod));
    }
}

static DWORD WINAPI watchdog(void *arg) {
    (void)arg;
    int reported = 0;
    for (;;) {
        Sleep(1000);
        uint64_t age = GetTickCount64() - (uint64_t)g_beat;
        if (age < 2000) {
            if (reported) logline("watchdog: window responds again (phase %s)", g_phase);
            reported = 0;
            continue;
        }
        if (age < 4000 || reported) continue;
        reported = 1;
        CONTEXT ctx;
        memset(&ctx, 0, sizeof ctx);
        ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
        static DWORD64 stack[2048];
        size_t words = 0;
        if (SuspendThread(g_gui_thread) != (DWORD)-1) {
            /* only copy while it is suspended: logging could need a lock the window thread holds */
            if (GetThreadContext(g_gui_thread, &ctx)) {
                MEMORY_BASIC_INFORMATION mbi;
                if (VirtualQuery((void *)(uintptr_t)ctx.Rsp, &mbi, sizeof mbi)) {
                    uintptr_t end = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
                    words = (end - (uintptr_t)ctx.Rsp) / 8;
                    if (words > 2048) words = 2048;
                    memcpy(stack, (void *)(uintptr_t)ctx.Rsp, words * 8);
                }
            }
            ResumeThread(g_gui_thread);
        }
        logline("watchdog: WINDOW FROZEN for %llu s in phase '%s'", (unsigned long long)(age / 1000), g_phase);
        log_address("rip", ctx.Rip);
        int shown = 0;
        for (size_t i = 0; i < words && shown < 40; i++) {
            HMODULE mod;
            if (stack[i] > 0x10000 &&
                GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   (LPCWSTR)(uintptr_t)stack[i], &mod)) {
                char what[32];
                snprintf(what, sizeof what, "stack+%03zx", i * 8);
                log_address(what, stack[i]);
                shown++;
            }
        }
    }
    return 0;
}

static void log_cb(void *ctx, const char *line) {
    (void)ctx;
    logline("%s", line);
}

static void message(const char *text, UINT icon) {
    if (G.in_tick) { /* shown when the tick is done; ticks keep running while it is open */
        snprintf(G.deferred, sizeof G.deferred, "%s", text);
        G.deferred_icon = icon;
        return;
    }
    MessageBoxW(G.wnd, W(text), L"DDCoop", MB_OK | icon);
}

static int sha256_file(const char *path, char hex[65]) { return lr_sha256_file(path, hex); }

static void random_cb(void *ctx, uint8_t *buf, size_t len) {
    (void)ctx;
    BCryptGenRandom(NULL, buf, (ULONG)len, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
}

static int darkest_running(void) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe = {.dwSize = sizeof pe};
    int found = 0;
    for (BOOL more = Process32FirstW(snap, &pe); more; more = Process32NextW(snap, &pe))
        if (_wcsicmp(pe.szExeFile, L"Darkest.exe") == 0) found = 1;
    CloseHandle(snap);
    return found;
}

static int game_running_cb(void *ctx) {
    (void)ctx;
    return darkest_running();
}

/* Steam installs a DLC the account owns as <game>\dlc\<app id>_<name>; parts of a DLC that a campaign lists
 * on their own ("flagellant", "districts") are <game>\dlc\<app id>_<name>\features\<part>. Only reads folder
 * names. */
static int has_dlc_cb(void *ctx, const char *name) {
    (void)ctx;
    char pattern[LR_PATH_MAX], dlc[LR_PATH_MAX];
    lr_join(dlc, sizeof dlc, G.root, "dlc");
    lr_join(pattern, sizeof pattern, dlc, "*");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(W(pattern), &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    int found = 0;
    size_t n = strlen(name);
    do {
        char folder[LR_PATH_MAX], path[LR_PATH_MAX], features[LR_PATH_MAX];
        to_utf8(fd.cFileName, folder, sizeof folder);
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || folder[0] == '.') continue;
        size_t len = strlen(folder);
        if (len > n && folder[len - n - 1] == '_' && strcmp(folder + len - n, name) == 0) found = 1;
        lr_join(path, sizeof path, dlc, folder);
        lr_join(features, sizeof features, path, "features");
        lr_join(path, sizeof path, features, name);
        if (lr_dir_exists(path)) found = 1;
    } while (!found && FindNextFileW(h, &fd));
    FindClose(h);
    return found;
}

static void set_check(int i, int ok, const char *fmt, ...) {
    G.ok[i] = ok;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(G.check_text[i], sizeof G.check_text[i], fmt, ap);
    va_end(ap);
}

/* ---- install checks ---- */

static void parent_dir(char *path) {
    char *s = strrchr(path, '\\');
    if (s) *s = 0;
}

static void find_paths(void) {
    wchar_t exe[LR_PATH_MAX];
    GetModuleFileNameW(NULL, exe, LR_PATH_MAX);
    char p[LR_PATH_MAX];
    to_utf8(exe, p, sizeof p);
    parent_dir(p); /* ...\mods\DDCoop\launcher */
    parent_dir(p); /* ...\mods\DDCoop */
    snprintf(G.moddir, sizeof G.moddir, "%s", p);
    parent_dir(p); /* ...\mods */
    parent_dir(p); /* game root */
    snprintf(G.root, sizeof G.root, "%s", p);
    lr_join(G.bin, sizeof G.bin, G.root, "_windows\\win64");
}

static void check_install(void) {
    char exe[LR_PATH_MAX], hex[65];
    lr_join(exe, sizeof exe, G.bin, "Darkest.exe");
    if (!lr_file_exists(exe)) {
        set_check(CHK_GAME, 0, "Spel niet gevonden. DDCoop hoort in <Darkest Dungeon>\\mods\\DDCoop\\launcher. Draai "
                               "Install DDCoop.bat opnieuw.");
        set_check(CHK_VERSION, 0, "Versie: niet te controleren");
        set_check(CHK_MOD, 0, "Modbestanden: niet te controleren");
        return;
    }
    set_check(CHK_GAME, 1, "Spel gevonden: %s", G.root);
    if (sha256_file(exe, hex) && strcmp(hex, LR_HASH_GAME) == 0)
        set_check(CHK_VERSION, 1, "Spelversie klopt");
    else
        set_check(CHK_VERSION, 0, "Andere Darkest Dungeon-versie dan deze DDCoop kent. Is er een Steam-update geweest? "
                                  "Dan is er een nieuwe DDCoop nodig.");
    char boot[LR_PATH_MAX], orig[LR_PATH_MAX], runtime[LR_PATH_MAX], h2[65];
    lr_join(boot, sizeof boot, G.bin, "SDL2.dll");
    lr_join(orig, sizeof orig, G.bin, "SDL2_orig.dll");
    lr_join(runtime, sizeof runtime, G.moddir, "ddcoop-runtime.dll");
    int repaired = lr_repair_bootstrap(G.root, G.moddir);
    if (repaired) logline("automatic bootstrap repair: %s", repaired > 0 ? "verified" : "could not complete");
    int boot_ok = sha256_file(boot, hex) && strcmp(hex, LR_HASH_BOOTSTRAP) == 0;
    int rt_ok = sha256_file(runtime, h2) && strcmp(h2, LR_HASH_RUNTIME) == 0;
    int orig_ok = sha256_file(orig, h2) && strcmp(h2, LR_HASH_SDL2_ORIGINAL) == 0;
    if (boot_ok && rt_ok && orig_ok)
        set_check(CHK_MOD, 1, "Modbestanden compleet");
    else
        set_check(CHK_MOD, 0, "Modbestanden %s. Draai Repair DDCoop.bat (bijvoorbeeld na een Steam-update of "
                              "'bestanden controleren').",
                  !boot_ok ? "missen (SDL2.dll is teruggezet)" : !rt_ok ? "beschadigd (runtime)" : "missen (SDL2_orig.dll)");
    G.install_ok = G.ok[CHK_GAME] && G.ok[CHK_VERSION] && G.ok[CHK_MOD];
}

static int steam_userdata_remote(char *out, size_t cap) {
    wchar_t path[LR_PATH_MAX];
    DWORD size = sizeof path;
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath", RRF_RT_REG_SZ, NULL, path, &size) != 0)
        return 0;
    char steam[LR_PATH_MAX];
    to_utf8(path, steam, sizeof steam);
    for (char *c = steam; *c; c++)
        if (*c == '/') *c = '\\';
    char user[16];
    snprintf(user, sizeof user, "%lu", (unsigned long)(lr_steam_me() & 0xFFFFFFFFu));
    static const char middle[] = "\\userdata\\";
    static const char tail[] = "\\262060\\remote";
    size_t steam_len = strlen(steam), user_len = strlen(user);
    if (steam_len + sizeof middle - 1 + user_len + sizeof tail > cap) return 0;
    memcpy(out, steam, steam_len);
    memcpy(out + steam_len, middle, sizeof middle - 1);
    memcpy(out + steam_len + sizeof middle - 1, user, user_len);
    memcpy(out + steam_len + sizeof middle - 1 + user_len, tail, sizeof tail);
    return 1;
}

static void log_save_folders(const char *remote) {
    char pattern[LR_PATH_MAX];
    lr_join(pattern, sizeof pattern, remote, "*");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(W(pattern), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        logline("save folder root does not exist (no Steam saves for Darkest Dungeon on this account?)");
        return;
    }
    do {
        char name[LR_PATH_MAX];
        to_utf8(fd.cFileName, name, sizeof name);
        if (name[0] != '.') logline("  in remote: %s%s", name, (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? "\\" : "");
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

/* ---- which campaign to host ---- */

static void check_slot(void);
static int session_active(void);
static int slot_cannot_host(int slot, char *why, size_t cap);
static void stop_session(const char *why);
static int spawn_game_process(const char *exe, const char *args, HANDLE *process);

static void slot_path(char *path, size_t cap) { lr_join(path, cap, G.moddir, "ddcoop-slot.txt"); }
static int slot_remembered(void) {
    char path[LR_PATH_MAX];
    uint8_t *data = NULL;
    size_t len = 0;
    int slot = 0;
    slot_path(path, sizeof path);
    if (lr_read_file(path, 16, &data, &len)) {
        if (len >= 1 && data[0] >= '1' && data[0] <= '9') slot = data[0] - '0';
        free(data);
    }
    return slot;
}
static void slot_remember(int slot) {
    char path[LR_PATH_MAX];
    uint8_t digit = (uint8_t)('0' + slot);
    slot_path(path, sizeof path);
    lr_write_file(path, &digit, 1);
}
/* Host saving: the player's choice, remembered next to the slot. A missing file means the kit's default. */
static void saving_path(char *path, size_t cap) { lr_join(path, cap, G.moddir, "ddcoop-save.txt"); }
static int saving_remembered(void) {
    char path[LR_PATH_MAX];
    uint8_t *data = NULL;
    size_t len = 0;
    int on = !DDC_TEST_NOSAVE;
    saving_path(path, sizeof path);
    if (lr_read_file(path, 16, &data, &len)) {
        if (len >= 1 && (data[0] == '0' || data[0] == '1')) on = data[0] == '1';
        free(data);
    }
    return on;
}
static void saving_remember(int on) {
    char path[LR_PATH_MAX];
    uint8_t digit = (uint8_t)(on ? '1' : '0');
    saving_path(path, sizeof path);
    lr_write_file(path, &digit, 1);
}
/* The dropdown: what is in the save folder now, with the name the player gave it. */
static void slot_fill(void) {
    lr_campaign_entry list[9];
    if (!G.slot) return;
    lr_list_campaigns(G.core.cfg.remote_dir, list);
    SendMessageW(G.slot, CB_RESETCONTENT, 0, 0);
    for (int i = 0; i < 9; i++) {
        wchar_t name[64] = L"", line[128];
        MultiByteToWideChar(CP_UTF8, 0, list[i].estate, -1, name, 64);
        if (list[i].empty) swprintf(line, 128, L"Slot %d: leeg", list[i].slot);
        else if (list[i].unreadable) swprintf(line, 128, L"Slot %d: niet leesbaar", list[i].slot);
        else swprintf(line, 128, L"Slot %d: %ls%ls", list[i].slot, name[0] ? name : L"(naamloos)", list[i].tutorial ? L"  (tutorial nog niet uitgespeeld)" : L"");
        LRESULT at = SendMessageW(G.slot, CB_ADDSTRING, 0, (LPARAM)line);
        SendMessageW(G.slot, CB_SETITEMDATA, (WPARAM)at, list[i].slot);
    }
    /* Nothing chosen yet: slot 3 was the old fixed place, otherwise the first campaign there is. A choice stays, also of an empty slot. */
    if (!G.chosen_slot) {
        G.chosen_slot = 3;
        if (list[2].empty || list[2].unreadable)
            for (int i = 8; i >= 0; i--)
                if (!list[i].empty && !list[i].unreadable) G.chosen_slot = list[i].slot;
    }
    SendMessageW(G.slot, CB_SETCURSEL, (WPARAM)(G.chosen_slot - 1), 0);
    if (!session_active()) G.core.cfg.coop_slot = G.chosen_slot;
}
/* An empty or unreadable slot has nothing to send to a friend. */
static int slot_cannot_host(int slot, char *why, size_t cap) {
    lr_campaign_entry list[9];
    lr_list_campaigns(G.core.cfg.remote_dir, list);
    if (slot < 1 || slot > 9 || (!list[slot - 1].empty && !list[slot - 1].unreadable)) return 0;
    if (why) snprintf(why, cap, "De save in slot %d is niet leesbaar. Kies een ander slot, of open dit slot eerst gewoon via Steam.", slot);
    return 1;
}
/* A campaign is made by the game itself, alone, and saved as always: its tutorial comes before there is anything to share. Only this process gets to know the slot. */
static void new_campaign_start(int slot) {
    char exe[LR_PATH_MAX];
    wchar_t value[2] = {(wchar_t)(L'0' + slot), 0};
    int ok;
    stop_session("nieuwe campagne");
    G.core.cfg.coop_slot = slot;
    lr_write_ini(G.core.cfg.ini_path, "off", LR_DEFAULT_GAME_PORT, "unused-until-ddcoop-starts", slot);
    lr_join(exe, sizeof exe, G.bin, "Darkest.exe");
    SetEnvironmentVariableW(L"DDCOOP_NEW_SLOT", value);
    ok = spawn_game_process(exe, "", &G.game);
    SetEnvironmentVariableW(L"DDCOOP_NEW_SLOT", NULL);
    if (!ok) return;
    G.new_campaign_slot = slot;
    logline("game started alone for a campaign in slot %d (pid %lu)", slot, GetProcessId(G.game));
    snprintf(G.core.status, sizeof G.core.status, "Maak je campagne in slot %d en speel de tutorial uit tot in het dorp. Sluit dan het spel en klik Host.", slot);
}
/* Host on a slot without a playable campaign yet. 1 = dealt with here. */
static int new_campaign_offer(int slot) {
    lr_campaign_entry list[9];
    wchar_t text[400];
    lr_list_campaigns(G.core.cfg.remote_dir, list);
    if (slot < 1 || slot > 9 || (!list[slot - 1].empty && !list[slot - 1].tutorial)) return 0;
    if (list[slot - 1].empty)
        swprintf(text, 400, L"Slot %d is nog leeg. Nieuwe campagne maken?\n\nHet spel start zonder je vriend. Je kiest een naam en speelt de tutorial uit tot in het dorp; dat wordt gewoon opgeslagen in slot %d. Sluit daarna het spel en klik Host.", slot, slot);
    else
        swprintf(text, 400, L"De campagne in slot %d staat nog in de tutorial. Verder spelen?\n\nHet spel start zonder je vriend. Speel de tutorial uit tot in het dorp; dat wordt gewoon opgeslagen. Sluit daarna het spel en klik Host.", slot);
    if (MessageBoxW(G.wnd, text, L"DDCoop", MB_YESNO | MB_ICONQUESTION) == IDYES) new_campaign_start(slot);
    return 1;
}
static void slot_chosen(void) {
    LRESULT at = SendMessageW(G.slot, CB_GETCURSEL, 0, 0);
    if (at == CB_ERR) return;
    G.chosen_slot = (int)SendMessageW(G.slot, CB_GETITEMDATA, (WPARAM)at, 0);
    G.core.cfg.coop_slot = G.chosen_slot;
    slot_remember(G.chosen_slot);
    logline("chosen campaign: slot %d", G.chosen_slot);
    check_slot();
}

static void check_slot(void) {
    char name[LR_NAME_MAX + 1];
    lr_game_info game;
    if (G.core.cfg.remote_dir[0] && lr_find_save_folder(G.core.cfg.remote_dir, G.core.cfg.coop_slot, name, sizeof name) && !slot_cannot_host(G.core.cfg.coop_slot, NULL, 0)) {
        if (lr_read_game_info(G.core.cfg.remote_dir, name, &game) && (game.text || game.tutorial))
            set_check(CHK_SLOT, 1, "Campagne in slot %d staat nog in de tutorial. Als jij host, speel je die eerst uit: klik Host.",
                      G.core.cfg.coop_slot);
        else
            set_check(CHK_SLOT, 1, "Co-op campagne in slot %d gevonden (%s)", G.core.cfg.coop_slot, name);
    } else
        set_check(CHK_SLOT, 1, "Slot %d is leeg. Als jij host, maak je daar eerst een nieuwe campagne: klik Host.", G.core.cfg.coop_slot);
}

/* ---- the game ---- */

static int restore_pending(void) { return G.core.cfg.restore_path[0] && lr_file_exists(G.core.cfg.restore_path); }

/* Both the preparation child and the game must use the real game's Steam identity.
 * The lobby launcher remains in its own AppID 480 process. */
static int spawn_game_process(const char *exe, const char *args, HANDLE *process) {
    /* Our environment says SteamAppId=480 (Spacewar); the game must not inherit that. */
    wchar_t *env = GetEnvironmentStringsW(), *out = calloc(65536, sizeof(wchar_t));
    if (!env || !out) {
        if (env) FreeEnvironmentStringsW(env);
        free(out);
        return 0;
    }
    size_t n = 0;
    for (wchar_t *p = env; *p; p += wcslen(p) + 1) {
        if (_wcsnicmp(p, L"SteamAppId=", 11) == 0 || _wcsnicmp(p, L"SteamGameId=", 12) == 0 ||
            _wcsnicmp(p, L"SteamOverlayGameId=", 19) == 0)
            continue;
        size_t len = wcslen(p) + 1;
        if (n + len + 1 >= 65536) break;
        memcpy(out + n, p, len * sizeof(wchar_t));
        n += len;
    }
    const wchar_t ids[] = L"SteamAppId=262060\0SteamGameId=262060\0";
    if (n + sizeof ids / sizeof ids[0] >= 65536) {
        free(out);
        FreeEnvironmentStringsW(env);
        return 0;
    }
    memcpy(out + n, ids, sizeof ids);
    FreeEnvironmentStringsW(env);
    char cmd[LR_PATH_MAX * 5];
    if (snprintf(cmd, sizeof cmd, "\"%s\" %s", exe, args) >= (int)sizeof cmd) {
        free(out);
        return 0;
    }
    STARTUPINFOW si = {.cb = sizeof si};
    PROCESS_INFORMATION pi;
    wchar_t wcmd[LR_PATH_MAX * 5], wexe[LR_PATH_MAX], wbin[LR_PATH_MAX];
    if (!MultiByteToWideChar(CP_UTF8, 0, cmd, -1, wcmd, LR_PATH_MAX * 5)) {
        free(out);
        return 0;
    }
    wcscpy(wexe, W(exe));
    wcscpy(wbin, W(G.bin));
    BOOL ok = CreateProcessW(wexe, wcmd, NULL, NULL, FALSE, CREATE_UNICODE_ENVIRONMENT, out, wbin, &si, &pi);
    free(out);
    if (!ok) {
        logline("CreateProcess failed: %lu", GetLastError());
        message("Kon Darkest Dungeon niet starten. Zie DDCoop-launcher.log.", MB_ICONERROR);
        return 0;
    }
    CloseHandle(pi.hThread);
    *process = pi.hProcess;
    return 1;
}

/* restore: 1 = a guest's own campaign back after a session; 2 = the player asked to put a slot back from a backup. */
static int begin_prepare(const char *bundle, int restore, int nosave) {
    if (G.prepare || darkest_running()) return 0;
    char exe[LR_PATH_MAX], backup[LR_PATH_MAX], name[96], args[LR_PATH_MAX * 4];
    wchar_t wexe[LR_PATH_MAX];
    GetModuleFileNameW(NULL, wexe, LR_PATH_MAX);
    to_utf8(wexe, exe, sizeof exe);
    if (!lr_make_dirs(G.core.cfg.backup_root)) return 0;
    SYSTEMTIME t;
    GetLocalTime(&t);
    snprintf(name, sizeof name, "Steam-%s-%04u%02u%02u-%02u%02u%02u-%llu.bin",
             restore == 2 ? "undo" : restore ? "restore" : "prepare", t.wYear,
             t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, (unsigned long long)now_ms());
    lr_join(backup, sizeof backup, G.core.cfg.backup_root, name);
    lr_join(G.prepare_report, sizeof G.prepare_report, G.moddir, restore ? "ddcoop-restore.log" : "ddcoop-prepare.log");
    if (snprintf(args, sizeof args, "--prepare-save \"%s\" \"%s\" \"%s\" \"%s\"", G.bin, bundle, backup,
                 G.prepare_report) >= (int)sizeof args)
        return 0;
    /* A failed spawn must not leave a previous successful report behind. */
    lr_delete_file(G.prepare_report);
    if (!spawn_game_process(exe, args, &G.prepare)) return 0;
    G.prepare_restore = restore == 1;
    G.prepare_undo = restore == 2;
    G.prepare_nosave = nosave;
    G.prepare_since = now_ms();
    snprintf(G.core.status, sizeof G.core.status,
             restore == 2 ? "De back-up wordt teruggezet via Steam. Even wachten..."
             : restore    ? "Je eigen campagne wordt automatisch teruggezet. Even wachten..."
                          : "Campagne voorbereiden en controleren via Steam. Even wachten...");
    logline("Steam %s helper started; game stays closed", restore == 2 ? "undo" : restore ? "restore" : "prepare");
    return 1;
}

static int launch_game(int nosave) {
    check_install();
    if (!G.install_ok) return 0;
    G.auto_logdir[0]=0;
    if (G.prepare || G.game || darkest_running()) {
        message("Darkest Dungeon draait al, of DDCoop is nog bezig met voorbereiden/herstellen. Even wachten.",
                MB_ICONWARNING);
        return 0;
    }
    lr_snapshot *s = calloc(1, sizeof *s);
    if (!s) return 0;
    s->user = lr_steam_me();
    snprintf(s->folder, sizeof s->folder, "%s", G.core.save_folder);
    int ok = G.core.file_count > 0;
    for (uint32_t i = 0; ok && i < G.core.file_count; i++) {
        lr_file *f = &G.core.files[i];
        char backup[LR_SNAPSHOT_NAME_MAX + 1];
        snprintf(backup, sizeof backup, "backup/%s", f->name);
        ok = lr_snapshot_add(s, f->name, f->data, f->size) && lr_snapshot_add(s, backup, f->data, f->size);
    }
    char path[LR_PATH_MAX];
    lr_join(path, sizeof path, G.moddir, "ddcoop-session.bin");
    ok = ok && lr_snapshot_write(path, s);
    lr_snapshot_free(s);
    free(s);
    snprintf(G.prepare_bundle, sizeof G.prepare_bundle, "%s", path);
    G.prepare_retry_at = G.prepare_wait_since = 0;
    return ok && begin_prepare(path, 0, nosave);
}

/* ---- session control ---- */

static int session_active(void) { return G.core.state != LR_ST_IDLE && G.core.state != LR_ST_FAILED; }

static void stop_session(const char *why) {
    if (session_active() || lr_steam_lobby()) logline("stop: %s", why);
    if (G.core.peer) lr_steam_close_session(G.core.peer);
    lr_core_stop(&G.core);
    lr_steam_leave_lobby();
    G.hosting_pending = 0;
    G.prepare_retry_at = G.prepare_wait_since = 0;
    snprintf(G.core.status, sizeof G.core.status, "%s", why);
}

/* The newest backup DDCoop made of the chosen slot before writing a session's campaign into it (Steam-prepare-...). */
static int newest_backup(int slot, char *out, size_t cap, char *when, size_t when_cap) {
    char pattern[LR_PATH_MAX];
    WIN32_FIND_DATAW fd;
    FILETIME best = {0, 0};
    int found = 0;
    lr_join(pattern, sizeof pattern, G.core.cfg.backup_root, "Steam-prepare-*.bin");
    HANDLE h = FindFirstFileW(W(pattern), &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    lr_snapshot *snap = calloc(1, sizeof *snap);
    uint64_t me = lr_steam_me();
    do {
        char name[LR_PATH_MAX], path[LR_PATH_MAX];
        int at = 0;
        if (!snap || (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || CompareFileTime(&fd.ftLastWriteTime, &best) <= 0)
            continue;
        to_utf8(fd.cFileName, name, sizeof name);
        lr_join(path, sizeof path, G.core.cfg.backup_root, name);
        memset(snap, 0, sizeof *snap);
        if (lr_snapshot_read(path, snap) && snap->user == me && lr_slot_folder_ok(snap->folder, &at) && at == slot) {
            best = fd.ftLastWriteTime;
            snprintf(out, cap, "%s", path);
            found = 1;
        }
        lr_snapshot_free(snap);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    free(snap);
    if (found) {
        SYSTEMTIME u, t;
        FileTimeToSystemTime(&best, &u);
        SystemTimeToTzSpecificLocalTime(NULL, &u, &t);
        snprintf(when, when_cap, "%u-%u-%04u om %02u:%02u", t.wDay, t.wMonth, t.wYear, t.wHour, t.wMinute);
    }
    return found;
}

static void do_undo(void) {
    char path[LR_PATH_MAX], when[64];
    wchar_t text[600];
    if (G.prepare || restore_pending() || session_active() || G.game) {
        message("Dit kan alleen als er geen sessie loopt en het spel dicht is.", MB_ICONINFORMATION);
        return;
    }
    if (!G.install_ok || !G.steam_ok) {
        message("Eerst moeten alle punten bovenaan groen zijn (OK).", MB_ICONWARNING);
        return;
    }
    if (darkest_running()) {
        message("Sluit Darkest Dungeon eerst.", MB_ICONWARNING);
        return;
    }
    if (!newest_backup(G.chosen_slot, path, sizeof path, when, sizeof when)) {
        message("DDCoop heeft nog geen back-up van dit slot.", MB_ICONINFORMATION);
        return;
    }
    swprintf(text, 600, L"Slot %d terugzetten zoals het was vlak voor de laatste DDCoop-sessie in dat slot (%hs)?\n\n"
             L"Alles wat daarna in dit slot is opgeslagen, verdwijnt uit het spel. DDCoop bewaart het wel eerst als "
             L"nieuwe back-up in je map DDCoop-backups.", G.chosen_slot, when);
    if (MessageBoxW(G.wnd, text, L"DDCoop", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return;
    logline("putting slot %d back from %s", G.chosen_slot, path);
    if (!begin_prepare(path, 2, 1)) message("Terugzetten kon niet starten. Er is niets veranderd.", MB_ICONWARNING);
}

static void do_host(void) {
    if (G.prepare || restore_pending()) {
        message("DDCoop moet eerst je eigen campagne terugzetten. Sluit een eventueel open gastspel en wacht even.",
                MB_ICONINFORMATION);
        return;
    }
    if (!G.install_ok || !G.steam_ok) {
        message("Eerst moeten alle punten bovenaan groen zijn (OK).", MB_ICONWARNING);
        return;
    }
    if (darkest_running()) {
        message("Sluit Darkest Dungeon eerst. DDCoop start het spel zelf, met co-op aan.", MB_ICONWARNING);
        return;
    }
    if (new_campaign_offer(G.chosen_slot)) return;
    {
        char why[300];
        if (slot_cannot_host(G.chosen_slot, why, sizeof why)) {
            message(why, MB_ICONINFORMATION);
            return;
        }
    }
    stop_session("nieuwe sessie");
    G.core.cfg.coop_slot = G.chosen_slot;
    {
        /* Where this game's log lines will start. */
        char path[LR_PATH_MAX];
        uint8_t *data;
        size_t len = 0;
        lr_join(path, sizeof path, G.moddir, "ddcoop-fw-host.log");
        G.host_log_start = lr_read_file(path, 32u * 1024 * 1024, &data, &len) ? len : 0;
        if (G.host_log_start) free(data);
    }
    if (!lr_core_host_start(&G.core, now_ms())) {
        message(G.core.error, MB_ICONWARNING);
        return;
    }
    if (!lr_steam_host_lobby()) {
        stop_session("Steam kon geen lobby maken");
        message("Steam kon geen lobby maken. Probeer het zo opnieuw.", MB_ICONWARNING);
        return;
    }
    G.hosting_pending = 1;
    G.hosting_since = now_ms();
    snprintf(G.core.status, sizeof G.core.status, "lobby wordt gemaakt...");
}

static void join_lobby(uint64_t lobby) {
    if (G.prepare || restore_pending()) {
        message("DDCoop moet eerst je eigen campagne terugzetten. Sluit een eventueel open gastspel en wacht even.",
                MB_ICONINFORMATION);
        return;
    }
    if (!G.install_ok || !G.steam_ok) {
        message("Eerst moeten alle punten bovenaan groen zijn (OK).", MB_ICONWARNING);
        return;
    }
    if (darkest_running()) {
        message("Sluit Darkest Dungeon eerst. DDCoop zet de campagne van de host klaar en start daarna het spel zelf.",
                MB_ICONWARNING);
        return;
    }
    stop_session("nieuwe sessie");
    if (!lr_steam_join_lobby(lobby)) {
        message("Kon de lobby niet openen.", MB_ICONWARNING);
        return;
    }
    snprintf(G.core.status, sizeof G.core.status, "lobby van de host binnengaan...");
    logline("joining lobby %llu", (unsigned long long)lobby);
}

static lr_friend *selected_friend(void) {
    LRESULT i = SendMessageW(G.friends, LB_GETCURSEL, 0, 0);
    if (i == LB_ERR) return NULL;
    LRESULT idx = SendMessageW(G.friends, LB_GETITEMDATA, (WPARAM)i, 0);
    return (idx >= 0 && idx < G.friend_count) ? &G.friends_list[idx] : NULL;
}

static void do_join_selected(void) {
    lr_friend *f = selected_friend();
    if (!f) {
        message("Kies eerst een vriend in de lijst.", MB_ICONINFORMATION);
        return;
    }
    if (!f->hosting) {
        message("Deze vriend host (nog) geen DDCoop-spel. Vraag je vriend op Host te klikken, of je uit te nodigen.",
                MB_ICONINFORMATION);
        return;
    }
    join_lobby(f->lobby);
}

static void do_invite_selected(void) {
    lr_friend *f = selected_friend();
    if (!f) {
        message("Kies eerst een vriend in de lijst.", MB_ICONINFORMATION);
        return;
    }
    if (G.core.role != LR_ROLE_HOST || !lr_steam_lobby()) {
        message("Klik eerst op 'Host een spel'; daarna kun je vrienden uitnodigen.", MB_ICONINFORMATION);
        return;
    }
    if (lr_steam_invite(f->id)) {
        char t[256];
        snprintf(t, sizeof t, "Uitnodiging verstuurd naar %.64s. Je vriend moet DDCoop open hebben en de uitnodiging in Steam "
                              "accepteren (of jou kiezen en op Join klikken).", f->name);
        message(t, MB_ICONINFORMATION);
    } else {
        message("Steam kon de uitnodiging niet versturen.", MB_ICONWARNING);
    }
}

static void do_join_code(void) {
    wchar_t w[64];
    GetWindowTextW(G.code, w, 64);
    char s[64];
    to_utf8(w, s, sizeof s);
    char *p = s;
    while (*p == ' ') p++;
    uint64_t lobby = _strtoui64(p, NULL, 10);
    if (!lobby) {
        message("Plak eerst de code van de host in het vak (alleen cijfers).", MB_ICONINFORMATION);
        return;
    }
    join_lobby(lobby);
}

static void do_copy_code(void) {
    uint64_t lobby = lr_steam_lobby();
    if (G.core.role != LR_ROLE_HOST || !lobby) {
        message("Je hebt pas een code als je host. Klik eerst op 'Host een spel'.", MB_ICONINFORMATION);
        return;
    }
    char code[32];
    snprintf(code, sizeof code, "%llu", (unsigned long long)lobby);
    if (OpenClipboard(G.wnd)) {
        EmptyClipboard();
        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, 64 * sizeof(wchar_t));
        wcscpy(GlobalLock(h), W(code));
        GlobalUnlock(h);
        SetClipboardData(CF_UNICODETEXT, h);
        CloseClipboard();
    }
    char t[160];
    snprintf(t, sizeof t, "Code %s staat op het klembord. Stuur de code naar je vriend; die plakt hem in het codevak.", code);
    message(t, MB_ICONINFORMATION);
}

static void collect_logs(int open_folder) {
    wchar_t desktop[MAX_PATH];
    if (SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desktop) != S_OK) return;
    char d[LR_PATH_MAX], dir[LR_PATH_MAX], stamp[64];
    to_utf8(desktop, d, sizeof d);
    time_t t = time(NULL);
    strftime(stamp, sizeof stamp, "DDCoop-logs-%Y-%m-%d_%H%M%S", localtime(&t));
    if(!open_folder && G.auto_logdir[0])snprintf(dir,sizeof dir,"%s",G.auto_logdir);
    else {
        lr_join(dir, sizeof dir, d, stamp);
        if(!open_folder)snprintf(G.auto_logdir,sizeof G.auto_logdir,"%s",dir);
    }
    lr_make_dirs(dir);
    const char *names[] = {"DDCoop-launcher.log",
                           "ddcoop-fw-host.log",
                           "ddcoop-fw-guest.log",
                           "ddcoop-heroes.ini",
                           "ddcoop-prepare.log",
                           "ddcoop-restore.log",
                           "app.log",
                           "app.log.bak",
                           "steam_appid.txt"};
    if (G.log) fflush(G.log);
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
        char from[LR_PATH_MAX], to[LR_PATH_MAX];
        uint8_t *data;
        size_t len;
        lr_join(from, sizeof from, i >= 6 ? G.bin : G.moddir, names[i]);
        lr_join(to, sizeof to, dir, names[i]);
        if (lr_read_file(from, 64u * 1024 * 1024, &data, &len)) {
            lr_write_file(to, data, len);
            free(data);
        }
    }
    char summary[4096];
    int n = snprintf(summary, sizeof summary, "DDCoop %s\r\nstate: %s\r\nstatus: %s\r\nerror: %s\r\n", DDC_VERSION,
                     lr_state_name(G.core.state), G.core.status, G.core.error);
    for (int i = 0; i < CHK_COUNT && n < (int)sizeof summary - 300; i++)
        n += snprintf(summary + n, sizeof summary - (size_t)n, "[%s] %s\r\n", G.ok[i] ? "OK" : "--", G.check_text[i]);
    char path[LR_PATH_MAX];
    lr_join(path, sizeof path, dir, "checks.txt");
    lr_write_file(path, (const uint8_t *)summary, strlen(summary));
    if (open_folder) ShellExecuteW(NULL, L"explore", W(dir), NULL, NULL, SW_SHOWNORMAL);
}
static void do_logs(void) { collect_logs(1); }

static BOOL CALLBACK close_game_window(HWND wnd, LPARAM pid) {
    DWORD owner = 0;
    GetWindowThreadProcessId(wnd, &owner);
    if (owner == (DWORD)pid && IsWindowVisible(wnd)) PostMessageW(wnd, WM_CLOSE, 0, 0);
    return TRUE;
}
static void request_game_close(void) {
    if (G.game) EnumWindows(close_game_window, (LPARAM)GetProcessId(G.game));
}

static void preparation_tick(void) {
    if (G.prepare && WaitForSingleObject(G.prepare, 0) == WAIT_OBJECT_0) {
        DWORD code = 1;
        GetExitCodeProcess(G.prepare, &code);
        CloseHandle(G.prepare);
        G.prepare = NULL;
        uint8_t *data = NULL;
        size_t len = 0;
        if (lr_read_file(G.prepare_report, 65536, &data, &len)) {
            char *text = malloc(len + 1);
            if (text) {
                memcpy(text, data, len);
                text[len] = 0;
                logline("preparation report:\n%s", text);
                free(text);
            }
            free(data);
        }
        if (G.prepare_undo) {
            G.prepare_undo = 0;
            logline("backup put back: %s (exit %lu)", code == 0 ? "verified" : "FAILED", code);
            snprintf(G.core.status, sizeof G.core.status, code == 0
                     ? "De back-up staat terug in slot %d. Wat er tot net in stond, is ook bewaard."
                     : "Terugzetten is niet gelukt; slot %d is onveranderd of teruggedraaid. De back-up is bewaard.",
                     G.chosen_slot);
            if (code != 0) collect_logs(0);
        } else if (G.prepare_restore) {
            if (code == 0 && lr_delete_file(G.core.cfg.restore_path)) {
                G.restore_failed = 0;
                logline("guest original campaign restored through Steam and verified; recovery journal cleared");
                snprintf(G.core.status, sizeof G.core.status,
                         "Je eigen campagne is teruggezet. Klaar voor een nieuwe sessie.");
                collect_logs(0);
            } else {
                G.restore_failed = 1;
                G.want_close = 0;
                logline("guest restore failed (exit %lu); recovery journal retained", code);
                snprintf(G.core.status, sizeof G.core.status,
                         "Herstel nog niet voltooid. Je back-up is bewaard; start DDCoop opnieuw om te herstellen.");
                message("Je eigen campagne kon nog niet volledig worden teruggezet. De back-up is bewaard. Sluit het "
                        "spel en start DDCoop opnieuw. Stuur de automatisch verzamelde logs als dit terugkomt.",
                        MB_ICONWARNING);
                collect_logs(0);
            }
        } else if (code == LR_PREPARE_UNREAD && session_active() && !G.want_close &&
                   (!G.prepare_wait_since || now_ms() - G.prepare_wait_since < LR_STEAM_WAIT_MS)) {
            /* Steam is still taking in the save the game just wrote, most often right after the game closed. Nothing was
             * written; try again in a moment, and say so. */
            if (!G.prepare_wait_since) G.prepare_wait_since = now_ms();
            G.prepare_retry_at = now_ms() + 3000;
            logline("Steam does not serve slot %d yet; trying again in 3 seconds", G.core.cfg.coop_slot);
            snprintf(G.core.status, sizeof G.core.status,
                     "Steam is nog bezig met het opslaan van slot %d (dat kan na het sluiten van het spel even duren). "
                     "DDCoop wacht en probeert het vanzelf opnieuw; het spel start zodra Steam klaar is.", G.core.cfg.coop_slot);
        } else if (code != 0) {
            logline("preparation refused launch (exit %lu)", code);
            stop_session("Campagnevoorbereiding mislukt; het spel is niet gestart.");
            message("De campagne kon niet betrouwbaar worden voorbereid. DDCoop start het spel niet. Een eventuele "
                    "gast-back-up wordt automatisch teruggezet. De diagnose wordt op je bureaublad verzameld.",
                    MB_ICONWARNING);
            collect_logs(0);
        } else if (session_active() && !G.want_close) {
            char exe[LR_PATH_MAX];
            lr_join(exe, sizeof exe, G.bin, "Darkest.exe");
            if (spawn_game_process(exe, G.prepare_nosave ? "-nosave" : "", &G.game)) {
                UnregisterHotKey(G.wnd,ID_HOTKEY); /* F9 is now handled inside SDL, without leaving the game. */
                G.runtime_line[0] = 0;
                logline("game started%s after Steam save verification (pid %lu)",
                        G.prepare_nosave ? " with -nosave" : " (host saving enabled)", GetProcessId(G.game));
                snprintf(G.core.status, sizeof G.core.status, "Campagne gecontroleerd; het spel start en opent vanzelf slot %d.", G.core.cfg.coop_slot);
            } else
                stop_session("Het spel kon niet starten; gast-herstel wordt uitgevoerd.");
        }
    }
    if (G.prepare_retry_at && !G.prepare && now_ms() >= G.prepare_retry_at) {
        G.prepare_retry_at = 0;
        if (session_active() && !G.want_close && !G.game) {
            if (darkest_running())
                stop_session("Darkest Dungeon draait al; sluit het en klik opnieuw op Host of Join.");
            else if (!begin_prepare(G.prepare_bundle, 0, G.prepare_nosave))
                stop_session("Campagnevoorbereiding kon niet opnieuw starten; het spel is niet gestart.");
            else
                snprintf(G.core.status, sizeof G.core.status,
                         "Steam is nog bezig met het opslaan van slot %d. DDCoop probeert het opnieuw (%llu s)...",
                         G.core.cfg.coop_slot, (unsigned long long)((now_ms() - G.prepare_wait_since) / 1000));
        }
    }
    if (G.prepare && G.prepare_since && now_ms() - G.prepare_since > 30000) {
        G.prepare_since = 0;
        logline("Steam storage helper still running after 30 seconds; not terminating a possible save write");
        snprintf(G.core.status, sizeof G.core.status,
                 "Steam is nog bezig met de campagne. Even wachten; het spel blijft gesloten.");
    }
    if (!G.prepare && !G.game && !session_active() && restore_pending() && !G.restore_failed && G.steam_ok) {
        if (!darkest_running()) {
            if(G.core.state==LR_ST_FAILED)stop_session("Gastcampagne herstellen na afgebroken sessie...");
            if (!begin_prepare(G.core.cfg.restore_path, 1, 1)) {
                G.restore_failed = 1;
                message("DDCoop kon het herstel niet starten. De oorspronkelijke campagne is bewaard. Sluit het spel "
                        "en start DDCoop opnieuw.",
                        MB_ICONWARNING);
            }
        } else
            snprintf(G.core.status, sizeof G.core.status,
                     "Sluit eerst Darkest Dungeon; daarna zet DDCoop je eigen campagne terug.");
    }
    if (G.want_close && !G.prepare && !restore_pending() && !G.game) DestroyWindow(G.wnd);
    static uint64_t close_at;
    if (G.want_close && G.game && now_ms() - close_at > 1000) {
        close_at = now_ms();
        request_game_close();
    }
}

/* ---- Steam callbacks ---- */

static int accept_session_cb(void *ctx, uint64_t peer) {
    (void)ctx;
    if (G.core.role == LR_ROLE_HOST) return lr_steam_is_member(peer);
    return peer != 0 && peer == lr_steam_lobby_owner();
}

static int allowed_cb(void *ctx, uint64_t peer) {
    (void)ctx;
    return lr_steam_is_member(peer);
}

static void on_message(void *ctx, uint64_t peer, int channel, const uint8_t *buf, size_t len) {
    (void)ctx;
    lr_core_on_message(&G.core, peer, channel, buf, len, now_ms());
}

static void on_event(void *ctx, const lr_steam_event *ev) {
    (void)ctx;
    switch (ev->type) {
    case LR_EV_LOBBY_CREATED:
        if (G.hosting_pending) {
            G.hosting_pending = 0;
            if (!launch_game(!G.saving)) {
                stop_session("spel kon niet starten");
                return;
            }
            /* launch_game queues verified preparation; its status stays visible until the child finishes. */
        }
        break;
    case LR_EV_LOBBY_FAILED:
        logline("lobby failed: %s", ev->text);
        stop_session(ev->text);
        message(ev->text, MB_ICONWARNING);
        break;
    case LR_EV_LOBBY_ENTERED:
        if (G.core.role == LR_ROLE_GUEST && G.core.peer == ev->user && session_active()) break;
        if (!lr_core_guest_join(&G.core, ev->user, now_ms())) {
            lr_steam_leave_lobby();
            message(G.core.error, MB_ICONWARNING);
        }
        break;
    case LR_EV_MEMBER_JOINED:
        logline("lobby: %llu joined", (unsigned long long)ev->user);
        break;
    case LR_EV_MEMBER_LEFT:
        logline("lobby: %llu left", (unsigned long long)ev->user);
        lr_core_peer_left(&G.core, ev->user);
        lr_steam_close_session(ev->user); /* half-open sessions broke a quick rejoin (1.1.0) */
        break;
    case LR_EV_JOIN_REQUESTED:
        logline("steam: join requested for lobby %llu", (unsigned long long)ev->lobby);
        if (session_active()) {
            message("Je zit al in een sessie. Klik eerst op Stop als je naar deze vriend wilt.", MB_ICONINFORMATION);
            return;
        }
        ShowWindow(G.wnd, SW_SHOWNORMAL);
        SetForegroundWindow(G.wnd);
        join_lobby(ev->lobby);
        break;
    }
}

/* ---- window contents ---- */

static void update_checks(void) {
    lr_state s = G.core.state;
    int host = G.core.role == LR_ROLE_HOST, guest = G.core.role == LR_ROLE_GUEST;
    if (lr_steam_lobby())
        set_check(CHK_LOBBY, 1, host ? "Lobby open (vrienden kunnen joinen)" : "In de lobby van de host");
    else
        set_check(CHK_LOBBY, 0, "Lobby: nog niet (klik Host, of join een vriend)");
    if ((host && (s == LR_ST_HOST_SENDING || s == LR_ST_HOST_PLAYING)) || (guest && s >= LR_ST_GUEST_RECEIVING && s != LR_ST_FAILED))
        set_check(CHK_FRIEND, 1, host ? "Vriend verbonden via Steam" : "Verbonden met de host via Steam");
    else
        set_check(CHK_FRIEND, 0, "Vriend: nog niet verbonden");
    if (strstr(G.runtime_line, "framework OFF"))
        set_check(CHK_RUNTIME, 0, "Co-op in het spel staat UIT: de runtime herkent dit spel niet volledig en doet niets "
                                  "(het spel draait gewoon). Er is een nieuwe ddcoop-runtime.dll nodig.");
    else if (G.core.host_spent)
        set_check(CHK_RUNTIME, 0, "De sessie van dit spel is afgelopen. Sluit het spel en klik opnieuw op Host.");
    else if (strstr(G.runtime_line, "host rejected or protocol"))
        set_check(CHK_RUNTIME, 0, "Het spel van de host antwoordt niet. Heeft de host slot %d geladen? Zo ja: de host sluit "
                                  "het spel en klikt opnieuw op Host.", G.core.cfg.coop_slot);
    else if (G.core.relay_up && G.core.relay.game_connected && strstr(G.runtime_line, "ready"))
        set_check(CHK_RUNTIME, 1, "Co-op in het spel actief. Laad allebei slot %d.", G.core.cfg.coop_slot);
    else if (G.core.relay_up && G.core.relay.game_connected)
        set_check(CHK_RUNTIME, 1, "Spellen verbonden via DDCoop (%s)", G.runtime_line[0] ? G.runtime_line : "wacht op runtime");
    else
        set_check(CHK_RUNTIME, 0, "Spellen: nog niet verbonden%s%s", G.runtime_line[0] ? " - laatste regel: " : "",
                  G.runtime_line);

    char text[4096];
    int n = 0;
    for (int i = 0; i < CHK_COUNT; i++)
        n += snprintf(text + n, sizeof text - (size_t)n, "%s  %s\r\n", G.ok[i] ? "[OK]" : "[ - ]", G.check_text[i]);
    wchar_t old[4096];
    GetWindowTextW(G.checks, old, 4096);
    if (wcscmp(old, W(text)) != 0) {
        int first=(int)SendMessageW(G.checks,EM_GETFIRSTVISIBLELINE,0,0);
        DWORD from=0,to=0;
        SendMessageW(G.checks,EM_GETSEL,(WPARAM)&from,(LPARAM)&to);
        SetWindowTextW(G.checks,W(text));
        int length=GetWindowTextLengthW(G.checks);
        SendMessageW(G.checks,EM_SETSEL,from>(DWORD)length?(WPARAM)length:from,to>(DWORD)length?(LPARAM)length:to);
        int now=(int)SendMessageW(G.checks,EM_GETFIRSTVISIBLELINE,0,0);
        SendMessageW(G.checks,EM_LINESCROLL,0,first-now);
    }

    char status[512];
    if (s == LR_ST_FAILED)
        snprintf(status, sizeof status, "Probleem: %s", G.core.error);
    else
        snprintf(status, sizeof status, "%s", G.core.status);
    if (strcmp(status, G.last_status) != 0) {
        snprintf(G.last_status, sizeof G.last_status, "%s", status);
        SetWindowTextW(G.status, W(status));
    }
}

static void update_friends(void) {
    lr_friend list[256];
    int n = lr_steam_friends(list, 256);
    uint64_t selected = 0;
    lr_friend *f = selected_friend();
    if (f) selected = f->id;
    memcpy(G.friends_list, list, sizeof(lr_friend) * (size_t)n);
    G.friend_count = n;
    SendMessageW(G.friends, WM_SETREDRAW, FALSE, 0);
    SendMessageW(G.friends, LB_RESETCONTENT, 0, 0);
    for (int i = 0; i < n; i++) {
        char line[256];
        snprintf(line, sizeof line, "%s   %s", list[i].name,
                 list[i].hosting ? "<< HOST DDCOOP - klik Join" : list[i].in_game ? "(in een spel)" : "(online)");
        LRESULT idx = SendMessageW(G.friends, LB_ADDSTRING, 0, (LPARAM)W(line));
        SendMessageW(G.friends, LB_SETITEMDATA, (WPARAM)idx, i);
        if (list[i].id == selected) SendMessageW(G.friends, LB_SETCURSEL, (WPARAM)idx, 0);
    }
    if (n == 0) SendMessageW(G.friends, LB_ADDSTRING, 0, (LPARAM)L"(geen vrienden online)");
    SendMessageW(G.friends, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(G.friends, NULL, TRUE);
}

static void update_runtime_line(void) {
    const char *name = G.core.role == LR_ROLE_HOST ? "ddcoop-fw-host.log" : G.core.role == LR_ROLE_GUEST ? "ddcoop-fw-guest.log" : NULL;
    if (!name) {
        G.runtime_line[0] = 0;
        return;
    }
    char path[LR_PATH_MAX];
    lr_join(path, sizeof path, G.moddir, name);
    uint8_t *data;
    size_t len;
    if (!lr_read_file(path, 32u * 1024 * 1024, &data, &len)) return;
    size_t end = len;
    while (end > 0 && (data[end - 1] == '\n' || data[end - 1] == '\r')) end--;
    size_t start = end;
    while (start > 0 && data[start - 1] != '\n') start--;
    size_t n = end - start < sizeof G.runtime_line - 1 ? end - start : sizeof G.runtime_line - 1;
    memcpy(G.runtime_line, data + start, n);
    G.runtime_line[n] = 0;
    /* Only what this game wrote: the log keeps earlier sessions too. */
    if (G.core.role == LR_ROLE_HOST && len > G.host_log_start && len - G.host_log_start < 64u * 1024 * 1024) {
        size_t k = len - G.host_log_start;
        char *tail = malloc(k + 1);
        if (tail) {
            memcpy(tail, data + G.host_log_start, k);
            tail[k] = 0;
            if (strstr(tail, "guest disconnected") || strstr(tail, "shared actions stopped")) lr_core_host_spent(&G.core);
            free(tail);
        }
    }
    free(data);
}

/* ---- hero list ---- */

static void set_heroes_hint(const char *text) {
    if (strcmp(text, G.heroes_hint_text) == 0) return;
    snprintf(G.heroes_hint_text, sizeof G.heroes_hint_text, "%s", text);
    SetWindowTextW(G.heroes_hint, W(text));
}

/* Redraws the list when the table changed (new revision, other role, session ended). */
#include "lr_live_party.inc"
static void update_heroes(void) {
    live_party_refresh();
    const lr_owners *o = &G.core.owners;
    int role = session_active() ? (int)G.core.role : 0;
    int me = lr_core_player(&G.core);
    uint64_t t = now_ms();
    if (G.heroes_recheck_at && t >= G.heroes_recheck_at) { /* a tick the host refused leaves the revision as it was */
        G.heroes_recheck_at = 0;
        G.heroes_shown = 0xFFFFFFFFu;
    }
    if (o->revision == G.heroes_shown && role == G.heroes_role) return;
    G.heroes_shown = o->revision;
    G.heroes_role = role;
    G.heroes_filling = 1; /* our own check changes are not clicks */
    SendMessageW(G.heroes, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(G.heroes);
    int order[LR_MAX_HEROES];
    G.party_count=lr_owners_live_order(o,G.live_party_ids,G.live_party_count,order,G.party_flags);
    for (int position = 0; role && position < o->count; position++) {
        int i=order[position];
        const lr_hero *h = &o->heroes[i];
        int party=G.party_flags[i];
        char cls[LR_HERO_CLASS_MAX + 1], level[8];
        snprintf(cls, sizeof cls, "%s", h->cls);
        for (char *c = cls; *c; c++)
            if (*c == '_') *c = ' ';
        snprintf(level, sizeof level, "%d", lr_hero_level(h->xp));
        const char *by = o->owner[i] == me ? "jij" : o->owner[i] == LR_PLAYER_HOST ? "host" : "vriend";
        LVITEMW it = {0};
        it.mask = LVIF_TEXT | LVIF_PARAM;
        it.iItem = position;
        wchar_t name[LR_HERO_NAME_MAX+8];
        swprintf(name,sizeof name/sizeof name[0],L"%ls%ls",party?L"\x25CF ":L"",W(h->name[0]?h->name:"?"));
        it.pszText = name;
        it.lParam = (LPARAM)h->id;
        int row = (int)SendMessageW(G.heroes, LVM_INSERTITEMW, 0, (LPARAM)&it);
        const char *cols[] = {cls, level, by};
        for (int k = 0; k < 3; k++) {
            LVITEMW sub = {0};
            sub.iSubItem = k + 1;
            sub.pszText = W(cols[k]);
            SendMessageW(G.heroes, LVM_SETITEMTEXTW, (WPARAM)row, (LPARAM)&sub);
        }
        ListView_SetCheckState(G.heroes, row, o->owner[i] == me);
    }
    SendMessageW(G.heroes, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(G.heroes, NULL, TRUE);
    G.heroes_filling = 0;
    if (t < G.heroes_hint_until) return; /* a message about the last click stays a moment */
    if (!role)
        set_heroes_hint("Host een spel of join een vriend; dan staan hier de helden van de co-op campagne.");
    else if (o->count == 0)
        set_heroes_hint(role == LR_ROLE_HOST ? "Geen helden gevonden in de campagne (zie DDCoop-launcher.log)."
                                             : "Wacht op de heldenlijst van de host...");
    else
        set_heroes_hint("\xe2\x97\x8f = actuele party uit het spel; deze staat bovenaan. Vink aan welke helden jij bestuurt; "
                        "wijzigingen gaan in zodra het spel een nieuwe keuze toestaat.");
}

/* A tick box was clicked: ask lr_core (the host decides). The list then follows the table again: the host's
 * own change is a new revision at once; a guest's comes back from the host, so check again after 2 s. */
static void on_hero_checked(const NMLISTVIEW *nm) {
    if (G.heroes_filling || !(nm->uChanged & LVIF_STATE)) return;
    UINT old_state = (nm->uOldState & LVIS_STATEIMAGEMASK) >> 12, new_state = (nm->uNewState & LVIS_STATEIMAGEMASK) >> 12;
    if (old_state == 0 || new_state == 0 || old_state == new_state) return; /* not a tick box change */
    uint32_t id = (uint32_t)nm->lParam;
    int claim = new_state == 2;
    logline("hero %u %s by this player", (unsigned)id, claim ? "ticked" : "unticked");
    if (!lr_core_claim(&G.core, id, claim)) {
        set_heroes_hint("Dat lukte niet; de lijst toont weer wie welke held heeft.");
        G.heroes_hint_until = now_ms() + 4000;
        G.heroes_shown = 0xFFFFFFFFu;
    } else if (G.core.role == LR_ROLE_GUEST) {
        G.heroes_recheck_at = now_ms() + 2000;
    }
}

/* Host: the game rewrites persist.roster.json after recruiting, dismissing and returning to town. */
static void refresh_roster(void) {
    if (G.core.role != LR_ROLE_HOST || !session_active() || !G.core.save_folder[0]) return;
    char dir[LR_PATH_MAX], path[LR_PATH_MAX];
    lr_join(dir, sizeof dir, G.core.cfg.remote_dir, G.core.save_folder);
    lr_join(path, sizeof path, dir, "persist.roster.json");
    uint8_t *data;
    size_t len;
    if (!lr_read_file(path, LR_MAX_FILE_SIZE, &data, &len)) return;
    uint32_t crc = lr_crc32(0, data, len);
    if (crc != G.roster_crc) {
        G.roster_crc = crc;
        lr_core_host_roster(&G.core, data, len);
    }
    free(data);
}

static void tick_body(void);
static void overlay_publish(void);

static void tick(void) {
    if (G.in_tick) return;
    G.in_tick = 1;
    tick_body();
    PHASE("idle");
    G.in_tick = 0;
    if (G.deferred[0]) {
        char text[512];
        snprintf(text, sizeof text, "%s", G.deferred);
        G.deferred[0] = 0;
        message(text, G.deferred_icon);
    }
}

static void tick_body(void) {
    uint64_t t = now_ms();
    static int ticks;
    if (ticks < 3) logline("tick %d", ++ticks);
    lr_state before = G.core.state;
    PHASE("tick: steam callbacks");
    lr_steam_pump();
    PHASE("tick: session");
    /* Steam callbacks may update last_rx with a newer timestamp than t captured above. */
    t = now_ms();
    lr_core_tick(&G.core, t);
    /* A guest session that fails on its own (missing DLC, lost host) gets a pop-up, not only the red status line.
     * Failures from a button (Host, Join) already show one there. */
    if (G.core.role == LR_ROLE_GUEST && before != LR_ST_FAILED && G.core.state == LR_ST_FAILED) {
        message(G.core.error, MB_ICONWARNING);
        collect_logs(0);
    }
    if (lr_core_take_launch(&G.core)) {
        if (!launch_game(1)) stop_session("spel kon niet starten");
    }
    if (G.hosting_pending && t - G.hosting_since > 20000) {
        logline("lobby: no answer from Steam in 20 s");
        stop_session("Steam gaf binnen 20 seconden geen lobby.");
        message(lr_steam_logged_on()
                    ? "Steam gaf binnen 20 seconden geen lobby. Probeer nog eens; lukt het weer niet, klik dan 'Logs kopiëren' en stuur de map."
                    : "Steam staat in offline-modus, dan kan er geen lobby gemaakt worden. Zet Steam online (menu Steam > Ga online) en probeer opnieuw.",
                MB_ICONWARNING);
    }
    if (G.game && WaitForSingleObject(G.game, 0) == WAIT_OBJECT_0) {
        int guest = restore_pending();
        DWORD exit_code = 0;
        GetExitCodeProcess(G.game, &exit_code);
        logline("game exited with code %lu; guest recovery pending=%d", exit_code, guest);
        CloseHandle(G.game);
        G.game = NULL;
        G.hotkey_ok=RegisterHotKey(G.wnd,ID_HOTKEY,MOD_NOREPEAT,VK_F9);
        stop_session("Darkest Dungeon is gesloten; sessie beëindigd. Klik Host of Join voor een nieuwe sessie.");
        if (G.new_campaign_slot) {
            int slot = G.new_campaign_slot;
            lr_campaign_entry list[9];
            G.new_campaign_slot = 0;
            lr_list_campaigns(G.core.cfg.remote_dir, list);
            snprintf(G.core.status, sizeof G.core.status,
                     list[slot - 1].empty ? "Er staat nog geen campagne in slot %d. Klik Host om het opnieuw te proberen." :
                     list[slot - 1].tutorial ? "Slot %d staat nog in de tutorial. Klik Host om verder te spelen." :
                     "De campagne in slot %d is klaar. Klik Host om samen te spelen. Is Steam nog bezig met opslaan, dan wacht DDCoop daar vanzelf op.", slot);
        }
        if (guest) collect_logs(0);
    }
    preparation_tick();
    if (t - G.last_friends > 3000) {
        G.last_friends = t;
        PHASE("tick: friends list");
        uint64_t f0 = now_ms();
        if (G.steam_ok) update_friends();
        if (now_ms() - f0 > 500) logline("friends list took %llu ms (%d online)", (unsigned long long)(now_ms() - f0), G.friend_count);
    }
    if (t - G.last_runtime > 1000) {
        G.last_runtime = t;
        PHASE("tick: runtime log");
        update_runtime_line();
        PHASE("tick: save folder");
        {
            /* The choice is for the next session: not while one is on, or its game or a restore is running. */
            int busy = session_active() || G.prepare || G.game;
            if (busy != G.slot_busy) {
                G.slot_busy = busy;
                EnableWindow(G.slot, !busy);
                EnableWindow(G.save_box, !busy);
                EnableWindow(G.undo, !busy);
                if (!busy) slot_fill();
            }
        }
        if (!session_active()) check_slot();
    }
    if (t - G.last_roster > 5000) {
        G.last_roster = t;
        PHASE("tick: roster");
        refresh_roster();
    }
    if (t - G.last_ui > 250) {
        G.last_ui = t;
        PHASE("tick: window text");
        update_checks();
        PHASE("tick: hero list");
        update_heroes();
        overlay_publish();
    }
}

/* ---- window ---- */

static HWND control(const wchar_t *cls, const wchar_t *text, DWORD style, int x, int y, int w, int h, int id, HFONT font) {
    DWORD ex = (wcscmp(cls, WC_LISTBOXW) == 0 || wcscmp(cls, L"EDIT") == 0) ? WS_EX_CLIENTEDGE : 0;
    HWND c = CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, D(x), D(y), D(w), D(h), G.wnd,
                             (HMENU)(INT_PTR)id, NULL, NULL);
    SendMessageW(c, WM_SETFONT, (WPARAM)font, TRUE);
    return c;
}

static void build_window(void) {
    int y = 12;
    G.status = control(L"STATIC", L"", SS_LEFT, 14, y, 640, 44, 0, G.big);
    y += 50;
    G.checks = control(L"EDIT", L"", WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL |
                       ES_READONLY | ES_NOHIDESEL, 14, y, 640, 150, 0, G.font);
    y += 156;
    control(L"STATIC", L"Campagne om te hosten:", SS_LEFT, 14, y + 5, 170, 20, 0, G.font);
    G.slot = control(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 190, y, 464, 240, ID_SLOT, G.font);
    y += 36;
    G.host = control(L"BUTTON", L"Host een spel", BS_PUSHBUTTON, 14, y, 200, 34, ID_HOST, G.font);
    G.stop = control(L"BUTTON", L"Stop sessie", BS_PUSHBUTTON, 224, y, 140, 34, ID_STOP, G.font);
    G.logs = control(L"BUTTON", L"Logs kopiëren", BS_PUSHBUTTON, 374, y, 140, 34, ID_LOGS, G.font);
    y += 42;
    G.save_box = control(L"BUTTON", L"Voortgang opslaan als ik host", BS_AUTOCHECKBOX | WS_TABSTOP, 14, y, 300, 26, ID_SAVE, G.font);
    G.undo = control(L"BUTTON", L"Back-up van dit slot terugzetten...", BS_PUSHBUTTON, 374, y - 2, 280, 30, ID_UNDO, G.font);
    y += 36;
    control(L"STATIC", L"Steam-vrienden (een host staat bovenaan):", SS_LEFT, 14, y, 400, 20, 0, G.font);
    y += 22;
    G.friends = control(WC_LISTBOXW, L"", LBS_NOTIFY | WS_VSCROLL | LBS_NOINTEGRALHEIGHT, 14, y, 460, 170, ID_FRIENDS, G.font);
    G.join = control(L"BUTTON", L"Join deze vriend", BS_PUSHBUTTON, 484, y, 170, 34, ID_JOIN, G.font);
    G.invite = control(L"BUTTON", L"Nodig uit", BS_PUSHBUTTON, 484, y + 42, 170, 34, ID_INVITE, G.font);
    y += 180;
    control(L"STATIC", L"Of met een code:", SS_LEFT, 14, y + 6, 120, 20, 0, G.font);
    G.code = control(L"EDIT", L"", ES_AUTOHSCROLL, 140, y, 200, 28, ID_CODE, G.font);
    G.join_code = control(L"BUTTON", L"Join code", BS_PUSHBUTTON, 350, y, 124, 28, ID_JOIN_CODE, G.font);
    G.copy_code = control(L"BUTTON", L"Kopieer mijn code", BS_PUSHBUTTON, 484, y, 170, 28, ID_COPY_CODE, G.font);
    y += 40;
    G.hint = control(L"STATIC", L"", SS_LEFT, 14, y, 640, 40, 0, G.font);

    /* right column: the heroes of the co-op campaign */
    control(L"STATIC", L"Wie bestuurt welke held?", SS_LEFT, 674, 12, 370, 22, 0, G.big);
    G.heroes = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                               WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_NOSORTHEADER |
                                   LVS_SHOWSELALWAYS,
                               D(674), D(40), D(370), D(576), G.wnd, (HMENU)(INT_PTR)ID_HEROES, NULL, NULL);
    SendMessageW(G.heroes, WM_SETFONT, (WPARAM)G.font, TRUE);
    LOGFONTW party_style;
    if(GetObjectW(G.font,sizeof party_style,&party_style)) {
        party_style.lfWeight=FW_BOLD;G.party_font=CreateFontIndirectW(&party_style);
    }
    ListView_SetExtendedListViewStyle(G.heroes, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    static const struct { const wchar_t *name; int width; } cols[] = {
        {L"Held", 112}, {L"Klasse", 120}, {L"Niv.", 44}, {L"Speler", 60}};
    for (int i = 0; i < 4; i++) {
        LVCOLUMNW c = {0};
        c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        c.pszText = (wchar_t *)cols[i].name;
        c.cx = D(cols[i].width);
        c.iSubItem = i;
        SendMessageW(G.heroes, LVM_INSERTCOLUMNW, (WPARAM)i, (LPARAM)&c);
    }
    G.heroes_hint = control(L"STATIC", L"", SS_LEFT, 674, 624, 370, 90, 0, G.font);
    G.heroes_shown = 0xFFFFFFFFu;
    wchar_t hint[400];
    swprintf(hint, 400, L"%ls  Werkt het best met Darkest Dungeon in venster of 'borderless'. DDCoop %hs",
             G.hotkey_ok ? L"F9 = dit venster tonen of verbergen, ook midden in het spel." : L"F9 is bezet door een ander programma; gebruik het DDCoop-icoon in de taakbalk.",
             DDC_VERSION);
    SetWindowTextW(G.hint, hint);
}

#include "lr_overlay.inc"

static void toggle_from_hotkey(void) {
    if (IsWindowVisible(G.wnd) && !IsIconic(G.wnd) && GetForegroundWindow() == G.wnd) {
        ShowWindow(G.wnd, SW_MINIMIZE);
        return;
    }
    ShowWindow(G.wnd, IsIconic(G.wnd) ? SW_RESTORE : SW_SHOWNORMAL);
    SetWindowPos(G.wnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    SetForegroundWindow(G.wnd);
}

static LRESULT CALLBACK wndproc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if(g_overlay_message && msg==g_overlay_message){overlay_command(wp,lp);return 0;}
    switch (msg) {
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_HOST: do_host(); break;
        case ID_JOIN: do_join_selected(); break;
        case ID_INVITE: do_invite_selected(); break;
        case ID_JOIN_CODE: do_join_code(); break;
        case ID_COPY_CODE: do_copy_code(); break;
        case ID_LOGS: do_logs(); break;
        case ID_UNDO: do_undo(); break;
        case ID_SAVE:
            if (HIWORD(wp) == BN_CLICKED) {
                G.saving = SendMessageW(G.save_box, BM_GETCHECK, 0, 0) == BST_CHECKED;
                saving_remember(G.saving);
                logline("host saving %s", G.saving ? "on" : "off");
            }
            break;
        case ID_STOP:
            stop_session("gestopt. Klik Host of Join voor een nieuwe sessie.");
            if (G.game) request_game_close();
            break;
        case ID_FRIENDS:
            if (HIWORD(wp) == LBN_DBLCLK) do_join_selected();
            break;
        case ID_SLOT:
            if (HIWORD(wp) == CBN_DROPDOWN) slot_fill();
            if (HIWORD(wp) == CBN_SELCHANGE) slot_chosen();
            break;
        }
        return 0;
    case WM_NOTIFY: {
        const NMHDR *nh = (const NMHDR *)lp;
        if (nh->idFrom == ID_HEROES && nh->code == LVN_ITEMCHANGED) on_hero_checked((const NMLISTVIEW *)lp);
        if(nh->idFrom==ID_HEROES && nh->code==NM_CUSTOMDRAW) {
            NMLVCUSTOMDRAW *draw=(NMLVCUSTOMDRAW *)lp;
            if(draw->nmcd.dwDrawStage==CDDS_PREPAINT)return CDRF_NOTIFYITEMDRAW;
            if(draw->nmcd.dwDrawStage==CDDS_ITEMPREPAINT) {
                int i=lr_owners_find(&G.core.owners,(uint32_t)draw->nmcd.lItemlParam);
                if(i>=0 && G.party_flags[i]) {
                    draw->clrTextBk=RGB(255,242,204);draw->clrText=RGB(90,52,12);
                    if(G.party_font)SelectObject(draw->nmcd.hdc,G.party_font);
                    return CDRF_NEWFONT;
                }
            }
        }
        return 0;
    }
    case WM_TIMER:
        if (wp == ID_TIMER) tick();
        return 0;
    case WM_HOTKEY:
        if (wp == ID_HOTKEY) toggle_from_hotkey();
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE) SetWindowPos(h, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        return 0;
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wp;
        if ((HWND)lp == G.status) SetTextColor(dc, G.core.state == LR_ST_FAILED ? RGB(170, 20, 20) : RGB(20, 90, 20));
        SetBkMode(dc, TRANSPARENT);
        return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
    }
    case WM_CLOSE:
        if ((G.prepare || G.game || restore_pending()) && !G.restore_failed) {
            G.want_close = 1;
            stop_session("Spel afsluiten en eigen campagne herstellen voordat DDCoop sluit...");
            request_game_close();
            return 0;
        }
        if (session_active() && G.game &&
            MessageBoxW(h, L"Er loopt een co-op sessie. Als je DDCoop sluit, stopt de verbinding met je vriend. Toch sluiten?",
                        L"DDCoop", MB_YESNO | MB_ICONQUESTION) != IDYES)
            return 0;
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        live_party_close();
        overlay_close();
        if(G.party_font){DeleteObject(G.party_font);G.party_font=NULL;}
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR cmdline, int show) {
    (void)prev;
    int argc = 0;
    wchar_t **argv = CommandLineToArgvW(cmdline && *cmdline ? GetCommandLineW() : L"DDCoop.exe", &argc);
    if (argv && argc >= 2 && wcscmp(argv[1], L"--prepare-save") == 0) return lr_prepare_win_main(argc, argv);
    if (argv && argc >= 2 && wcsncmp(argv[1], L"--", 2) == 0) return lr_install_main(argv[1]);
    HANDLE mutex = CreateMutexW(NULL, TRUE, L"Local\\DDCoopLauncher");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND other = FindWindowW(L"DDCoopWindow", NULL);
        if (other) {
            ShowWindow(other, SW_RESTORE);
            SetForegroundWindow(other);
        }
        return 0;
    }
    find_paths();
    lr_join(G.log_path, sizeof G.log_path, G.moddir, "DDCoop-launcher.log");
    G.log = _wfopen(W(G.log_path), L"w");
    PHASE("startup");
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &g_gui_thread, 0, FALSE,
                    DUPLICATE_SAME_ACCESS);
    CloseHandle(CreateThread(NULL, 0, watchdog, NULL, 0, NULL));
    logline("DDCoop %s starting; game folder %s", DDC_VERSION, G.root);
    lr_sock_startup();

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = wndproc;
    wc.hInstance = inst;
    wc.lpszClassName = L"DDCoopWindow";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    RegisterClassW(&wc);
    {
        HDC screen = GetDC(NULL);
        g_dpi = GetDeviceCaps(screen, LOGPIXELSY);
        ReleaseDC(NULL, screen);
        if (g_dpi < 96) g_dpi = 96;
    }
    INITCOMMONCONTROLSEX icc = {sizeof icc, ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&icc);
    RECT r = {0, 0, D(1058), D(726)};
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRect(&r, style, FALSE);
    G.wnd = CreateWindowExW(0, L"DDCoopWindow", L"DDCoop - Darkest Dungeon co-op", style, CW_USEDEFAULT, CW_USEDEFAULT,
                            r.right - r.left, r.bottom - r.top, NULL, NULL, inst, NULL);
    G.font = CreateFontW(-D(15), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    G.big = CreateFontW(-D(18), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    G.hotkey_ok = RegisterHotKey(G.wnd, ID_HOTKEY, MOD_NOREPEAT, VK_F9);
    g_overlay_message=RegisterWindowMessageW(DD_OVERLAY_MESSAGE);
    build_window();

    lr_config cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.send = lr_steam_send;
    cfg.allowed = allowed_cb;
    cfg.game_running = game_running_cb;
    cfg.has_dlc = has_dlc_cb;
    cfg.random = random_cb;
    cfg.log = log_cb;
    cfg.coop_slot = 3;
    {
        char kit[9];
        memcpy(kit, LR_HASH_RUNTIME, 8);
        kit[8] = 0;
        cfg.kit = (uint32_t)strtoul(kit, NULL, 16);
    }
    lr_join(cfg.ini_path, sizeof cfg.ini_path, G.moddir, "ddcoop.ini");
    lr_join(cfg.heroes_path, sizeof cfg.heroes_path, G.moddir, "ddcoop-heroes.ini");
    lr_join(cfg.restore_path, sizeof cfg.restore_path, G.moddir, "ddcoop-guest-original.bin");
    {
        wchar_t profile[MAX_PATH];
        char p[LR_PATH_MAX];
        if (SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, profile) == S_OK) {
            to_utf8(profile, p, sizeof p);
            lr_join(cfg.backup_root, sizeof cfg.backup_root, p, "DDCoop-backups");
        }
    }
    check_install();
    {
        /* both launchers say which build they run; the host refuses a different one */
        char exe_hex[65], exe[LR_PATH_MAX];
        lr_join(exe, sizeof exe, G.bin, "Darkest.exe");
        if (sha256_file(exe, exe_hex)) {
            exe_hex[8] = 0;
            cfg.build = (uint32_t)strtoul(exe_hex, NULL, 16);
        }
    }

    lr_steam_callbacks cb = {0};
    cb.on_message = on_message;
    cb.on_event = on_event;
    cb.accept_session = accept_session_cb;
    cb.log = log_cb;
    char err[256] = "";
    wchar_t dll[LR_PATH_MAX];
    {
        wchar_t exe[LR_PATH_MAX];
        GetModuleFileNameW(NULL, exe, LR_PATH_MAX);
        wchar_t *s = wcsrchr(exe, L'\\');
        if (s) *s = 0;
        swprintf(dll, LR_PATH_MAX, L"%ls\\steam_api64.dll", exe);
        SetCurrentDirectoryW(exe); /* steam_appid.txt (480) is read from here */
    }
    G.steam_ok = lr_steam_start(dll, &cb, err, sizeof err);
    if (G.steam_ok) {
        cfg.steam_user = lr_steam_me();
        if (lr_steam_logged_on())
            set_check(CHK_STEAM, 1, "Steam: ingelogd als %s", lr_steam_my_name());
        else
            set_check(CHK_STEAM, 0, "Steam staat in offline-modus (%s). Zet Steam online: menu Steam > Ga online.",
                      lr_steam_my_name());
        steam_userdata_remote(cfg.remote_dir, sizeof cfg.remote_dir);
        logline("save folder root: %s", cfg.remote_dir);
        log_save_folders(cfg.remote_dir);
    } else {
        set_check(CHK_STEAM, 0, "%s", err);
        logline("steam: %s", err);
    }
    PHASE("startup: session init");
    lr_core_init(&G.core, &cfg);
    G.chosen_slot = slot_remembered();
    G.saving = saving_remembered();
    SendMessageW(G.save_box, BM_SETCHECK, G.saving ? BST_CHECKED : BST_UNCHECKED, 0);
    slot_fill();
    /* A crash or power cut can leave a session ini behind; a normal Steam start must stay single player. */
    PHASE("startup: ini reset");
    lr_write_ini(cfg.ini_path, "off", LR_DEFAULT_GAME_PORT, "unused-until-ddcoop-starts", cfg.coop_slot);
    logline("startup: ini reset to role=off");
    snprintf(G.core.status, sizeof G.core.status,
             G.install_ok && G.steam_ok ? "Klaar. Host een spel, of kies een vriend die host en klik Join."
                                        : "Nog niet klaar: los eerst de punten met [ - ] hieronder op.");
    PHASE("startup: checks");
    check_slot();
    update_checks();
    PHASE("startup: show window");
    ShowWindow(G.wnd, show);
    UpdateWindow(G.wnd);
    SetTimer(G.wnd, ID_TIMER, 10, NULL);
    logline("startup: window shown, entering the message loop");
    PHASE("message loop");
    timeBeginPeriod(1);

    MSG m;
    while (GetMessageW(&m, NULL, 0, 0) > 0) {
        if (IsDialogMessageW(G.wnd, &m)) continue;
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    timeEndPeriod(1);
    stop_session("afgesloten");
    lr_steam_stop();
    if (G.game) CloseHandle(G.game);
    logline("DDCoop closed");
    if (G.log) fclose(G.log);
    ReleaseMutex(mutex);
    return 0;
}
