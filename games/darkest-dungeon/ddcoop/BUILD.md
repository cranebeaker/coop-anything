# Build DDCoop (Windows x64)

Prerequisites: Visual Studio2022 C/C++ tools, Windows SDK and Python3. No GCC runtime
DLLs are needed. Use an x64 Native Tools Command Prompt at this directory.

The supported Steam Windows x64 executable is build28385, SHA256
f9d914ca1d7d9ad3ff4f151a46e4853e532ce1f5f109f700b0c1381a3f7e0d12.
All signature checks must match. Never relax pins to accept an unknown game update.
Use your legally installed game; its executable/assets/Steam DLL are not included.

    set "DD_GAME_EXE=<game dir>\_windows\win64\Darkest.exe"
    runtime\test-windows-msvc.cmd
    runtime\test-overlay-msvc.cmd
    runtime\build-windows-msvc.cmd

After a changed runtime build, compute its SHA256 and update LR_HASH_RUNTIME in
launcher/src/lr_hashes.h, then build/test the matching launcher:

    set "DDC_TEST_NOSAVE=0"
    launcher\build-windows-msvc.cmd
    launcher\test-windows-msvc.cmd

Close the game before launcher tests: the isolated preparation fixture intentionally
refuses while any Darkest process runs. Fixtures use fake Steam and disposable files.
The default launcher lets the host save progress; guests always run -nosave.

The player kit needs a separately built, verified SDL2 bootstrap. To build one:

    set "DD_SDL_ORIGINAL=<verified original SDL2.dll>"
    runtime\build-bootstrap-msvc.cmd
    python -I tools/check_forwarders.py build/win/bootstrap/SDL2.dll 845 SDL_PollEvent SDL_GL_SwapWindow

The existing bootstrap pin is ad3c25374e9fb23ff5e388cde052a7359abad1cda9bf5afb6aad399e25e82589.
A rebuilt bootstrap must be reviewed and pinned before packaging. Packaging accepts
a staging directory containing DDCoop.exe, SDL2.dll and DDCoop/ddcoop-runtime.dll:

    python -I tools/make_dropkit.py <staging dir> <output zip> <version>

For the optional two-window build, compile the runtime with DD_DUO, pin that separate
DLL, and build the launcher with DDC_VERSION set to the matching duo version. See
runtime/DUNGEON.md and the runtime build source list. No binary/game file is in git.
