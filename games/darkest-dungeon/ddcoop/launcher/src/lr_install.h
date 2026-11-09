/* Install, repair, uninstall and start (Windows): DDCoop.exe --install | --repair | --uninstall | --start.
 * The .bat files in the zip only call these, so every check and message lives in one tested place. */
#ifndef LR_INSTALL_H
#define LR_INSTALL_H

#include <wchar.h>

/* Returns the process exit code. mode: L"--install", L"--repair", L"--uninstall", L"--start". */
int lr_install_main(const wchar_t *mode);

/* SHA-256 of a file as lowercase hex; 1 on success. */
int lr_sha256_file(const char *path, char hex[65]);
/* Repair a known Steam original or verified predecessor bootstrap, using a pinned recovery copy.
 * Upgrading a predecessor always requires an intact SDL2_orig.dll; never back up a proxy as the original.
 * 1 = repaired, 0 = no repair, -1 = known repair could not be completed. Never runs the game. */
int lr_repair_bootstrap(const char *game, const char *moddir);

#endif
