# AGENTS.md

Notes for anyone (person or coding agent) changing DamnationCE. Read this,
then the AGENTS.md of the directory you are working in.

## What this is

DamnationCE is a Halo: Combat Evolved client: a fork of OpenCE
(`cybersecurity/halo-ce-universal`, the `upstream` remote) with
ChupathingyCE's features (used with permission), the Glassed menu theme,
Custom Edition map support, AI sync and networked campaign co-op.

## Layout

| Path | What it is | How to change it |
|---|---|---|
| `source/` | Halo's own code, decompiled to match the Xbox build | Keep changes small, and mark each with a `port:` comment, so upstream merges stay clean. |
| `port/linux/game/` | Game code the port adds: netcode, menus, Custom Edition maps | Our code lives here. See its AGENTS.md. |
| `port/linux/src/` | The platform layer: SDL, GL, audio, files, the overlay UI | |
| `port/windows`, `port/macos`, `port/android` | Per-platform glue and build notes | |
| `port/assets/menus/` | Menu definitions (data) | Edit these, plus `tools/shell_art.py` and `tools/shell_skin.py`. Never edit `port/assets/menus/ce`: `tools/ce_menus.py` generates it. |
| `tools/` | Build scripts, asset generators, tests | See its AGENTS.md. |
| `port/linux/NETCODE.md` | How the network game works | Read it before touching anything under `network_*`. |

## Building

You need Python 3, ninja and clang (LLVM). On Windows, put
`C:\Program Files\LLVM\bin` on `PATH`.

```
python configure.py
ninja windows        # build/windows/halo.exe
ninja linux          # build/linux/halo
ninja macos          # build/macos/DamnationCE.app
ninja android_apk
```

CI (`.github/workflows/build.yml`) builds all four. A change is not done
until all four are green.

- The macOS build is 64-bit. `tools/lp64_rewrite.py` makes `long` 32 bits
  there, so `long` fields are 4 bytes on every platform. Pointers are still
  8 bytes on macOS: never store one in a `long`. Xbox-address fields
  (`XPTR`) go through `xbox_pointer()`.
- macOS rejects implicit function declarations, and it stops at the first
  file that fails. Declare everything you call.
- The Windows build compiles with `-w`, which hides warnings. Before
  committing, compile the files you changed without `-w`, with
  `-Wall -Wextra`.

## Rules

- **The wire protocol is OpenCE's.** Don't change how games are joined, or
  any existing message. New co-op messages use kinds from 64 up
  (`network_distributed.h`). A build that doesn't know a kind drops it, so
  players on stock OpenCE can still join.
- **Multiplayer behaves as upstream's.** Co-op code checks that it is in a
  co-op game (`!game_engine_running()` on a network game) before doing
  anything.
- **The host is authoritative.** Clients apply what the host sends. They
  don't decide damage, spawns, pickups, scripts or device state.
- C is gnu89: declarations at the top of a block, no `//` comments in
  `source/`.
- Both menu themes (Glassed and Vanilla) must offer the same features.
- Don't commit game files (maps, disc images) or signing keys.

## Style

Match the file you are in. In our own files:

- Tabs. One `/* ---------- section */` header per section: headers,
  constants, structures, globals, private code, public code.
- Each file opens with a comment saying what it does, in plain sentences.
  It explains why, not what each line does.
- Comment a function when its name doesn't say enough: what it returns and
  who calls it. Short and plain beats long and clever.
- No dead code, no speculative options, no helpers with one trivial use.
- `snake_case`. Names say what a thing is (`device_group_find`, not
  `helper2`).

## Commits

Write plain messages: a short summary line, then what changed and why.
Commit messages and pull requests are public.
