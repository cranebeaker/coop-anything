/* Unit tests for src/dd_addr.c. Build and run on Linux:
 *   gcc -std=c11 -Wall -Wextra -Isrc -o build/test_addr tests/test_addr.c src/dd_addr.c && build/test_addr [Darkest.exe]
 * With a Darkest.exe argument it also resolves the real table against the game's .text section. */
#include "dd_addr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(cond, ...)                                    \
    do {                                                    \
        if (!(cond)) {                                      \
            failures++;                                     \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);     \
            printf(__VA_ARGS__);                            \
            printf("\n");                                   \
        }                                                   \
    } while (0)

static void fill_pattern(uint8_t *p, uint8_t seed) {
    for (int i = 0; i < DD_PATTERN_LEN; i++) p[i] = (uint8_t)(seed + i * 7);
}

static void test_synthetic(void) {
    enum { SIZE = 4096 };
    static uint8_t code[SIZE];
    const uint64_t code_va = 0x140001000ULL;
    memset(code, 0xcc, SIZE);

    dd_addr t[4] = {{"exact", code_va + 0x100, {0}, 0, 0},
                    {"moved", code_va + 0x200, {0}, 0, 0},
                    {"missing", code_va + 0x300, {0}, 0, 0},
                    {"twice", code_va + 0x400, {0}, 0, 0}};
    fill_pattern(t[0].pattern, 1);
    fill_pattern(t[1].pattern, 2);
    fill_pattern(t[2].pattern, 3);
    fill_pattern(t[3].pattern, 4);
    memcpy(code + 0x100, t[0].pattern, DD_PATTERN_LEN); /* where expected */
    memcpy(code + 0x850, t[1].pattern, DD_PATTERN_LEN); /* moved by an update */
    memcpy(code + 0x900, t[3].pattern, DD_PATTERN_LEN); /* present twice, never at its address */
    memcpy(code + 0xa00, t[3].pattern, DD_PATTERN_LEN);

    size_t ok = dd_addr_resolve(t, 4, code, SIZE, code_va);
    CHECK(ok == 2, "expected 2 resolved, got %zu", ok);
    CHECK(t[0].status == DD_ADDR_EXACT && t[0].resolved_va == code_va + 0x100, "exact entry");
    CHECK(t[1].status == DD_ADDR_RELOCATED && t[1].resolved_va == code_va + 0x850, "relocated entry");
    CHECK(t[2].status == DD_ADDR_MISSING && t[2].resolved_va == 0, "missing entry");
    CHECK(t[3].status == DD_ADDR_AMBIGUOUS && t[3].resolved_va == 0, "ambiguous entry must not guess");
    CHECK(dd_addr_find(t, 4, "moved") == &t[1], "find resolved entry");
    CHECK(dd_addr_find(t, 4, "twice") == NULL, "find must not return an unresolved entry");
    CHECK(dd_addr_find(t, 4, "nope") == NULL, "find unknown name");

    /* An address past the end of the buffer must not be read. */
    dd_addr edge = {"edge", code_va + SIZE - 4, {0}, 0, 0};
    fill_pattern(edge.pattern, 9);
    dd_addr_resolve(&edge, 1, code, SIZE, code_va);
    CHECK(edge.status == DD_ADDR_MISSING, "edge entry");
}

/* Reads the .text section of a PE file into memory. */
static uint8_t *load_text(const char *path, size_t *size, uint64_t *va) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *file = malloc((size_t)len);
    if (!file || fread(file, 1, (size_t)len, f) != (size_t)len) {
        fclose(f);
        free(file);
        return NULL;
    }
    fclose(f);
    uint32_t pe;
    uint16_t nsec, optsize;
    memcpy(&pe, file + 0x3c, 4);
    memcpy(&nsec, file + pe + 6, 2);
    memcpy(&optsize, file + pe + 20, 2);
    uint8_t *text = NULL;
    for (int i = 0; i < nsec; i++) {
        const uint8_t *sh = file + pe + 24 + optsize + i * 40;
        if (memcmp(sh, ".text", 5) != 0) continue;
        uint32_t vsize, vaddr, rsize, raw;
        memcpy(&vsize, sh + 8, 4);
        memcpy(&vaddr, sh + 12, 4);
        memcpy(&rsize, sh + 16, 4);
        memcpy(&raw, sh + 20, 4);
        *size = rsize < vsize ? rsize : vsize;
        *va = DD_IMAGE_BASE + vaddr;
        text = malloc(*size);
        if (text) memcpy(text, file + raw, *size);
    }
    free(file);
    return text;
}

static void test_real_exe(const char *path) {
    size_t size = 0;
    uint64_t va = 0;
    uint8_t *text = load_text(path, &size, &va);
    CHECK(text != NULL, "could not read .text from %s", path);
    if (!text) return;

    size_t ok = dd_addr_resolve(dd_addresses, dd_address_count, text, size, va);
    CHECK(ok == dd_address_count, "real exe: %zu of %zu resolved", ok, dd_address_count);
    for (size_t k = 0; k < dd_address_count; k++)
        CHECK(dd_addresses[k].status == DD_ADDR_EXACT, "%s is %s, expected exact", dd_addresses[k].name,
              dd_addr_status_name(dd_addresses[k].status));

    /* Simulated game update: pretend every function moved (wrong expected address), so each one has to be
     * found again by its bytes alone. Reports how many are unique enough to relocate. */
    dd_addr moved[64];
    size_t n = dd_address_count < 64 ? dd_address_count : 64;
    memcpy(moved, dd_addresses, n * sizeof *moved);
    for (size_t k = 0; k < n; k++) moved[k].va = 0x100; /* outside .text */
    size_t relocated = dd_addr_resolve(moved, n, text, size, va);
    printf("simulated update: %zu of %zu functions relocatable by their %d bytes\n", relocated, n, DD_PATTERN_LEN);
    for (size_t k = 0; k < n; k++)
        if (moved[k].status != DD_ADDR_RELOCATED)
            printf("  not relocatable: %s (%s)\n", moved[k].name, dd_addr_status_name(moved[k].status));
        else
            CHECK(moved[k].resolved_va == dd_addresses[k].va, "%s relocated to the wrong place", moved[k].name);
    free(text);
}

int main(int argc, char **argv) {
    test_synthetic();
    if (argc > 1) test_real_exe(argv[1]);
    printf("%s (%d failure%s)\n", failures ? "FAILED" : "OK", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
