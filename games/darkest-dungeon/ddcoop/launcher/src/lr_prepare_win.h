#ifndef LR_PREPARE_WIN_H
#define LR_PREPARE_WIN_H
#include <wchar.h>
/* Child process only: --prepare-save <game bin> <snapshot> <recovery output> <report>.
 * The parent sets SteamAppId=262060. No game is running while this imports the snapshot's one slot. */
/* Exit code: the slot could not be read through Steam yet; nothing was written. */
#define LR_PREPARE_UNREAD 3
int lr_prepare_win_main(int argc, wchar_t **argv);
#endif
