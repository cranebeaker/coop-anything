/* Actual launcher reader and named mapping, isolated from game/Steam/saves/UI. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../src/dd_party_wire.h"
static struct {
    HANDLE game;
    int live_party_count;
    uint32_t live_party_ids[4], heroes_shown;
} G;
static uint64_t now_ms(void) { return 1000; }
#include "../src/lr_live_party.inc"
static int failures;
#define CHECK(x) do { if (!(x)) { printf("FAIL line%d: %s\n", __LINE__, #x); failures++; } } while (0)
int main(void) {
    G.game = GetCurrentProcess();
    wchar_t name[96];
    swprintf(name, 96, L"Local\\DDCoopParty-%lu", GetCurrentProcessId());
    HANDLE mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(dd_party_snapshot), name);
    dd_party_snapshot *writer = mapping ? MapViewOfFile(mapping, FILE_MAP_WRITE, 0, 0, sizeof(dd_party_snapshot)) : NULL;
    CHECK(writer != NULL);
    if (!writer) return 1;
    dd_party_snapshot next = {0};
    next.magic = DD_PARTY_MAGIC; next.version = DD_PARTY_VERSION; next.size = sizeof next;
    next.pid = GetCurrentProcessId(); next.updated = now_ms(); next.mode = DD_PARTY_RAID; next.count = 4;
    uint32_t ids[4] = {1, 2, 15, 7}; memcpy(next.ids, ids, sizeof ids);
    dd_party_write(writer, &next);
    G.heroes_shown = 17;
    live_party_refresh();
    CHECK(G.live_party_count == 4 && !memcmp(G.live_party_ids, ids, sizeof ids) && G.heroes_shown == UINT32_MAX);
    MEMORY_BASIC_INFORMATION mbi;
    CHECK(VirtualQuery(g_live_party_view, &mbi, sizeof mbi) && mbi.Protect == PAGE_READONLY);
    G.heroes_shown = 17;
    live_party_refresh(); CHECK(G.heroes_shown == 17); /* Same party does not rebuild the UI each timer tick. */
    next.ids[0] = 7; next.ids[3] = 1; dd_party_write(writer, &next);
    live_party_refresh();
    CHECK(G.heroes_shown == UINT32_MAX && G.live_party_ids[0] == 7 && G.live_party_ids[3] == 1);
    writer->sequence |= 1;
    live_party_refresh(); CHECK(G.live_party_count == -1 && !G.live_party_ids[0]);
    writer->sequence++;
    next.pid++; dd_party_write(writer, &next);
    live_party_refresh(); CHECK(G.live_party_count == -1); /* Another game's report is never accepted. */
    next.pid--; next.updated = 1001; dd_party_write(writer, &next);
    live_party_refresh(); CHECK(G.live_party_count == -1);
    next.updated = 1000; next.ids[3] = next.ids[0]; dd_party_write(writer, &next);
    live_party_refresh(); CHECK(G.live_party_count == -1);
    next.count = 0; next.mode = DD_PARTY_TOWN; memset(next.ids, 0, sizeof next.ids); dd_party_write(writer, &next);
    live_party_refresh(); CHECK(G.live_party_count == 0 && !G.live_party_ids[0]);
    G.game = NULL; live_party_refresh();
    CHECK(G.live_party_count == -1 && !g_live_party_view && !g_live_party_mapping);
    UnmapViewOfFile(writer); CloseHandle(mapping);
    printf("Launcher live party IPC: current game/read-only view, rank change without owner revision, torn/foreign/future/duplicate/empty/closed states: %s (%d failures)\n",
           failures ? "FAILED" : "PASS", failures);
    return failures ? 1 : 0;
}
