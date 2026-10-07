# port/linux/game

Game code the port adds on top of Halo's own (`source/`). Most of
DamnationCE's own code is here. Read the root AGENTS.md first.

## Files by feature

### Network game (read `../NETCODE.md` first)

| File | What it does |
|---|---|
| `network_distributed.c/.h` | The message layer. Every message kind, sending (batched per tick, unreliable or reliable), receiving, the stale-message check, and the per-tick host and client hooks. |
| `network_objects.c` | Units, vehicles, weapons and equipment are the same objects at the same index on every machine. Creation, deletion, positions, inventories. |
| `network_damage.c` | The host decides damage and deaths; clients show hits. |
| `network_actors.c` | AI sync. Only the host runs the AI; clients drive the actors' units from the host's control. |
| `network_coop.c` | Campaign co-op: the host's cinematics, camera, fades, script sounds, devices (doors, elevators, switches) and scripted objects, on every client. |
| `coop_spectate.c` | A dead co-op player watches a living teammate until they respawn. |

Adding a message kind takes five steps, all in `network_distributed.c/.h`
except the first:

1. The entry struct, with send and handle functions, in your file.
2. The kind in the enum in `network_distributed.h` (64 and up).
3. Its entry size in `network_distributed_handle_message`.
4. Its handler in the switch below that.
5. If it is unreliable, its case in `distributed_message_stale`.

### Menus and screens

| File | What it does |
|---|---|
| `menu_tags.c` | Builds the XML menus (`port/assets/menus`) into the game's widget tags. |
| `menu_functions.c` | Event handlers the XML menus can call: hosting, Server Setup, the lobby. |
| `overlay_screens.c/.h` | Helpers shared by the screens drawn over the menus: key repeat, buttons, map pictures and names, the list screens' palette, fitted text. |
| `map_screen.c` | The Glassed map picker used when hosting. Cooperative or PvP, then Campaign (stock or custom) or Vanilla/Custom maps. Backing out of it returns to Online Games if that opened it. Vanilla uses the stock map list. |
| `browser_screen.c` | The Online Games server browser: a list with sortable columns and a details pane. |
| `lobby_screen.c` | The Glassed pregame lobby, drawn in the browser's style over the lobby's invisible widgets. Vanilla uses the stock lobby. |

The overlay screens are drawn with `../src/ui_overlay.h`, in a 640x480
layout. The map picker and the lobby are Glassed only; Vanilla keeps the
stock screens. The browser draws in both themes (`overlay_palette_current`).
The lobby's XML comes in two forms (`tools/port_settings.py`, `_lobby`): the
stock one in `ce/`, and the overlay's invisible one in the Glassed layer
(`tools/shell_skin.py`).

The browser and the map picker take over the menus' input and drawing
while open (`ui_widget.c` checks `*_screen_active`). The lobby doesn't: its
widgets (`tools/port_settings.py`, `_lobby`) still run it, update it and
take the focus and the mouse, but have no pictures and clear text, and
`lobby_screen.c` draws over them. Their places must match the constants at
the top of `lobby_screen.c`.

### Custom Edition maps

| File | What it does |
|---|---|
| `cache_file_formats.c/.h` | Reads Custom Edition cache files. See `docs/custom_edition_caches.md`. |
| `custom_edition_cache.c/.h` | Loads a CE cache into this build's tag layout. |
| `custom_edition_bitmaps.c`, `_geometry.c`, `_objects.c`, `_scripts.c`, `_sounds.c` | Converts each kind of CE data. |
| `custom_edition_maps.c/.h` | Lists CE maps (multiplayer and campaign) for the menus, with names and pictures. |

Display indices (`custom_edition_maps.h`) say what a menu entry is:

| Range | Meaning |
|---|---|
| below 0x1000 | Xbox multiplayer level |
| 0x3000 + n | stock campaign level n |
| 0x4000 + n | CE multiplayer map |
| 0x5000 + n | CE campaign map |

### Other

`game_list_claims.c` (the browser's confirmed players), `hud_hires_tags.c`
(the high-res HUD), `bmp_files.c` (BMP reading), and `stb_vorbis.c`
(third party, don't edit).

## Co-op in one paragraph

A network game on a campaign map with no game engine is co-op. Only the host
runs the scripts, the AI, spawning and the checkpoint logic (`game.c`,
`players.c`, `main.c`, all marked `port:`). Everything those decide that a
client must see goes through `network_coop.c` or `network_actors.c`. On a
client, `network_coop_devices_remote()` makes devices obey only the host.

Halo's own code reports script effects through `network_coop.h`: each
`network_coop_note_` call queues an event on the host and does nothing
elsewhere, so a client can call the same engine function to apply what it
receives. When you add a scripted effect, ask whether a client would see it.
If not, add a note call where the engine does it, and an event kind in
`network_coop.c`.

## Checking a change

You can't run a network game from a test, so reason it through and read
the logs (`debug.txt`). Then:

- build Windows, and compile your files with warnings on (root AGENTS.md);
- push and let CI build all four platforms.
