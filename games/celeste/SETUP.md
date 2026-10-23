# Celeste 2-player co-op: setup guide

Goal: you and one friend play the **Partnership** co-op mini campaign together, on the
**Steam (PC)** version of Celeste.

You both follow every step. Co-op does not work if only one of you has the mods.

> This is about the vanilla Steam game plus mods. Nothing here modifies or bypasses
> Steam, and it does not touch anyone else's game. All the mods are free and publicly
> downloadable.

## What you need

- Celeste on Steam, on Windows. (Mods are Windows-only in practice; Everest supports
  Linux/macOS but this guide is written for Windows.)
- ~1 GB free disk space for Everest plus the mods.
- Both players on the same CelesteNet server and the same channel.

## Step 1: Install Everest

Everest is the mod loader. There are two ways to install it. **Olympus** is the friendly
GUI and is what most players use.

### Option A: Olympus (recommended for a normal install)

1. Go to the Olympus releases: <https://github.com/EverestAPI/Olympus/releases> and
   download the newest installer for Windows.
2. Run it. It finds your Celeste install and installs Everest.
3. Later, Olympus is also where you install and update mods (Step 2).

Official site and help: <https://everestapi.github.io/>.

### Option B: Manual (MiniInstaller)

Use this if you keep Celeste in a separate folder (for example a copy you do not want to
disturb), which is how the test install in this repo was made.

1. Download the Everest release marked `stable` from
   <https://github.com/EverestAPI/Everest/releases> (for example `everest-main.zip`).
2. Extract it **into your Celeste folder** (the one containing `Celeste.exe`). It adds
   `MiniInstaller-win64.exe` and its libraries. Make a backup copy of the folder first if
   you care about the vanilla files.
3. Run `MiniInstaller-win64.exe`. It patches the game and creates `Mods/`. Everest keeps
   the pre-mod files in `orig/`.
4. Launch `Celeste.exe` once to confirm Everest starts.

## Step 2: Install the mods

You need **five** mods. Install all five, on both machines.

| Mod | Download | Why |
|---|---|---|
| CelesteNet.Client | <https://gamebanana.com/mods/53695> | the multiplayer layer |
| Co-op Helper | <https://gamebanana.com/mods/453706> | synced entities co-op maps use |
| MaxHelpingHand | <https://gamebanana.com/mods/53687> | required by Partnership |
| LunaticHelper | <https://gamebanana.com/mods/53692> | required by Partnership |
| Partnership | <https://gamebanana.com/mods/150725> | the co-op campaign itself |

> **Gotcha:** MaxHelpingHand's GameBanana page is titled **"Maddie's Helping Hand"**. It
> was renamed years ago but the mod's internal name was kept, so if a map says it needs
> `MaxHelpingHand`, this is the mod. Searching GameBanana for "MaxHelpingHand" does not
> obviously find it.

### With Olympus

Open Olympus, search each mod by name, install it. Olympus pulls dependencies for you;
if it offers to install MaxHelpingHand or LunaticHelper as a dependency of Partnership,
accept.

### Manually

1. For each mod, download the release ZIP from the page above.
2. Create a folder under `Mods/` named after the mod, and extract the ZIP **into it**, so
   that `<Celeste>/Mods/<ModName>/everest.yaml` exists.
   For example `Mods/Partnership/everest.yaml`, `Mods/CelesteNet.Client/everest.yaml`.
3. The folder name does not matter to Everest; the `everest.yaml` inside does.
4. If a ZIP already contains a top-level folder, extract that folder into `Mods/` rather
   than nesting an extra level.

Do not put the raw ZIPs in `Mods/`; extract them.

## Step 3: Connect with CelesteNet

1. Launch Celeste through Steam. You should see an Everest / CelesteNet entry in the main
   menu (CelesteNet adds its own settings page and in-game overlay).
2. Open CelesteNet's settings and set a **display name** (what your partner will see).
3. Pick a **server**. The public CelesteNet server is the default. Both players must use
   the same server. If you want your own, CelesteNet's server is self-hostable (see
   <https://github.com/0x0ade/CelesteNet>).
4. Connect. You should see yourself joined in the CelesteNet overlay.

## Step 4: Make a private channel

A channel is a room on the server. Without a private one, you share the map with everyone
else on that server.

1. In the CelesteNet overlay, use the channel controls to create a channel (for example
   `partner-<something-only-you-two-know>`).
2. **Set a password** on it, or keep the name unguessable.
3. Your partner joins by entering the same channel name and password.
4. Confirm you can see each other's name in the overlay before starting the map.

A community video walkthrough of CelesteNet and private channels is linked from the
Partnership mod page.

## Step 5: Play Partnership

1. From the main menu, open the **chapter select** and find **Partnership** in the mod
   list.
2. Start the first chapter. It is designed for exactly two players; it is not meant to be
   playable solo.
3. Progress is tracked in the co-op map, separately from your vanilla save.

## Troubleshooting

- **A mod does not load / "missing dependency" on launch.** Partnership needs
  MaxHelpingHand and LunaticHelper. Confirm both are present as folders with their own
  `everest.yaml` under `Mods/`.
- **You cannot see each other.** Confirm same server, same channel, and that both of you
  actually connected (CelesteNet overlay shows the player list). Restart Celeste after
  changing the server.
- **Random strangers appear in your map.** You are in a public channel; make or join a
  private one.
- **Only one of you installed the mods.** Both machines need the full list. A map will
  fail to load for whoever is missing a dependency.
- **Everest log for debugging:** `log.txt` in the Celeste folder (Everest does not call it
  `everest-log.txt`). It lists every mod that loads and any errors.
- **You copied Celeste to a separate folder and Steam keeps opening your normal install.**
  A copied install needs a file named `steam_appid.txt` containing `504230` next to
  `Celeste.exe`; without it the launch is handed to the Steam install. A normal Steam
  install does not need this.
- **Old download links.** Some guides and even CelesteNet's own README link to
  `gamebanana.com/gamefiles/<id>`. Those redirect to the right page today, but if one
  lands somewhere odd, use the mod pages in the table above instead.

## Note on the test install used for this write-up

To avoid touching the owner's Steam install, testing was done on a **separate copy** of
the game under `local/` (git-ignored), with Everest installed there. Your own setup can be
either a copy or your normal install; if you want to keep vanilla intact, copy the Celeste
folder somewhere else and install Everest into the copy.
