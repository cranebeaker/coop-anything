/* The exact files DDCoop knows (SHA-256). The installer refuses anything else rather than guess.
 * Game: Darkest Dungeon on Steam, Windows x64. Bootstrap and runtime: Codex's v2 test kit (mods\DDCoop). */
#ifndef LR_HASHES_H
#define LR_HASHES_H

#define LR_HASH_GAME "f9d914ca1d7d9ad3ff4f151a46e4853e532ce1f5f109f700b0c1381a3f7e0d12"          /* Darkest.exe build28385 */
#define LR_HASH_SDL2_ORIGINAL "7ba3aa8b03e43690e2b1d4696a3beb90611224a9fee77ca2d4a19295bb4961dd" /* SDL2.dll from Steam */
#define LR_HASH_BOOTSTRAP "ad3c25374e9fb23ff5e388cde052a7359abad1cda9bf5afb6aad399e25e82589"     /* test12 SDL input + GL swap bootstrap */
#define LR_HASH_BOOTSTRAP_TEST11 "e7f1c4a7aa8a96144b17497fd774aefac71af3625a69e3e1f2e57be59c24a627" /* verified predecessor, upgrade only with intact original */
#define LR_HASH_TOWN_PROBE_V12 "e3d4796686656c496715ab0d9917d4413b12b34ef8eef3cb6929b9c27c7dd8fc" /* read-only probe */
#define LR_HASH_RUNTIME "25b70c38af1dbbd175e4b6538fa3565e20032fdea11ce5c5926f95b26f739589" /* test55: live party IDs for launcher/F9; protocol25 unchanged */

#endif
