# tools

Python scripts for building, generating assets, and testing. Each script
opens with a docstring saying what it does and how to run it. Read the root
AGENTS.md first.

## Build

| Script | What it does |
|---|---|
| `../configure.py` | Writes `build.ninja`. Rerun it after adding a source file. |
| `windows_build.py`, `linux_build.py`, `macos_build.py`, `android_build.py` | The ninja rules for each platform. |
| `ci_build.py` | Builds one platform the way CI does. |
| `lp64_rewrite.py` | Makes `long` 32 bits for the 64-bit macOS build. |
| `version.py` | The version number every build reports, from `../VERSION`. |
| `embed_assets.py` | Compiles the HUD, menu and font assets into the game. |
| `port_neutrality_check.py` | Checks that a change leaves the 32-bit builds' code unchanged. |

Android only: `android_*.py`.

## Menus

The menus are data in `port/assets/menus`.

| Script | What it does |
|---|---|
| `ce_menus.py` | Generates `port/assets/menus/ce` from the PC version's menus. Don't hand-edit its output. |
| `shell_skin.py` | Writes the two themes (Glassed and Vanilla) as layers over those screens. Change a theme here, not in the generated XML. |
| `shell_art.py` | Draws the left-hand menu pictures. |
| `port_settings.py` | The port's own settings screens. |
| `menu_preview.py` | Renders a menu screen to a PNG, so you can check a layout without running the game. |
| `title_assets.py`, `title_font.py`, `hud_assets.py` | Generate titles, the title font and high-res HUD textures. |
| `app_icon.py` | Makes the README logo and the macOS and Android icons from `port/assets/logo.png` (then run `android_icon.py`). |

After changing `shell_skin.py` or `shell_art.py`, rerun it and commit the
files it writes.

## Tests

```
python -m pytest tools/test_cache_file_formats.py tools/test_bmp_files.py tools/test_custom_edition_tag_footprints.py tools/test_linux_port.py
```

`system_link_bots.py` (and `rally/`) joins stand-in machines to a host, for
testing large sessions.

## Research

`custom_edition_tag_footprints.py`, `pdb200_*.py` and `xdk_headers.py` were
used to work out file formats and types. Nothing in the build runs them.
