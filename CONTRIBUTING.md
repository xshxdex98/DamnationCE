# Contributing to DamnationCE

DamnationCE is a fork of [OpenCE](https://github.com/cybersecurity/halo-ce-universal).
Fixes that belong in OpenCE itself are best sent there; this repository merges
OpenCE's builds as they come out.

## Pull requests

- **Open them against `xshxdex98/DamnationCE`'s `main`.** This repository is a
  GitHub fork, so GitHub's "New pull request" page suggests
  `cybersecurity/halo-ce-universal` first: change the base repository.
- The game protocol stays OpenCE's: don't change how games are joined or what's
  sent on the wire, so players of every compatible build keep playing together.
- Keep changes to OpenCE's own files small and commented (`port:`), so its next
  build still merges cleanly. Menus are data: change `port/assets/menus` and
  the tools that draw it (`tools/shell_art.py`, `tools/shell_skin.py`), not
  `port/assets/menus/ce`, which `tools/ce_menus.py` writes.
- Commit messages, pull requests and issues are public.

## Bugs

Use [the bug report form](https://github.com/xshxdex98/DamnationCE/issues/new?template=bug.yml).
The game's log is `debug.txt` beside it; attach it.

## Game files

DamnationCE never provides game files. Don't commit, attach or link maps, disc
images or other copyrighted game data.
