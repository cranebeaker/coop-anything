/* Darkest Dungeon co-op runtime plugin. The SDL2 bootstrap loads this DLL from mods/DDCoop. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dd_addr.h"
#include "dd_config.h"
#include "dd_log.h"
#include "dd_runtime.h"
#include "dd_overlay.h"
#include "dd_saves.h"
#include "dd_focus.h"
#ifdef DD_DUO
#include "dd_duo.h"
#endif

#define DD_FW_VERSION "0.5.0-test57"
/* Save storage (runtime test 8): global StorageManager*; the loaded campaign is the device mounted in
 * device slot 5 (manager + 0x10 + slot * 8, StorageManager::IO_MountDevice 0x14024a820); a device's name
 * is at device + 0x20 (StorageDevice constructor). Valid for the known build only. */
#define STORAGE_MANAGER_VA 0x141185248ULL
#define STORAGE_DEVICE_SLOTS 0x10
#define CAMPAIGN_DEVICE_SLOT 5
#define DEVICE_NAME 0x20
#define CAMPAIGN_CURRENT_VA 0x14118e138ULL
#define CAMPAIGN_ROSTER_VECTOR 0x20
/* Game builds this version was made for (sha256 of _windows\win64\Darkest.exe). */
static const char *k_known_builds[] = {
    "f9d914ca1d7d9ad3ff4f151a46e4853e532ce1f5f109f700b0c1381a3f7e0d12", /* Steam build 28385 / 26437048, Nov 2026 */
};

typedef enum { DD_STATE_STARTING, DD_STATE_READY, DD_STATE_OFF } dd_state;
static volatile dd_state g_state = DD_STATE_STARTING;
static dd_config g_cfg;
static HMODULE g_self;

static void path_next_to_module(char *out, size_t len, const char *file) {
    DWORD n = GetModuleFileNameA(g_self, out, (DWORD)len);
    if (!n || n >= len) { out[0] = 0; return; }
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
        *code_va = (uintptr_t)base + sec->VirtualAddress;
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

/* Runtime test 2: Campaign::Current() owns the roster vector at +0x20. The menu may briefly mount
 * every profile in storage device slot 5, so require a live campaign with roster entries as well. */
static int campaign_fully_loaded(void) {
    uintptr_t base = (uintptr_t)GetModuleHandleA(NULL), campaign = 0, roster_begin = 0, roster_end = 0;
    if (!safe_read(base + (uintptr_t)(CAMPAIGN_CURRENT_VA - DD_IMAGE_BASE), &campaign, sizeof campaign) || !campaign) return 0;
    if (!safe_read(campaign + CAMPAIGN_ROSTER_VECTOR, &roster_begin, sizeof roster_begin) ||
        !safe_read(campaign + CAMPAIGN_ROSTER_VECTOR + sizeof(uintptr_t), &roster_end, sizeof roster_end)) return 0;
    if (!roster_begin || roster_end < roster_begin || (roster_end - roster_begin) % 8 != 0) return 0;
    return roster_end - roster_begin >= 8 && roster_end - roster_begin <= 8 * 64;
}

/* Watches which campaign is loaded and logs whether co-op is allowed for it. Read only. */
static DWORD WINAPI slot_watch(void *mask_ptr) {
    uint32_t mask = *(uint32_t *)mask_ptr;
    char last[0x80] = "?";
    for (;;) {
        char name[0x80];
        loaded_campaign_device(name);
        int campaign_ready = campaign_fully_loaded();
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
        int pos = 0;
        dd_slot_decision decision = dd_slot_decide(name, mask, &pos);
        dd_runtime_campaign_active(decision == DD_SLOT_COOP && campaign_ready);
        Sleep(2000);
    }
    return 0; /* not reached: the watch runs for the life of the game */
}

#ifdef DD_DUO
/* Its own thread: changing a title asks the window's thread, which may be busy loading. */
static DWORD WINAPI duo_title_watch(void *title) {
    int told = 0;
    for (;;) {
        if (dd_duo_rename((const char *)title) && !told) {
            told = 1;
            dd_log("window title set to \"%s\"", (const char *)title);
        }
        Sleep(500);
    }
    return 0;
}
#endif

static DWORD WINAPI startup(void *unused) {
    (void)unused;
    char path[MAX_PATH];
    path_next_to_module(path, sizeof path, "ddcoop.ini");
    int have_ini = dd_config_load(path, &g_cfg);
#ifdef DD_DUO
    {
        char duo[16] = "";
        GetEnvironmentVariableA("DDCOOP_DUO_ROLE", duo, sizeof duo);
        g_cfg.role = dd_duo_role(duo, g_cfg.role);
    }
#endif
    path_next_to_module(path, sizeof path, g_cfg.role == DD_ROLE_OFF ? "ddcoop-fw.log" :
                     g_cfg.role == DD_ROLE_HOST ? "ddcoop-fw-host.log" : "ddcoop-fw-guest.log");
    dd_log_open(path);
    dd_log("=== Darkest Dungeon co-op framework %s starting ===", DD_FW_VERSION);
#ifdef DD_DUO
    dd_log("TWO WINDOWS TEST BUILD: this copy is the %s", dd_role_name(g_cfg.role));
    dd_log("DUO graphics pacing: minimum 16 ms per frame (about 60 FPS per window)");
    {
        HANDLE title = CreateThread(NULL, 0, duo_title_watch, (void *)dd_duo_title(g_cfg.role), 0, NULL);
        if (title) CloseHandle(title);
    }
#endif
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
    if (known) dd_runtime_frontend_early(&g_cfg);

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

    if (ok != dd_address_count || (!known && relocated != 0)) {
        g_state = DD_STATE_OFF;
        dd_log("framework OFF: this game build is not fully supported. The game runs as normal.");
        return 0;
    }
    if (!known) {
        g_state = DD_STATE_OFF;
        dd_log("save slot and town integration need the known game build; features remain off");
        return 0;
    }

    const dd_addr *assign_addr = dd_addr_find(dd_addresses, dd_address_count, "activity_assign_hero");
    const dd_addr *widget_addr = dd_addr_find(dd_addresses, dd_address_count, "activity_slot_widget_ctor");
    const dd_addr *upgrade_addr = dd_addr_find(dd_addresses, dd_address_count, "buy_upgrade");
    const dd_addr *wagon_addr = dd_addr_find(dd_addresses, dd_address_count, "wagon_buy");
    const dd_addr *price_addr = dd_addr_find(dd_addresses, dd_address_count, "trinket_price");
    const dd_addr *recruit_addr=dd_addr_find(dd_addresses,dd_address_count,"hero_recruit");
    const dd_addr *sell_addr=dd_addr_find(dd_addresses,dd_address_count,"wagon_sell");
    const dd_addr *show_addr=dd_addr_find(dd_addresses,dd_address_count,"recruit_panel_show");
    const dd_addr *hide_addr=dd_addr_find(dd_addresses,dd_address_count,"recruit_panel_hide");
    const dd_addr *add_addr=dd_addr_find(dd_addresses,dd_address_count,"roster_add_entry");
    const dd_addr *count_addr=dd_addr_find(dd_addresses,dd_address_count,"roster_recruit_count");
    const dd_addr *capacity_addr=dd_addr_find(dd_addresses,dd_address_count,"roster_capacity");
    const dd_addr *drop_addr=dd_addr_find(dd_addresses,dd_address_count,"roster_ui_drop");
    uint8_t build_id[8] = {0};
    if (known) for (int i = 0; i < 8; i++) {
        unsigned byte = 0;
        sscanf(hex + i * 2, "%2x", &byte);
        build_id[i] = (uint8_t)byte;
    }
    if (!assign_addr || !widget_addr || !upgrade_addr || !wagon_addr || !price_addr || !recruit_addr || !show_addr || !hide_addr ||
        !add_addr || !count_addr || !capacity_addr || !drop_addr || !sell_addr ||
        !dd_runtime_init(&g_cfg, build_id, assign_addr->resolved_va, widget_addr->resolved_va,
                         upgrade_addr->resolved_va, wagon_addr->resolved_va, price_addr->resolved_va,
                         recruit_addr->resolved_va,show_addr->resolved_va,hide_addr->resolved_va,
                         add_addr->resolved_va,count_addr->resolved_va,capacity_addr->resolved_va,drop_addr->resolved_va,sell_addr->resolved_va)) {
        dd_log("town integration could not initialize; framework stays read-only");
        g_state = DD_STATE_OFF;
        return 0;
    }
    g_state = DD_STATE_READY;
    dd_log("framework READY (town placement sync is gated to a loaded configured co-op campaign)");

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
    HANDLE w = CreateThread(NULL, 0, slot_watch, &coop_mask, 0, NULL);
    if (w) CloseHandle(w);
    return 0;
}

/* Called by the small SDL2 bootstrap after the real SDL_PollEvent, on the game thread. */
__declspec(dllexport) void ddcoop_poll(void) {
    if (g_state == DD_STATE_READY) dd_runtime_poll();
}
__declspec(dllexport) void ddcoop_draw(void *window){
    dd_overlay_draw(window);
#ifdef DD_DUO
    static uint64_t previous;
    DWORD wait = dd_duo_frame_wait(GetTickCount64(), previous);
    if (wait) Sleep(wait);
    previous = GetTickCount64();
#endif
}
__declspec(dllexport) int ddcoop_event(const void *event){
    if (g_cfg.role != DD_ROLE_OFF && dd_focus_lost(event)) {
        dd_overlay_event(event); /* The panel still closes. */
        return 1;
    }
    if (dd_overlay_event(event)) return 1;
    return dd_runtime_input_stopped() && dd_gameplay_input(event);
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    g_self = inst;
    DisableThreadLibraryCalls(inst);
    /* Keep DllMain small (it runs under the loader lock): the real work happens on its own thread. */
    HANDLE t = CreateThread(NULL, 0, startup, NULL, 0, NULL);
    if (t) CloseHandle(t);
    return TRUE;
}
