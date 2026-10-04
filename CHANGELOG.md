# Changelog

## 0.2.2

### Crash fixes

- Fixed a crash about four seconds into setting up any network game (LAN or
  internet, co-op or PvP), for example while picking a map. It came in with
  0.2.0.
- Fixed a crash when a co-op game starts loading a campaign level.

### Co-op

- Fixed the black screen after skipping a cutscene.
- Everyone but the first player now waits for the level's first checkpoint
  before spawning, watching the first player meanwhile. On Pillar of Autumn
  that means after the cryo tube and the tutorial.
- Respawning checks that the teammate you come back beside is safe (no
  enemies attacking them, nothing exploding nearby), instead of waiting for
  the whole map to be quiet, which with many players could take minutes.
- The lobby shows the right map name for campaign levels and Custom Edition
  maps (it showed Battle Creek).
- Co-op games in the server browser show their difficulty ("Co-op Heroic").
- Fixed checkpoints and the next level carrying over between games.
- Co-op code no longer runs in the menus while a game is being set up. It
  used to stop the menu background's scripts for joining players.

### Menus

- The Glassed theme's text is now set in Rajdhani, a free font in the style
  of Conduit. Vanilla keeps its fonts.
- The Glassed server browser uses the whole screen: games down the left
  edge, the selected game's details at the right edge.
- Server browser notices (missing maps, refreshing) and full or closed games
  are teal, and notices sit under the game list instead of over the header.
  The main menu's version number is teal too.
- The main menu's background no longer freezes or jumps back when some
  menus open.

## 0.2.1

Finishes co-op. Host and players all need 0.2.1; it doesn't mix with 0.2.0
in co-op games.

- **Vote to skip cutscenes.** Press Space (or A) during a cutscene to vote.
  Everyone sees the count, and the cutscene is skipped once more than half
  the players have voted.
- **Everyone now sees what the level's scripts do**, not just the host:
  chapter titles, help and objective text, "Checkpoint" messages, the
  mission timer (like the countdown on The Maw), screen shake, nav points,
  HUD elements the scripts hide or show, and characters' cutscene
  animations.
- **New logo**, on GitHub and as the macOS and Android app icon.

## 0.2.0

### Campaign co-op online

You can now play the campaign with friends over LAN or the internet, with
up to 128 players.

- **Create Game** asks whether you want **Cooperative** or **PvP**. Under
  Cooperative, pick Campaign, then either the stock levels or Custom
  Edition campaign maps from your maps folder. Then choose a level and a
  difficulty, name the game in Server Setup, and go to the lobby.
- Co-op games appear in the server browser as "Co-op", with their level
  and difficulty.
- The host runs the campaign. Everyone sees the same AI, cutscenes,
  camera moves, fades, dialogue and music, doors, elevators, switches, and
  objects the level's scripts create or remove.
- When you die, you watch a living teammate (A or jump switches between
  them). You respawn beside a teammate once it's safe.
- If everyone dies, you all respawn where you were at the last checkpoint.
- When the level is finished, everyone moves on to the next one together.
- Players who join a game already in progress spawn next to the others.

### AI sync

- Enemies and allies now move, aim, shoot, throw grenades and animate the
  same way on every machine, not just on the host.

### Server browser

- It now also lists public games from the internet lobby.
- Games on Custom Edition maps show the map's name and picture, and say so
  when you don't have the map.
- Plain buttons along the bottom for Join, Create Game, Refresh, Sort,
  Profile and Back. Escape goes back.

### Map picker

- The keyboard prompts are now clickable buttons (Back, Grid/List view,
  Difficulty). Escape goes back.
- It starts from the first step every time you host, rather than where you
  left off.
- Campaign level pictures now fit their frames.

### Fixes

- A LAN host can start a game alone.
- Loading a Custom Edition map with Ogg Vorbis sounds no longer crashes.
- Custom Edition maps use the Xbox pause menu, so Resume and Quit work.
- Better frame rates on Custom Edition maps: their geometry stays on the
  GPU, and lens flares out of view are no longer tested or drawn.
- The invite link for internet co-op games shows correctly instead of
  "LAN game".

### For developers

- `AGENTS.md` files explain the layout, the build, and the rules for
  changing the code. `port/linux/NETCODE.md` covers co-op and AI sync.
- `debug.gpu_stats` and `debug.gpu_trace_heavy` help track down slow
  frames.

## 0.1.0

The first DamnationCE release: OpenCE with ChupathingyCE's features, the
Glassed and Vanilla menu themes, and Custom Edition map support, for
Windows, Linux, macOS and Android.
