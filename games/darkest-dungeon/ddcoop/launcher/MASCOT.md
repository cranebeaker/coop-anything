# Optional animated launcher mascot

Player test58/duo41 builds embed the user-provided GIF as resource201. The original
375632-byte GIF is512x512, eight frames,120ms per frame. It is not a game-process
overlay and does not handle user input. The launcher shows it in a150x150 child
control beside the friends/code controls, with Copy my code and the footer moved
below to avoid overlap. The checks remain scrollable and the hero table unchanged.

lr_mascot.c decodes the embedded resource through the Windows GDI+ flat API. It
retains the IStream until image disposal, respects frame delays, preserves aspect
ratio/transparency, uses double-buffered painting and does not redraw hidden or
minimized windows. Images/frames/property lengths are bounded. Image, stream,
timer and GDI+ token are released with the child window. Missing or invalid optional
artwork simply omits the mascot. No extra DLL or loose GIF is needed by players.

Build with DDC_MASCOT_GIF pointing to your local GIF. tools/gen_mascot_resource.py
generates build/win/ddcoop-mascot.rc; without that variable, the resource is empty.
The launcher build now links Windows gdiplus.lib. The artwork is deliberately
excluded from published source in accordance with the project's asset rule;
developers may supply their own image. Never add an entire game asset folder.

Tests use an original synthetic two-frame1x1 GIF and a hidden child window. Optional
test_lr_mascot.exe <local GIF> also checks the supplied image's eight frames,
120ms timing and different rendered frames. No Steam/game is started by the test.

API reference: [Microsoft Image::SelectActiveFrame](https://learn.microsoft.com/en-us/windows/win32/api/gdiplusheaders/nf-gdiplusheaders-image-selectactiveframe).
Flat ABI declarations follow the installed Windows SDK gdiplusflat.h,
gdiplusinit.h and gdiplusimaging.h. No third-party graphics package is required.

Written by Codex,9 Nov2026. Launcher change explicitly requested by the owner.
