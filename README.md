<p align="center"><img src="docs/icon-160.png" width="120" alt=""></p>

<h1 align="center">DamnationCE</h1>

<p align="center"><b>A Halo CE Client forked from OpenCE.</b></p>

<p align="center">
<a href="https://github.com/xshxdex98/DamnationCE/releases/latest">Download</a> ·
<a href="https://discord.gg/5vSnrK35fz">Project Discord</a> ·
</p>

> **Plays with [OpenCE](https://github.com/OpenCommunityEdition/OpenCE) build-76 through build-102 (network version 11)** and with ChupathingyCE, both ways.

DamnationCE is a fork of **OpenCE**
([OpenCommunityEdition/OpenCE](https://github.com/OpenCommunityEdition/OpenCE)),
the port of the Halo: Combat Evolved decompilation to modern computers. It
follows OpenCE's builds closely and adds its own menus, Online Co-oP Campaign, Custom Edition map
support and synced AI on top.

<img width="1219" height="708" alt="image" src="https://github.com/user-attachments/assets/6375f1d0-2593-4550-a0dd-3a77160abc42" />


## What it adds

**Menus**
- Menu themes, switched from the main menu's **MENUS** button: **Glassed**, a
  minimal look of plain text over the scene and clear glass behind what is
  chosen, and **Vanilla**, the PC menus as they were
- Left-hand main menu, with Campaign, Multiplayer and Menus columns
- A map picker: **Vanilla** (the Xbox's 13 levels and Halo PC's 6) and
  **Custom** categories, shown as a list with the map's picture beside it or as
  a grid of cards (**Y** switches); used wherever a map is chosen
- Its own Online Games screen: a card for each game, the chosen game's details
  beside the list
- The menu scene keeps running behind every screen

**Halo Custom Edition maps** (`docs/custom_edition_caches.md`)
- Custom Edition maps load and run, listed as CUSTOM SINGLEPLAYER (played
  alone or as network co-op) and CUSTOM MULTIPLAYER
- Their Ogg Vorbis sounds play (announcer, dialogue, music)
- Their scripts run; OpenSauce-only effect calls do nothing instead of
  refusing the map
- Their stock HUD is drawn with the port's high-res HUD where the layouts match
- Models of up to 64 bones
- Up to 1024 maps in the list, with names of up to 51 characters

**Multiplayer**
- AI synced to every player in a game (no "ghost" AI)

## I Don't Provide Game Copies.

DamnationCE doesn't include the game's maps, sounds or art. You need an Xbox
disc image (`.iso` or `.xiso`) of Halo: Combat Evolved. The first time it
starts, it asks for the image and copies the game's `maps` folder out of it.

### Custom Edition Map Support

Put these from a Halo Custom Edition install's `maps` folder into a
`custom_maps` folder beside the game's `maps` folder (the game's own maps stay
in `maps`):

- `bitmaps.map`, `sounds.map` and `loc.map`, which every Custom Edition map
  draws its stock art, sounds and text from
- the maps you want: Halo PC's own (`dangercanyon.map`, `deathisland.map`,
  `gephyrophobia.map`, `icefields.map`, `infinity.map`, `timberland.map`) and
  any custom ones

A map's picture is `<name>.bmp` beside it and its description `<name>.txt`.
To use an install where it is instead of copying, set `paths.custom_edition`
in `config.toml`.

## Online Multiplayer

| You want to | Do this |
| --- | --- |
| Join a game | **Multiplayer → Online Games**, pick a game from the list, press **A** |
| Host a game | **Multiplayer → Online Games → Create Game**, or host from LAN as usual |

The game stats are ChupathingyCE's service,
[halo.milenko.org](https://halo.milenko.org).

## Building

You need Python 3, [ninja](https://ninja-build.org/) and clang.

```sh
python3 configure.py
ninja            # the game for the computer you're on
```

| Target | Result | Instructions |
| --- | --- | --- |
| `ninja windows` | `build/windows/halo.exe` | [port/windows/README.md](port/windows/README.md) |
| `ninja linux` | `build/linux/halo` | [port/linux/README.md](port/linux/README.md) |
| `ninja macos` | `build/macos/DamnationCE.app` | [port/macos/README.md](port/macos/README.md) |
| `ninja android_apk` | the Android app | [port/android/README.md](port/android/README.md) |

`--release` makes a release build; `--pgo=off --lto=off` builds faster. The
menus are XML and pictures in `port/assets/menus`
([its README](port/assets/menus/README.md) explains the themes and the tools
that draw them).

## Credits

- The decompilation: [punpckhdq/halo](https://github.com/punpckhdq/halo) and
  [bnunu/halo-1](https://github.com/bnunu/halo-1), of the Xbox build 2342.
- The port: [OpenCE](https://github.com/OpenCommunityEdition/OpenCE) and its
  contributors, which this is a fork of.
- Stats:
  [ChupathingyCE](https://github.com/ChupathingyCE/chupathingyce), by
  [Milenko](https://github.com/MrMilenko) and contributors (CC0), used with
  their permission, and its service halo.milenko.org.
- Custom Edition map loading: [bnunu](https://github.com/bnunu/halo-ce-universal).
- Fonts: [Noto Sans](https://fonts.google.com/noto) (SIL OFL) and
  [Kenney's Input Prompts](https://kenney.nl/assets/input-prompts) (CC0).
- Libraries: SDL3, stb (and stb_vorbis), Mbed TLS, miniupnpc, KCP, tomlc17,
  musl's maths, expat and extract-xiso. Their licenses are beside them in
  `port/third_party`.

Halo is a trademark of Microsoft. DamnationCE is a fan project, not made or
endorsed by Microsoft, Bungie or 343 Industries, and includes none of the
game's content. The code is released under [CC0](LICENSE.md).
