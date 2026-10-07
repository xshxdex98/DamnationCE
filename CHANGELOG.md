# Changelog

## 0.3.24

- The menus show again. 0.3.23's map checks treated every tag the menus
  add to a map as missing, so the menus drew nothing.

Network version 20, as 0.3.21: players on either can play together.

## 0.3.23

### Custom Edition maps

- Halo PC's server commands work in map scripts: `sv_say`, `sv_end_game`,
  `sv_map_next`, `sv_map_reset`, `sv_kick`, `sv_ban` (kicks), `sv_players`,
  `sv_timelimit`, `sv_friendly_fire` and `sv_log_note`.
- HUD bitmaps marked *half hud scale* or *force hud use highres scale* are
  drawn at half size, as on Halo PC.
- Map scripts may set any global, as Halo PC allowed (lolcano's jetpack,
  coldsnap's cheat guard).
- The multiplayer pause menu is centred in its box again; it had slid down
  once SETTINGS and END GAME were added.

### Weapons HUD

- A weapon's ammunition warnings follow its own ammunition: weapons with
  one magazine (coldsnap's flamethrower among them) no longer show "out of
  ammo" or "no fuel" while loaded.
- Weapons with one magazine no longer set off the secondary magazine's
  warnings, which showed NO FUEL beside NO AMMO on every weapon on maps
  whose master HUD warns of a secondary reload (coldsnap, hugeass).

### Multiplayer and co-op

- Vehicles' (and players') lights stay in step: a client that missed the
  press, or joined after it, no longer sees a vehicle's lights the wrong
  way round, and scripts asking whether they are on get the host's answer.

### From OpenCE (build-139)

- Every map's tags are checked before it loads: a damaged map is refused
  with a reason instead of crashing, and what can be corrected is.
  Custom Edition maps keep their own loader's checks. Map scripts may only
  call the functions maps need. (Not yet on macOS.)

### Level editor

- The game side of OpenCE-Tools' live view (a Sapien-style running game
  beside the editor). Not ready for use yet: it is being tested.

Network version 20, as 0.3.21: players on either can play together.

## 0.3.22

### Co-op

- Only the host's crossing of a loading zone switches the BSP, and it brings
  every player to the host, however far behind. No more being teleported
  back through the level by someone running ahead. A player standing on a
  loading zone is told it waits for the host.
- A dead host (or anyone) respawns beside a living teammate, not at the
  last checkpoint.
- The Maw's lift, and every other point where the level waits for the whole
  team, brings the stragglers along instead of leaving them behind.
- The Pillar of Autumn tutorial's look-at-the-lights step works for every
  player.
- `bringto` in the console brings every player to the host (host only).
- A scoreboard in the campaign: hold BACK (Tab) for the players' names and
  pings.
- Hosts with many extra enemies run faster: bodies far from every player
  are cleaned up, and the cleanup no longer runs every tick.

### Multiplayer

- A player joining a game in progress starts at zero, not with the score of
  whoever had their slot before.
- Bodies no longer hang in midair in large games.
- Killing blows reach every player, so bodies fall as they did on the host.

The flashlight and other dynamic lights, missing in 0.3.20 on Windows,
work again (since 0.3.21).

Network version 20, as 0.3.21: players on either can play together.

## 0.3.21

### From OpenCE

- Anti-aliasing, in Video Setup: FXAA, SMAA, 2x supersampling, or 2x to 8x
  multisampling.
- Per-pixel lighting of models, and sharper shadows (Shadow Resolution, up
  to 1024), in Video Setup.
- Reverb: sounds echo as the room they are in does, and are muffled behind
  walls (Audio Setup's Reverb). The sound is cleaner too: better
  resampling, a limiter in place of clipping, 3D sounds at the right
  distance, and Xbox ADPCM decoded properly.
- Public lobbies can have a password; the server browser shows a lock on
  them.
- Multiplayer maps can be played alone.
- A co-op option for players to collide with one another.
- The profile settings show the gamepad's layout.
- Dynamic lights no longer corrupt on Windows.
- More checks against damaged maps and network messages.

Network version 20: everyone needs 0.3.21 to play together.

## 0.3.20

### Menus

- A new menu theme, Cairo, in the style of Halo 2's menus. Choose it under
  SETTINGS > MENUS. It covers every screen, from the main menu to the
  lobby, the pause menu and split screen, and fills the window at any
  width, with its data streams and rulers sliding behind the menus.
- In Cairo, the main menu can play a song of your own in place of the
  game's music: put it beside `config.toml` as `music/cairo.wav`. The
  music fades from one to the other as you change themes, and loops
  with a fade.
- The gametype and settings lists' highlight no longer runs past the panel.

### Co-op

- The garbage collection notices are gone from the screen for host and
  clients (they show with `console_log = "all"`).
- Clients draw enemies with the right shader for their variant.

### From OpenCE

- Damaged maps and network messages are refused instead of crashing the
  game.
- Breaking glass over the network no longer misreads the level.

Network version 18: everyone needs 0.3.20 to play together.

## 0.3.19

### Co-op

- Triggers and cutscenes work for every player, not only the first two.
  Levels whose scripts wait for every player (the Maw's bridge, a30's
  Pelican pickups) now go ahead for any player, and players past the
  second are taken along by Pelicans, exits and cutscene moves.
- Enemies placed in cutscenes appear for everyone, like the Elite with the
  sword at the Silent Cartographer's shaft door.
- A scripted mission failure (the Maw's timer, Keyes on Truth and
  Reconciliation) goes back to the last checkpoint for everyone instead of
  locking the game.
- A cutscene waiting until it's safe to play needs only one player safe.
- Extra enemies spread onto free ground on their own floor, clear of
  crates and with room to stand, instead of stacking on one spot.
- Breaking glass and destructible scenery or machines happens the same for
  everyone.
- A client never goes back to a checkpoint on its own.

### From OpenCE

- Much faster with many enemies: The Silent Cartographer at 32x extra
  enemies went from 53 to about 97 fps (by MrBruh).
- Extra enemies that find no free ground are still placed, not left out.

Network version 17: everyone needs 0.3.19 to play together.

## 0.3.18

### Custom Edition maps

- Maps whose scripts use something this engine doesn't have no longer halt
  the game a moment into play ("script node index ... is unused or
  changed"). Those calls were made harmless, but the engine's clean-up of
  script data then threw them away while the scripts still used them.
- New tool, tools/custom_edition_script_names.py: lists the functions and
  globals a map's scripts use that this build lacks, checks every call's
  arguments, and writes a script back out as source. Every one of the 35
  Custom Edition maps checked has all its script names here.

Still network version 16.

## 0.3.17

### Custom Edition maps (adapted from ChupathingyCE's custom map work, by MrMilenko)

- Halo PC's sv_end_game works: a map's script can end the game on the host.
- Maps whose scripts call Halo PC's server commands (sv_map_next, sv_kick,
  rcon and the rest), change_team, set_gamma, thread_sleep or its sound
  settings keep their scripts: those calls do nothing here.
- Twelve of Halo PC's settings a map's script may set (hud_filter,
  sv_public, rasterizer_fps and others) are accepted and change nothing.

Still network version 16.

## 0.3.16

### Custom Edition maps (adapted from ChupathingyCE's custom map work, by MrMilenko)

- A map whose scripts use something this engine doesn't know keeps every
  script that does load. Before, one unknown line cost the map all its
  scripts; now only the scripts holding it are dropped, and the log says
  which and why.
- Halo PC's quit and sound_impulse_predict, which some maps' scripts call,
  no longer stop those scripts loading.
- none is accepted as an object name in scripts, as on Halo PC.
- More room for heavily scripted maps.
- Scripts' progress messages show on screen only with console_log = "all",
  as the Xbox never showed them.

Still network version 16.

## 0.3.15

### Co-op

- Joining a game in progress works again between DamnationCE and OpenCE:
  since 0.3.10, players on one joining a game hosted on the other never
  spawned and couldn't spectate. Elite majors and commanders keep their
  own armor.

Still network version 16.

## 0.3.14

### Co-op

- Going back through a loading zone works again: two thirds of the team
  within about 45 metres of it is enough, so a team walking back through a
  doorway together switches straight away.
- A player waiting at a loading zone for the team is told so, with how many
  are there and how many it needs.

Still network version 16: plays with 0.3.12 and 0.3.13. Going back works
the new way in games hosted on 0.3.14.

## 0.3.13

### Co-op

- When a loading zone switches the level, your player waits where they
  stand until your game has loaded the new part, instead of dropping
  through the floor for a moment first.

Still network version 16: plays with 0.3.12.

## 0.3.12

### Co-op

- Everyone stays on the same part of the level. A loading zone reaches
  every player at once, a player still loading it can't trigger another
  or be pulled around by their own lag, and no loading zone switches again
  until everyone has loaded the last one.
- In a big team, players who don't fit beside whoever crossed a loading
  zone are placed beside another teammate instead of being left behind.

Network version 16: everyone needs 0.3.12 to play together.

## 0.3.11

### From OpenCE

- Network version 15, now that OpenCE has taken the allegiance, loading
  zone and vehicle explosion fixes from 0.3.8 and 0.3.9. Everyone needs
  0.3.11 to play together.

## 0.3.10

### Co-op

- Elite majors and commanders wear their own armor for clients, instead
  of the minor's.

Everyone needs 0.3.10 to play together.

## 0.3.9

### Co-op

- Going back into a part of the level the team has already left needs two
  thirds of the players at the loading zone instead of everyone, so one
  player who stayed behind can't hold the team up.

## 0.3.8

### Co-op

- Marines are friends for clients too: no more red reticles on allies or
  marines showing on the wrong team.
- Going back into a part of the level the team has already left waits
  until everyone is at the loading zone, so one player can't drag the
  whole team back.
- Covenant vehicles the host destroys explode for clients too, instead of
  staying intact.

### From OpenCE

- Network version 14. Everyone needs 0.3.8 to
  play together.

## 0.3.7

### Co-op

- Clients see the black bars of cutscenes that leave the players their
  controls.
- From OpenCE: no more crash when extra enemies take a level past 256 AI.

## 0.3.6

### Co-op

- Crossing a loading zone brings the whole team to whoever crossed it, so
  nobody is thrown back to a hallway, under a floor or behind a locked door
  when the level loads its next part.
- The rescue for a player outside the level only takes someone actually
  falling, after two seconds, so riding an elevator no longer teleports you.
- Doors and elevators match the host's for everyone: no more doors stuck
  half open (Captain Keyes' door, Assault on the Control Room's bridge) or
  elevators that leave clients behind and drop them.
- Flood combat forms that get back up do so on every machine, instead of
  their bodies sliding around on clients.
- Dropship doors open and close for clients as they do for the host.

## 0.3.5

### From OpenCE (build-121)

- Co-op's Server Setup has FRIENDLY FIRE between players, on by default.
- EXTRA ENEMIES: enemy squads grow with the players (a percentage of
  themselves per player past the first) or by a fixed multiplier.
- Co-op games start with room for 16 players, and private.
- A kick command for hosts.
- New Game can also play multiplayer maps alone; they don't replace the
  campaign's saved game.
- Leaving the lobby after a game goes to the main menu.
- The host only takes grenade damage from grenades actually thrown.
- Their review fixes to co-op: device messages split to fit, screen
  effects and cameras from the host checked before use.

Players need 0.3.5 (or OpenCE build-121) to play together: the network
version changed.

## 0.3.4

### From OpenCE

- Resolution, scaling and window size settings.
- Textures bind correctly after their first upload.
- No more vertices shooting to the middle of the screen near the camera.
- The death and respawn delays are the same at every frame rate.
- Friendly fire setting stops teammates' instant kills.
- Windows release builds open without a console window.

### Co-op

- AI on any team keeps its team on clients, for multiplayer maps with AI.

## 0.3.3

### Co-op

- Finishing a level takes everyone back to the lobby with the next level
  ready to start (or another to pick), instead of freezing every game on
  the level's last white screen. It works whoever reaches the end first.
- Marines and other friendlies no longer show as enemies on clients'
  reticles.
- Skipping a cutscene stops its music and dialogue for everyone.
- The Pillar of Autumn's tutorial prompts show for clients every time.

## 0.3.2

### Co-op

- Crossing into a new part of a level always brings the players left
  behind along, even when whoever crossed hasn't fully arrived, so nobody is
  left standing where they would switch the level straight back.
- Levels that move every player at once by script (The Library, Two
  Betrayals, Keyes, The Maw) spread the players around the spot instead of
  stacking them on top of each other.
- A player who respawns can no longer be left at the start of the level
  when there's no room beside the teammate they were meant to join.
- When everyone dies after reaching a new part of a level but before its
  next checkpoint, everyone comes back where they last stood instead of
  falling out of the world.
- Players who joined since the last checkpoint come back beside the others
  instead of on top of one of them.
- Someone joining in progress sees the current objective and waypoints.
- The playable Elite and the lobby's PLAYER button are gone; everyone plays
  as a Spartan in their own profile colour.

## 0.3.1

### Co-op

- Two players standing on different loading points no longer make the
  level switch back and forth. That switching made textures flicker, AI
  glitch and doors stay shut.
- Every player gets the shield back at the Pillar of Autumn's charging
  station, not only the host.
- Cutscenes play smoothly on every machine, without the camera stuttering.
- Someone joining in progress gets the doors, light bridges, moved scenery,
  music and screen effects straight away, instead of falling through a
  bridge that hadn't appeared yet.
- A Pelican dropping a Warthog brings one Warthog for every four players,
  up to five, so bigger games have enough rides.
- Players' names and the markers over their heads move smoothly with them.

### Menus

- Clicking PLAYER: SPARTAN / ELITE in the lobby now changes it.

## 0.3.0

### Co-op

- Elite players see their shield and health meter, and no longer freeze in
  place after landing a jump.
- An Elite player respawning no longer stops the game.
- A client's view stops shaking when the host's cutscene shake ends, even
  if the cutscene was skipped.
- Fleeing enemies no longer run in place on clients.
- Either co-op player can pick the level.
- Split screen players can join a game that is already under way.

### Custom Edition maps

- Large maps such as Coldsnap have enough memory to load, and scripts that
  use Halo PC's teammate-name and developer settings run.
- Maps with damaged script threads, particles, shader layers, sounds,
  bitmaps or animations play on instead of crashing or hanging.
- Projectile trails show from a weapon's first shot.
- A '%' in a map's script messages prints as written.
- The first-person arms and other models drawn by parts are placed by the
  right bone.
- On 64-bit builds (Linux, macOS, Android), deeply nested scripts no longer
  overwrite each other and crash the game.

## 0.2.9

### Co-op

- Play as an Elite: the lobby has a PLAYER: SPARTAN / ELITE button on the
  campaign levels that have Elites. An Elite player uses the Spartan's
  animations and movement, so it holds every weapon, drives every vehicle
  and strafes like the Spartan.
- Everyone wears their profile colour, instead of the Spartan's fixed green.
- Players' names show over their heads.
- A player who leaves is removed from the game; their model no longer stays
  behind or respawns.
- When the level moves to its next part, only players left outside the map
  are moved. Players far apart were being pulled back and forth, snapping
  back as they walked.
- Dropships and Pelicans flown by the AI move smoothly, and enemies no
  longer hitch on small corrections.
- The Pelican rides at the start of a level (The Truth and Reconciliation,
  The Silent Cartographer) are watched from your teammate's seat, wide
  enough to see them whole.
- Someone joining during a cutscene gets the vote to skip it, and machines
  still loading no longer hold the vote up.
- Save and Quit takes every split screen player on that machine out at once.

### Custom Edition maps

- Protected maps load.
- Maps whose models or animations have broken bone lists are repaired as
  they load, instead of hanging.
- A map whose scripts can't be loaded plays without them instead of halting,
  and maps with deeply nested scripts run on 64-bit builds.
- Several kinds of unusual map data no longer stop the game: HUD meters,
  counters and sounds, collision edges, unit dialogue, short weapon lists,
  and odd shaders under camouflage.
- Messages a map's scripts send to the players (sv_say) show on screen.

### Menus

- In Glassed, the gametype lists light the row under the mouse.

### Other

- On Windows, the updater's and platform's messages are written to
  debug.txt, so a failed update shows why.
- The host logs how many of its clients' hits it accepted and refused.

## 0.2.8

### Co-op

- When the level moves on to its next part, every player is brought along,
  not only the host. Before, the others were left where the old part was
  and fell into the void until the rescue caught them.
- A player left outside the map in a vehicle is brought back too, unless
  the AI is flying it (the intro Pelicans fly outside the map on purpose).
- A player left outside the map with no teammate standing on ground is
  brought beside any teammate inside it, or to the last checkpoint.

## 0.2.7

### Co-op

- Joining a game in progress spawns you straight away and starts you
  watching another player, from behind them, instead of showing their
  first-person view.
- A player who joins in progress hears the music and ambience already
  playing.
- A spectating or joining player no longer sees enemies and vehicles
  rubberband or drive on their own. The host was sending them every moving
  object every tick, which flooded their connection; they now get what is
  near the players, as everyone else does.
- Switching between players while spectating works.
- New players can spawn beside a teammate driving a vehicle.
- A player left outside the map when the level moves on is brought back
  beside a teammate after a second.
- Respawning is held back only by enemy grenades and fire close to the
  teammate you come back beside, and after 10 seconds you respawn anyway.

### Menus

- In the Glassed menus, lists highlight the entry under the mouse and
  scroll one entry at a time, by mouse wheel or keys.

## 0.2.6

### Custom Edition maps

- A map whose tags point past the end of a list no longer crashes the
  game. Halo PC never checked these, so many maps have them; they now read
  as empty and are logged (this crashed foundation@ce 13 seconds in).
- Uncompressed sounds and mono 44 kHz sounds play. They were refused every
  time they played; they are now converted when the map loads, as Ogg
  Vorbis sounds already were.
- Maps with big Halo 3 and Reach textures show them: the texture cache is
  64 MB, up from 44.
- Maps whose scripts use functions or settings this build doesn't have
  (OpenSauce's, for example) load, with those doing nothing, instead of
  being refused.
- Maps with a shader marked as the wrong type (as protected maps have) load
  instead of being refused.
- Maps with very detailed models (44 bones or more) load, as long as each
  part of the model fits.
- Custom Edition singleplayer maps are listed under CUSTOM in co-op's
  campaign list.
- The log no longer claims a Custom Edition map "cannot run" when it can.

### Co-op

- After The Maw, the campaign goes on to The Pillar of Autumn. Every won
  level takes everyone back to the lobby with the next one set; a Custom
  Edition campaign map repeats.

### Crash reports

- On Windows, a crash's log names the functions and source lines it
  happened in (halo.pdb now comes with the game), so crashes can be found
  and fixed.

### Updates

- The log says why a build doesn't look for updates.

### Internet play, from OpenCE

- The brokers internet play finds games through are read from brokers.txt
  beside the game.

## 0.2.5

### Updates

- The game looks for updates again. When a new release is out, it offers
  to update as it starts. Network Setup's CHECK FOR UPDATES turns this on
  and off, and it is now on for everyone.

### Online split screen, from OpenCE

- In a game's lobby, another controller's START joins it as a second
  player, who picks a profile. ADD PLAYER makes the next controller's START
  count. The lobby marks this machine's players [P1], [P2].
- A split-screen player who quits leaves the others in the game.
- Works in both the Glassed and Vanilla menus.

## 0.2.4

### Co-op

- On Pillar of Autumn the other players no longer wait for the first
  checkpoint. They watch the host's cryo tube from the front, then spawn
  beside the host four seconds after it is out of the tube and can move.
- The tutorial goes on for whichever player does what it asks, and looking
  around with the mouse counts as well as the right stick.
- When the level's scripts hold the host's controls, they hold every
  player's.

## 0.2.3

### Co-op

- Everyone in the game, including players who join late, sees cutscenes
  live through the host's camera. This fixes the pink screen joining
  players got.
- Cutscene effects show for everyone: full-screen blur, filters and static,
  effects the scripts play, and objects the scripts attach, also for players
  who join late.
- Scenery and machines the scripts move are where the host has them, and
  objects that change their look do so for everyone, also for players who
  join late.
- Vehicles nobody drives, like the Pelicans on The Silent Cartographer,
  move smoothly instead of jittering.
- When a level starts with everyone riding in, the other players watch a
  teammate in their seat and spawn beside them four seconds after they are
  on foot.
- A player with nothing to watch sees the host's view.
- The host counts everyone's votes to skip a cutscene. The vote opens once
  the cutscene's checkpoint is saved.
- Start opens the campaign's pause menu.
- Split-screen co-op, from OpenCE.
- Fixed a crash when the level Halo starts: a teleport that finds no room
  for a player now leaves them where they spawned.
- The respawn safety check runs twice a second, which is lighter on the
  host.

### AI and netcode

- Enemies and allies melee, leap, fire in bursts, speak, panic and animate
  as they do on the host.
- Shields flare on every machine when they are shot.
- Units have the host's colors (no more blue Elite on one screen and red on
  another).
- Flinches and death animations are the ones the host picked.
- A second copy of the game on the host's computer gets the host's game
  state, instead of a purple screen.

### Menus

- Online Games lists games in sortable columns. Backing out of Create Game
  returns to it.
- The lobby is drawn in the server browser's style: a column per team,
  player tags, the game's details, and a large start countdown.
- The Vanilla theme uses the stock menus again. Co-op or PvP, and stock or
  Custom Edition maps, are steps in the Map screen's own list, and campaign
  level pictures fit their frames.
- The pause menu has Settings and End Game. Quitting from it with the mouse
  or keyboard no longer crashes.
- The server browser outlines the selected row, and players who quit are
  left off the scoreboard.
- Fixed a crash when switching themes.

### Gameplay, from OpenCE

- In multiplayer, walking over a second weapon picks it up and readies it,
  as in the campaign.
- No unarmed grenades or melee from vehicle seats.
- Enemy names show within the motion sensor's range, with a setting.
- The shotgun's ammo meter is left-aligned.

### Custom Edition maps

- Protected maps load.
- Their pause menu has Settings.
- Sounds set louder than full volume play at the right loudness.
- Fixed crashes from some particle effects and unknown function types.

### Networking

- LAN searches are answered directly, and every packet keeps the port it
  came from, so LAN games show up and can be joined.
- Builds of main offer each newer build as an update.

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
- The main menu's background keeps moving behind every menu; it used to
  freeze or jump back in most of them.

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
