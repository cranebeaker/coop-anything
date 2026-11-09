/* Upgrade/repair only inside build/win/bootstrap-tests. No installer, registry, Steam or saves. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lr_files.h"
#include "lr_install.h"
static int bad;
#define CHECK(x)                                                                                             \
    do {                                                                                                     \
        if (!(x)) {                                                                                          \
            printf("FAIL %d: %s\n", __LINE__, #x);                                                           \
            bad++;                                                                                           \
        }                                                                                                    \
    } while (0)
static int same(const char *a, const char *b) {
    uint8_t *aa = NULL, *bb = NULL;
    size_t an = 0, bn = 0;
    int ok = lr_read_file(a, 4 * 1024 * 1024, &aa, &an) && lr_read_file(b, 4 * 1024 * 1024, &bb, &bn) &&
             an == bn && !memcmp(aa, bb, an);
    free(aa);
    free(bb);
    return ok;
}
int main(int argc, char **argv) {
    if (argc != 4) return 2;
    char cwd[LR_PATH_MAX], root[LR_PATH_MAX], bin[LR_PATH_MAX], mod[LR_PATH_MAX], sdl[LR_PATH_MAX],
        orig[LR_PATH_MAX], seed[LR_PATH_MAX];
    CHECK(GetCurrentDirectoryA(sizeof cwd, cwd));
    lr_join(root, sizeof root, cwd, "build\\win\\bootstrap-tests");
    lr_join(bin, sizeof bin, root, "_windows\\win64");
    lr_join(mod, sizeof mod, root, "mods\\DDCoop");
    CHECK(lr_make_dirs(bin) && lr_make_dirs(mod));
    lr_join(sdl, sizeof sdl, bin, "SDL2.dll");
    lr_join(orig, sizeof orig, bin, "SDL2_orig.dll");
    lr_join(seed, sizeof seed, mod, "SDL2-bootstrap.dll");
    CHECK(CopyFileA(argv[1], seed, FALSE) && CopyFileA(argv[2], sdl, FALSE) &&
          CopyFileA(argv[3], orig, FALSE));
    CHECK(lr_repair_bootstrap(root, mod) == 1 && same(seed, sdl) && same(orig, argv[3]));
    CHECK(CopyFileA(argv[2], sdl, FALSE) && DeleteFileA(orig));
    CHECK(lr_repair_bootstrap(root, mod) == -1 && same(sdl, argv[2]) && !lr_file_exists(orig));
    CHECK(lr_write_file(orig, (const uint8_t *)"unknown", 7));
    CHECK(lr_repair_bootstrap(root, mod) == -1 && same(sdl, argv[2]));
    CHECK(DeleteFileA(orig) && CopyFileA(argv[3], sdl, FALSE));
    CHECK(lr_repair_bootstrap(root, mod) == 1 && same(seed, sdl) && same(orig, argv[3]));
    CHECK(lr_write_file(sdl, (const uint8_t *)"unknown", 7));
    CHECK(lr_repair_bootstrap(root, mod) == 0 && !same(sdl, seed));
    CHECK(CopyFileA(argv[2], sdl, FALSE) && lr_write_file(seed, (const uint8_t *)"modified seed", 13));
    CHECK(lr_repair_bootstrap(root, mod) == 0 && same(sdl, argv[2]));
    printf("Bootstrap repair fixture: verified predecessor upgrade, intact original, missing/bad original, "
           "original recovery and unknown/modified files: %s (%d failures)\n",
           bad ? "FAILED" : "OK", bad);
    return bad ? 1 : 0;
}
