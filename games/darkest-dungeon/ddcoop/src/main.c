/* Darkest Dungeon co-op framework: entry point.
 *
 * Loaded as SDL2.dll next to Darkest.exe (every SDL export forwards to SDL2_orig.dll, see
 * sdl2_forward.def). At this stage it changes nothing in the game: it reads its settings, checks the
 * game build, verifies every mapped game address against the running code (dd_addr.c) and reports the
 * result in ddcoop-fw.log. Co-op features build on this and only switch on when the check passed. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dd_addr.h"
#include "dd_config.h"
#include "dd_log.h"
#include "dd_saves.h"

#define DD_FW_VERSION "0.2.0"
/* Save storage (runtime test 8): global StorageManager*; the loaded campaign is the device mounted in
 * device slot 5 (manager + 0x10 + slot * 8, StorageManager::IO_MountDevice 0x140248f60); a device's name
 * is at device + 0x20 (StorageDevice constructor). Valid for the known build only. */
#define STORAGE_MANAGER_VA 0x141174ac8ULL
#define STORAGE_DEVICE_SLOTS 0x10
#define CAMPAIGN_DEVICE_SLOT 5
#define DEVICE_NAME 0x20
/* Game builds this version was made for (sha256 of _windows\win64\Darkest.exe). */
static const char *k_known_builds[] = {
    "4d78fbfaa65b7d002d2208d02da809200f4a5723119521180e30100d6f1a75f2", /* Steam, linked 2026-09-21 */
};

typedef enum { DD_STATE_STARTING, DD_STATE_READY, DD_STATE_OFF } dd_state;
static volatile dd_state g_state = DD_STATE_STARTING;
static dd_config g_cfg;

static void path_next_to_exe(char *out, size_t len, const char *file) {
    GetModuleFileNameA(NULL, out, (DWORD)len);
    char *slash = strrchr(out, '\\');
    if (slash) slash[1] = 0;
    strncat(out, file, len - strlen(out) - 1);
}

static int sha256_file(const char *path, char hex[65]) {
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return 0;
    BCRYPT_ALG_HANDLE alg;
    BCRYPT_HASH_HANDLE h;
    static UCHAR chunk[1 << 16];
    UCHAR digest[32];
    DWORD got;
    BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0);
    BCryptCreateHash(alg, &h, NULL, 0, NULL, 0, 0);
    while (ReadFile(f, chunk, sizeof chunk, &got, NULL) && got) BCryptHashData(h, chunk, got, 0);
    BCryptFinishHash(h, digest, sizeof digest, 0);
    BCryptDestroyHash(h);
    BCryptCloseAlgorithmProvider(alg, 0);
    CloseHandle(f);
    for (int i = 0; i < 32; i++) sprintf(hex + 2 * i, "%02x", digest[i]);
    return 1;
}

/* Finds the game's .text section as mapped in memory. */
static int find_code(const uint8_t **code, size_t *size, uint64_t *code_va) {
    const uint8_t *base = (const uint8_t *)GetModuleHandleA(NULL);
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)base;
    const IMAGE_NT_HEADERS64 *nt = (const IMAGE_NT_HEADERS64 *)(base + dos->e_lfanew);
    const IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        if (memcmp(sec->Name, ".text", 5) != 0) continue;
        *code = base + sec->VirtualAddress;
        *size = sec->Misc.VirtualSize;
        *code_va = DD_IMAGE_BASE + sec->VirtualAddress; /* addresses in the table use the preferred base */
        return 1;
    }
    return 0;
}

/* Reads game memory without risking a crash (the game may free a block while we look at it). */
static int safe_read(uintptr_t addr, void *dst, size_t len) {
    SIZE_T got = 0;
    return ReadProcessMemory(GetCurrentProcess(), (const void *)addr, dst, len, &got) && got == len;
}

/* Name of the storage device mounted as the loaded campaign, or "" if none can be read. */
static void loaded_campaign_device(char name[0x80]) {
    uintptr_t base = (uintptr_t)GetModuleHandleA(NULL), manager = 0, device = 0;
    name[0] = 0;
    if (!safe_read(base + (uintptr_t)(STORAGE_MANAGER_VA - DD_IMAGE_BASE), &manager, sizeof manager) || !manager) return;
    if (!safe_read(manager + STORAGE_DEVICE_SLOTS + CAMPAIGN_DEVICE_SLOT * 8, &device, sizeof device) || !device) return;
    if (!safe_read(device + DEVICE_NAME, name, 0x80)) name[0] = 0;
    name[0x7f] = 0;
}

/* Watches which campaign is loaded and logs whether co-op is allowed for it. Read only. */
static DWORD WINAPI slot_watch(void *mask_ptr) {
    uint32_t mask = *(uint32_t *)mask_ptr;
    char last[0x80] = "?";
    for (;;) {
        char name[0x80];
        loaded_campaign_device(name);
        if (strcmp(name, last) != 0) {
            int pos = 0;
            dd_slot_decision d = dd_slot_decide(name, mask, &pos);
            if (d == DD_SLOT_NO_CAMPAIGN)
                dd_log("save slot: no campaign loaded (device \"%s\")", name);
            else if (d == DD_SLOT_REFUSED)
                dd_log("save slot: device \"%s\" is not a campaign slot; co-op refused", name);
            else
                dd_log("save slot: campaign in menu slot %d (\"%s\"): %s", pos, name, dd_slot_decision_name(d));
            memcpy(last, name, sizeof last);
        }
        Sleep(2000);
    }
    return 0; /* not reached: the watch runs for the life of the game */
}

static DWORD WINAPI startup(void *unused) {
    (void)unused;
    char path[MAX_PATH];
    path_next_to_exe(path, sizeof path, "ddcoop-fw.log");
    dd_log_open(path);
    dd_log("=== Darkest Dungeon co-op framework %s starting ===", DD_FW_VERSION);

    path_next_to_exe(path, sizeof path, "ddcoop.ini");
    int have_ini = dd_config_load(path, &g_cfg);
    dd_log("settings: %s; role=%s port=%d host=%s password=%s verbose=%d", have_ini ? "ddcoop.ini" : "defaults (no ddcoop.ini)",
           dd_role_name(g_cfg.role), g_cfg.port, g_cfg.host[0] ? g_cfg.host : "-",
           g_cfg.password[0] ? "(set)" : "(empty)", g_cfg.verbose);

    char exe[MAX_PATH], hex[65] = "";
    GetModuleFileNameA(NULL, exe, sizeof exe);
    int known = 0;
    if (sha256_file(exe, hex))
        for (size_t i = 0; i < sizeof k_known_builds / sizeof k_known_builds[0]; i++)
            if (strcmp(hex, k_known_builds[i]) == 0) known = 1;
    dd_log("game build: sha256 %s (%s)", hex[0] ? hex : "unreadable", known ? "known" : "UNKNOWN");

    const uint8_t *code;
    size_t code_size;
    uint64_t code_va;
    if (!find_code(&code, &code_size, &code_va)) {
        dd_log("could not find the game's code section; framework OFF");
        g_state = DD_STATE_OFF;
        return 0;
    }
    size_t ok = dd_addr_resolve(dd_addresses, dd_address_count, code, code_size, code_va);
    size_t relocated = 0;
    for (size_t k = 0; k < dd_address_count; k++) {
        const dd_addr *a = &dd_addresses[k];
        if (a->status == DD_ADDR_RELOCATED) relocated++;
        if (a->status != DD_ADDR_EXACT || g_cfg.verbose)
            dd_log("  address %-26s 0x%llx -> %s 0x%llx", a->name, (unsigned long long)a->va, dd_addr_status_name(a->status),
                   (unsigned long long)a->resolved_va);
    }
    dd_log("addresses: %zu of %zu resolved (%zu relocated)", ok, dd_address_count, relocated);

    if (ok == dd_address_count && (known || relocated == 0)) {
        g_state = DD_STATE_READY;
        dd_log("framework READY (no co-op features yet; nothing in the game is changed)");
    } else {
        g_state = DD_STATE_OFF;
        dd_log("framework OFF: this game build is not fully supported. The game runs as normal.");
        return 0;
    }

    /* Co-op campaigns. A malformed setting switches co-op off everywhere instead of guessing. */
    int bad = 0;
    static uint32_t coop_mask;
    coop_mask = dd_coop_slots_parse(g_cfg.coop_slots, &bad);
    if (bad) {
        coop_mask = 0;
        dd_log("coop_slots = \"%s\" is not a list of campaign slots 1..9 (e.g. 3 or 3,7): co-op OFF for every campaign",
               g_cfg.coop_slots);
    } else if (!coop_mask) {
        dd_log("coop_slots is empty: every campaign is single player");
    } else {
        char list[64] = "";
        for (int k = 1; k <= DD_MENU_SLOTS; k++)
            if ((coop_mask >> k) & 1) snprintf(list + strlen(list), sizeof list - strlen(list), "%s%d", list[0] ? "," : "", k);
        dd_log("co-op campaigns: menu slot(s) %s", list);
    }
    if (!known) {
        dd_log("save slot check needs the known game build; co-op campaigns are not detected on this build");
        return 0;
    }
    HANDLE w = CreateThread(NULL, 0, slot_watch, &coop_mask, 0, NULL);
    if (w) CloseHandle(w);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(inst);
    /* Keep DllMain small (it runs under the loader lock): the real work happens on its own thread. */
    HANDLE t = CreateThread(NULL, 0, startup, NULL, 0, NULL);
    if (t) CloseHandle(t);
    return TRUE;
}
