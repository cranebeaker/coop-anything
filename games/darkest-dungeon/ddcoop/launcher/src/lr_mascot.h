#ifndef LR_MASCOT_H
#define LR_MASCOT_H
#include <windows.h>
/* Optional embedded GIF; no external file, input handler or game-process work. */
HWND lr_mascot_create(HINSTANCE instance,HWND parent,int x,int y,int width,int height);
#endif
