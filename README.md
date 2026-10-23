# petari-pc

An unofficial, experimental **native x86-64 Windows build** of the
[SMGCommunity/Petari](https://github.com/SMGCommunity/Petari) decompilation, using
[Aurora](https://github.com/encounter/aurora) (GX on WebGPU/Dawn) in place of the Wii hardware.

*Français plus bas.*

> [!IMPORTANT]
> - **No game data is included or distributed here.** You need your own copy of the original disc
>   (US version, **RMGE01**). No compiled executable will be published either: the build embeds
>   data tables read from your own `main.dol`.
> - **This port was written with heavy help from an AI assistant (Anthropic's Claude).** Petari does
>   not accept AI-generated code, so none of this is meant for, or will be proposed to, Petari.
>   Please do **not** report issues of this port to the Petari team.
> - Not affiliated with Petari, Luma's Workshop / AstroCore, or Nintendo. If a rights holder asks
>   for something to be removed, it will be removed.

## What it does

- The decompiled C++ is compiled for x86-64; the Wii SDK (GX, VI, PAD, WPAD/KPAD, NAND, AX, DVD...)
  is replaced by Aurora plus PC backends in `pc/`. PC changes are guarded with `#if TARGET_PC`, so
  the decomp build still rebuilds the original `main.dol` byte for byte.
- Reads the disc image directly (`.rvz`, `.iso`, `.wbfs`, `.ciso`).
- Xbox / SDL3 gamepads (the left stick is the Nunchuk, the right stick moves the star pointer,
  B/X spin), mouse pointer; keyboard and mouse as a fallback (ZQSD on AZERTY / WASD on QWERTY
  to move, Space to jump, E or middle click to spin, Shift to crouch, Escape to pause; the full list
  is in the F1 menu); two-player co-star mode with a second gamepad; rumble.
- Settings menu (F1, or View + Menu on a gamepad) and `%APPDATA%\Super Mario Galaxy\display.ini`:
  window / fullscreen, supersampling, MSAA, anisotropic filtering, vsync, D3D12 / Vulkan / D3D11,
  game language (English, French, Spanish from the US disc; French by default, `langue = en`
  for English).
- Optional enhancements, off or close to the original by default: widescreen and any window aspect,
  draw distance, more particles, per-pixel lighting, FXAA, sharpening, vibrance, ambient occlusion
  (GTAO), sun rays. Dolphin-format HD texture packs load from `textures/` next to the executable.
- Saves use the Wii save format (`nand/` under the same folder).

Status: the whole game boots, loads every galaxy and is playable, but it is a work in progress and
has only been tested on one Windows PC. Expect bugs.

## Building (Windows)

Requirements: Visual Studio 2022 or later with the C++ workload (its CMake and Ninja are fine),
Git, Python 3. Other dependencies (SDL3, Dawn, nod, libpng...) are fetched by CMake.

```
git clone --recursive https://github.com/yacineolivier-maker/petari-pc.git
cd petari-pc
```

1. Put your disc's `main.dol` at `orig/RMGE01/sys/main.dol` (Dolphin: right click the game >
   Properties > Filesystem > right click the disc > *Extract System Data*, then copy `sys/`).
   Expected SHA-1: `9a71008ae1ee9010e267fa67d1f0b0d4f0e895dd`.
2. From a *x64 Native Tools Command Prompt*:
   ```
   cmake -S . -B build/pc -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
   cmake --build build/pc
   ```
   The configure step applies the Aurora patches in `pc/patches/aurora`.
3. Copy `build/pc/smgpc.exe` and the `.dll` files next to it into a folder, and put your disc image
   in a `Rom` folder beside (or above) it, or pass its path as the first argument:
   `smgpc.exe "D:\Games\disc.rvz"`.

The original decomp build (`python configure.py --version RMGE01` then `ninja`) is unchanged; see
[docs/README_decomp.md](docs/README_decomp.md) and [docs/getting_started.md](docs/getting_started.md).

## Credits and licenses

- [Petari](https://github.com/SMGCommunity/Petari) and its contributors: the decompilation this
  is built on (CC0). Their original README is in [docs/README_decomp.md](docs/README_decomp.md).
- [Aurora](https://github.com/encounter/aurora) by Luke Street and contributors (MIT); local
  patches in `pc/patches/aurora`.
- Ideas and code patterns from the Twilight Princess PC port Dusklight and from community Wind Waker
  ports. The ambient occlusion is a port of Bevy's / XeGTAO's GTAO (MIT / Apache-2.0, see
  `pc/src/fx/licenses`).
- The PC port code added here is released under CC0 like Petari, except where a file says otherwise.

---

## Français

Portage PC natif (Windows x86-64), **non officiel et expérimental**, de la décompilation
[Petari](https://github.com/SMGCommunity/Petari), avec [Aurora](https://github.com/encounter/aurora)
à la place du matériel Wii.

> [!IMPORTANT]
> - **Aucune donnée du jeu n'est fournie.** Il faut votre propre copie du disque original (version
>   US, **RMGE01**, qui contient le français). Aucun exécutable compilé ne sera publié : la
>   compilation intègre des tables lues dans votre propre `main.dol`.
> - **Ce portage a été écrit avec l'aide importante d'une IA (Claude, d'Anthropic).** Petari
>   n'accepte pas le code généré par IA : rien de ceci ne lui est destiné. Merci de ne **pas**
>   signaler les problèmes de ce portage à l'équipe Petari.
> - Sans lien avec Petari, Luma's Workshop / AstroCore ou Nintendo. Toute demande de retrait d'un
>   ayant droit sera respectée.

Fonctions : lecture directe de l'image disque (`.rvz`...), manettes Xbox (stick gauche = Nunchuk,
stick droit = pointeur étoile, B/X = toupie), souris, clavier + souris en dépannage (ZQSD pour se
déplacer, Espace pour sauter, E ou clic molette pour la toupie, liste complète dans le menu F1), mode deux joueurs, vibrations, menu de
réglages (F1), écran large, suréchantillonnage, améliorations graphiques optionnelles, packs de
textures HD au format Dolphin, langue du jeu (français par défaut possible).

Compilation : voir la section *Building* ci-dessus (Visual Studio 2022+, Git, Python 3 ; placer
`main.dol` dans `orig/RMGE01/sys/`, puis `cmake` ; mettre l'image disque dans un dossier `Rom` à
côté de l'exécutable).
