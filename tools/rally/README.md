# Rally demo

Bots join your system link game and run to where you stand: up to 127
of them, a game of 128 players on one PC. For a PC where
[HaloLauncher.exe](https://github.com/bnunu/halo-ce-universal/releases/latest)
installed Halo CE Universal.

In a Command Prompt:

```bat
curl.exe -fsSL -o "%TEMP%\rally.cmd" https://raw.githubusercontent.com/bnunu/halo-ce-universal/main/tools/rally/rally.cmd && "%TEMP%\rally.cmd" 127
```

`rally.cmd` starts Halo. Create a system link game (Multiplayer), wait in
the lobby, then press a key in the rally window: the bots join, the game
starts, and about 3 seconds in they run to where you are standing. Ctrl+C in
the rally window takes them out. Give it another number for fewer bots.

The first time (and after every update of the game), `rally.cmd` adds a
position dump to the installed game's `source/game/players.c`
(`rally_patch.py`) and rebuilds it, which takes a minute or two. With
`HALO_POSITIONS` set, which only `rally.cmd` sets, the game writes every
player's position to `positions.txt` in its data folder every 3 ticks; the
bots (`system_link_bots_rally.py`, `tools/system_link_bots.py` with
`--wander`, `--positions` and `--gather`) steer by it. The dump is in the
port's `HALO_LINUX` code, so the Xbox build is untouched.

The bots are stand-ins that speak the system link protocol and only run,
jump and turn: they don't fight.
