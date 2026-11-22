# Gravity Leon development notes

Gravity Leon is a C++17 platformer built on raylib. Most of the game lives in `src/main.cpp`, where `Level` reads a plain-text tile map and `Game` advances the simulation at a fixed 120 Hz. Rendering interpolates between physics steps. Movement and collision use the player’s local gravity frame, so the same rules work on floors, walls and ceilings. The grappling tongue wraps around tile corners, and Verlet integration gives its slack section a natural sag. Separate modules handle controller input with SDL-format mappings, live tuning from `data/params.txt`, profiling and audio. Sound effects are synthesised at startup. A custom four-channel ProTracker player streams the original `.mod` music, and raylib handles the MP3 tracks.

Leon and Anna are drawn and animated in code, down to the way Anna’s hat brim bends against a wall. In engraving mode, the renderer draws the world to an RGBA texture and the characters and monsters to a separate tint texture. A full-screen GLSL fragment shader uses each pixel’s brightness to lay down diagonal ink strokes, adding a second diagonal and then horizontal hatching as shadows deepen. It reverses the camera transform to keep the ink anchored to the walls through gravity turns. Screen-space derivatives smooth the stroke edges, and the tint texture supplies a wash of colour beneath the ink. For the sketch’s repeating Japanese patterns, the CPU skips tiles outside the view or hidden by solid ground. It packs the visible strokes into a reusable GPU vertex buffer capped at 16 MiB, then draws each run of one colour in a single call. In a stationary Hard Mode scene at 1280 × 720 on an Apple M3, these optimizations cut median CPU work per frame from 15.67 ms to 0.64 ms. Seeded screenshot comparisons checked that the drawings still matched pixel for pixel. See Optimization measurements in *workshop/optimization reports* if you are curious.

The same game code builds natively on macOS and Linux and compiles to WebAssembly with Emscripten for itch.io. The shaders share their rendering logic across desktop OpenGL and WebGL 2. In the browser, the procedural sky shader receives cloud shapes for three parallax layers through a uniform buffer. Both renderers use colour-only off-screen framebuffers where supported, saving four bytes per pixel on each target by omitting the depth attachment. The browser build follows the browser’s animation frames and saves highscores in IndexedDB. For debugging across a level, Shift+B captures a screenshot, the game state and the exact tuning values in use. Loading the report with `--from-report` lets a developer return to the reported scene and run it with scripted inputs.

## Building and packaging

On macOS or Linux, use a C++17 compiler, Make and raylib. The Makefile uses `pkg-config` to find raylib and also recognises a Linux source build at `~/.local/opt/raylib-6.0/src`. Plain `make` builds `./gravityleon`. To build and launch it, run

```sh
make run
```

On macOS, `make dist` uses Python 3 and the static raylib library to build `dist/GravityLeon`, a standalone executable with embedded resources. It writes scores under `data/`, screenshots under `screenshots/` and reports under `Reports/`, beside the executable.

`make dist_mac` packages the executable as `dist/GravityLeon.app` and signs it ad hoc. The icon step uses Pillow and `tools/icon-macos.png`, the rounded version of `tools/icon.png`. The app uses the same save directories under `~/Library/Application Support/GravityLeon/`.

On Linux, `make linux_dist` uses Python 3 and static raylib to build `dist/GravityLeon-linux`, a stripped executable with embedded resources. It uses the system libraries for the Linux desktop and writes scores, screenshots and reports beside the executable.


On Windows, run `make windows_dist` from an MSYS2 UCRT64 shell with GNU Make, Python 3, a MinGW-w64 C++ compiler and a static raylib built for the same toolchain. The target produces `dist/GravityLeon.exe` with embedded resources and writes scores, screenshots and reports beside the executable. Windows also uses its temporary directory when reading some embedded resources.

The target uses the compiler and raylib flags from the regular build by default. With a separate raylib build or a cross-compiler, set `WINDOWS_CXX` to the MinGW-w64 compiler, `WINDOWS_RL_STATIC` to the matching `libraylib.a`, and `WINDOWS_RL_CFLAGS` to the matching header flags, such as `-I/path/to/raylib/src`.

Game settings live in `data/params.txt`. When running the local executable, edits take effect during play. Git tracks the project data in `data/`. Playtest screenshots and reports stay local, with `screenshots/screenshots.txt` and `Reports/guide_for_AI.txt` shared in Git.

### Distribution highscores

`make dist`, `make dist_mac`, `make linux_dist`, `make windows_dist` and `make web-dist` run `tools/update_dist_highscores.py` before packaging. You can also run the script directly. For each character and mode, it keeps the fastest recorded time from the local and distribution tables. An equal time keeps the existing distribution initials. A mode appears once someone has recorded a time for it.

The packaged highscore tables come from `data/dist_highscores.txt` and `data/dist_highscores_anna.txt` and ship under the names `data/highscores.txt` and `data/highscores_anna.txt`. The updater also reads older files named `highscore.txt` and `highscore_anna.txt` if present. Both packagers include only `params.txt` and the two starting tables from `data/`. Existing player tables take precedence. Character unlocks last for the current session.

### Browser build and itch.io

On macOS or Linux, the browser build uses Git, Make, CMake and Python 3. The first build downloads Emscripten 4.0.20 and raylib 6.0 into `.scratch/web/`. The SDK and its cache stay in this checkout. The four source files under `web/` belong in Git. `index.html` handles startup, focus and keyboard events, `platform.cpp` and `platform.h` connect the C++ game to the browser, and `storage.js` saves scores in IndexedDB. The build generates the playable files in the ignored `dist/web/` directory.

```sh
make web           # Build the playable files in dist/web/
make web-run       # Play at http://127.0.0.1:8000 (override with WEB_PORT=8080)
make web-dist      # Package dist/game-web.zip
make web-clean     # Remove web outputs and raylib build objects, keeping the isolated SDK and sources
```

Upload `dist/game-web.zip` to an itch.io project of type **HTML**, and select **This file will be played in the browser**. The ZIP contains `index.html`, `index.js`, `index.wasm` and `index.data` at its root. The data file includes `data/params.txt`, the starting highscore tables, the levels, and the game assets. For a widescreen embed, use 1280 × 720 and allow fullscreen.

Click **Play Gravity Leon** inside the embed to start audio and give the keyboard focus. The game requires WebGL 2. A standard browser gamepad works after its first button press. **Shift+F** toggles fullscreen where the embed permits it. Highscores are stored in the browser for the game's origin. If storage is unavailable, the game displays a notice and keeps scores for the session. **Shift+T** offers a PNG download, and saving a **Shift+B** report offers both the PNG and the text file.


## Controls

| Action | Keyboard | Gamepad |
|---|---|---|
| Run (tap = walk, hold = run) | Left / Right (or A / D) | left stick (analog), D-pad |
| Jump (hold for higher), again in mid-air | Space / W / Cmd (Ctrl off a Mac) | A |
| Look up / look down (also while running) | Up / Down or S to look down | D-pad, left stick up / down |
| Look far up, down or to either side | Alt + arrows | Right Stick |
| Tongue: throw where you look / release (again while flying: re-throw) | X | B |
| Pump a tongue swing | Left / Right | stick |
| Swallow one chew of a stuck or limp tongue | V (once per press) | RT (once per pull) |
| Pay out the stuck tongue quickly | C (held) | LT (held) |
| Let go of the stuck tongue with a jump | Space / W / Cmd (Ctrl off a Mac) | A |
| Anna only, dash four tiles in the direction she faces | Tap and release either Shift key | RB or LB |
| The title, both games finished: start Leon's game / Anna's | Y / X | Y / X |
| The title, debug mode, the two not both finished: switch Leon's game and Anna's | Shift+Enter | LB + RB together |
| Screenshot to `screenshots/` | Shift+T | |
| Restart (reloads the level) | Shift+R (Shift+Ctrl+R: then 3, 2, 1, GO -- Leon held, his eyes shut, the clocks waiting, till GO) | |
| Suicide: trembling harder and harder, then a burst (let go before: he calms down) -- back at the start of the latest mode running (its key; easy mode: the start), its clock from nothing, his powers as they were then (double jump, tongue, hat -- the fruits eaten since, back in their places), the pineapples lit since grey again (to be eaten again), after 3, 2, 1, GO | | RT + LT + both sticks pushed in, held |
| Fullscreen | Shift+F | |
| Exit, asking first (the screen fades to black): on the title "Exit Game?", in the game "Back to Title?" (yes: the level from the start, on the title) -- NO at first, pick with Left/Right, answer with a jump or Enter, ESC again: no | Esc | Menu (then stick / D-pad, A; Menu again: no) |
| Cheat mode: typing `SUPERBO` (`cheat_code` in params.txt: letters and digits) -- its letters in a row, either case, nothing else pressed between them, within 4 seconds ("Cheats !") -- turns it on till the game quits: debug mode at once (DEBUG MODE bottom right, the stats bottom left, no highscore); the cheat keys count half a second after it -- its last O is not a jump | SUPERBO | |
| Cheat mode -- on / off at once: the red key, the blue key, the green key, the violet key, the golden key, the double jump, the tongue, the hat | 1, 2, 3, 4, 5, 8, 9, 0 | |
| Cheat mode -- to the start of a mode (the run restarted there, as `start_level` has it): easy, hard, dark, harder, cameleon; P: the end of the game, at the golden key (taken at once; every mode's door open, the flags furled, in silence) | T, Y, U, I, O, P | |
| The look: sketch, or engraving (black and white steampunk, after Gustave Doré) -- in turn (`look` in params.txt) | Shift+S | |
| GOD on / off (debug mode only): Leon golden, glowing in the dark three golden flies' belly light; Monstermunchs' tongues bounce off him as off ice (none eats him); dark matter is ground to him -- to walk on, to stick his tongue to. Off: where he is, his checkpoint | Shift+G | |
| Bug report (text box + picture into `Reports/`) | Shift+B, type (arrows move in the text), then Enter to save / Esc to cancel | |

The game keeps its shape on every screen (`screen_ratio`, 16:9): a screen of another shape shows it
whole, black bands round it, never stretched (screenshots and report pictures: the game alone).
Behind the world, in the sketch look, a far layer: a Japanese pattern repeated in faint ink
(`background_pattern_opacity`), each mode its own (`background_pattern_easy` ... `_cameleon`; a missing mode setting falls back to `background_pattern`) -- as `params.txt` has them: easy asanoha (hemp leaf), hard seigaiha (waves),
dark ichimatsu (checkerboard), harder yagasuri (arrow feathers), cameleon sayagata (interlocked manji); then, from
the golden key's first taking on (a golden run too), whatever the mode, `background_pattern_golden`: karakusa
(scrolling vine), the one no mode uses; none for bare paper. Its size `background_pattern_scale`, sliding with
`background_pattern_parallax` of the world's moves; another mode, the old pattern fades out as the new
one fades in (`background_pattern_fade`, 3 seconds).
The dotted camera frame is hidden with the current `camera_zone_show = off`. Set it to `debug` to display it in debug mode, or `on` to display it during ordinary play.

The fruit lessons display the following command text. A and B appear as green and red circular buttons, respectively. Right Stick is violet, LT and RT are brown, RB and LB are orange, and keyboard keys are blue.

| Lesson | Command shown |
|---|---|
| Double-jump ! | `Joypad (A) - keyboard <spacebar>` |
| Tongue ! | `Joypad (B) - keyboard <X>` |
| Extend or Swallow Tongue ! | `Joypad LT or RT - keyboard <C> and <V>` |
| Look down ! | `Right Stick or keyboard <Alt>+<Down>` |
| Dash ! | `Joypad RB or LB - Keyboard <Shift>` |

The single-character title reads `Press A or <spacebar>`. The shipped level uses keys and flags as objectives. Custom maps can place stars with `*`. In a map without an exit, collecting the last star finishes the run.
Hitting an ordinary ceiling hard enough or landing after a fall longer than two screen heights can knock Leon out. A whole hat can absorb a ceiling hit.
In the air on his stuck tongue, his feet point outwards -- the left one left, the right one right -- and, swinging,
his legs turn out from his body with the swing (drawn only): out at each end of it, back at its bottom, as far as
45 degrees for a swing a quarter turn wide, less as it is less. His mouth open in the air -- the body split in two halves
round the tongue --, each leg hangs from the bottom of the half it is in, as drawn.
Flies buzz about: the tongue snaps them up; chewed, a fly is coughed back out golden and follows
Leon for ever.
Only what lies within two screens of Leon goes on moving -- flies, lamps' blackouts, blocks, a lost
hat, the Monstermunchs (one farther goes back to sleep at once, its tongue in) -- and only what the
screen can show is drawn.
Near the exit lies a hat -- a tall crown in four bands, from the brim up colour, white, colour, white: the colour the last key taken's (`key_color_*`), black before any; its white white -- through the Love door, the bands and white take his body colour, pink in a regular game or grey in a Solo Run; the Solo door's gold, not (white and the key's colour still) -- a colour fly's tint as his body, GOD's gold; knocked off, its own still: worn, it takes the bumps on his head -- crushed by the ceiling it meets,
folding up like an accordion -- its four bands its panels, two pleats a side, zigzag: folded, still a bellows --, then unfolding -- instead of Leon being knocked out (the jolt shuts
his eyes for `hat_bump_shut_time`, then he blinks, `hat_bump_blinks` times in `hat_bump_blink_time`); until it has unfolded, it protects no more.
Dark matter (spikes) is no ceiling to it: the hat takes no bump off it and stops nothing -- squashed into what room is left over his head, it lets him live
while only the hat is in it; his body there, he dies, hat or no hat.
Pedalling in a long fall, its brim's ends flap in the air (`hat_brim_flap*`). Caught by a Monstermunch's
tongue, he loses it: it flies off, falls, bounces and rolls, and lies where it stops, upright, its brim on
the ground's line -- lying on a Monstermunch that munches (or chews), it hops at each munch, off to the side
of its middle it lies on, the farther out the harder -- his
again when he touches it, or when his tongue catches it (as a fly): brought back on its tip, his eyes
on it, the tongue curls up over his head and puts it on, his mouth closing as it does (the hat
lands just as it was, same size, no gulp) -- coming back on its own, even past ice, every
tongue command (B, RT, LT) ignored till the hat is on his head (a death drops it). Without it, the first bump is his.
Back from that death, he has it on his head again (`hat_back_on_respawn`; `False`: it lies where it fell, his to
fetch -- or, still lost as he comes near a spare-hat spot `h`, it waits there, put there just before the spot can
show on the screen, unless where it lies might show).
The clocks show top right, each by its mode's name: EASY RUN, HARD RUN, DARK RUN, HARDER RUN, CAMELEON RUN --
in a golden run, its own alone: SOLO RUN (the golden run's name on the screen: its clock, its highscores, its flag).
Any clock still running at an hour (`clock_limit`, in hours:minutes:seconds) and the player explodes. A Solo Run restarts at the beginning with a fresh countdown. Other modes return to the title. Its last ten
seconds it shows in red, blinking, faster in the last two (`clock_*`). The title always shows the
level from its start, whatever `start_level`: that place is taken as the game starts.
The title comes up from black (held black until the window has settled into fullscreen, `fullscreen_toggle_wait`):
GRAVITY LEON and the scene behind it fade in over `title_fade_in_time` (1 s), then the
prompt, highscores and credits fade in over `title_fade_in_rest_time` (1 s). Setting `title_scores_fade_in_early` above its current 0 s gives the tables a head start. On the single-character title, A, Space or Enter starts the game once the first fade completes. The grey bottom-left build tag (RGB 128, 128, 128) shows the build machine's local time as `Build YYYY-MM-DD_HH:MM`, at half the size of the highscore entries.
Opening “Back to Title?” during play fades the music out over `music_fade_out`. Choosing No fades the
playing track back in over `music_fade_in`. Choosing Yes fades in the title music. The post-credits
messages keep playing the ending music.
Easy mode runs from the start to the yellow flag at the end of the exit room's left corridor. The next four modes each start at a key and end at their own flag, on separate clocks.
The red key at the end of the right corridor starts hard mode ("Red key !"). On the way back, the red key is the new checkpoint and the pineapple by the tongue fruit becomes available. Earlier easy-mode pineapples disappear. Checkpoints beyond locked doors remain for later modes. The
key shows in the top-left corner until it opens the red door left of the start: touched with it,
the door turns gold, then fades away (a door's keyhole always upright on the screen, whatever the gravity). Behind it, a corridor leads to the red flag: hard mode is
over (its clock stops, if it ran). Further on, down the long fall into the dark, in the first dark
room, lies the green key: dark mode -- lamps and their flies for light, on down to the green door
and the green flag, between two lamps. In the dark below lies the blue key:
harder mode, the way back up to the blue door, at the end of a corridor high in the left wall of
the first dark room, and the blue flag behind it, between two lamps. Left of it, a corridor -- a block in its mouth,
as high as it: to be pushed through, or dragged back out with the tongue -- to an ice room: on its floor, three up-gravity tiles, and over them, in its ceiling, a chimney as long as
the pit, up out of the dark (it fades as he rises) -- seven tiles wide in the dark, ice walls, lamp posts
(no fly) out of one wall then the other, ever farther apart: the first two tiles up from its foot,
the next four on, then six, eight... (falling up, he meets them at a steady beat) -- none in its last
 stretch, where the dark fades, and its top open
its whole width -- to a room in the light and the purple key
("Purple key !"): cameleon mode, on its own clock (its end, the violet flag, `V`, at the far end of the cameleon level below). In that
room's ceiling -- the ground under Leon there, his gravity up -- by its right wall (on his left, on
the screen turned round), sleep four Monstermunchs side by side, from the wall on -- four tiles out from
them, a little platform (`===`), a row of ice under it (on the screen): it stops their tongues; level with it, a
corridor 20 tiles long into the room's right wall (on his left), ending in the colour room: 36 tiles wide, 11 high -- over each balloon pole, three Monstermunchs in its ceiling, 2 tiles apart,
just reaching its floor (the gold pole's a tile short of centred: the room ends there), and one in its floor 2 tiles past
the pole; each group of four the four suits, in its own order --, its floor the
corridor's, six colour poles on it, six tiles apart, from the corridor on -- for blue, magenta, then gold, its colour fly's pole (`4`, `5`, `6`)
then its balloon's (`1`, `2`, `3`).

A Monstermunch (`M`) occupies 2 × 2 tiles and faces the open side. The scenery behind it shows around its rounded corners and through its mouth. Its design and face markings are drawn in `workshop/specs and corrections/monstermunch-design.jpg` and `workshop/specs and corrections/monstermunch-love.jpg`.
Each face has a playing card’s suit, assigned by `monstermunch_suits` in map order. The black suits have red eye outlines. The eyes have yellow retinas and blink and twitch according to the `monstermunch_heart_*`, `_eye_*` and `_blink_*` settings. A sleeping Monstermunch keeps its eyes shut. It wakes when Leon
comes near and watches him (nearer still, but not to be had, its mouth opens a little and it
drools: `monstermunch_drool_*`); out in front of it and within reach, it throws its tongue at him, whatever lies in the way --
off ice, its tongue bounces as Leon's does, as a ball off a wall, and flies on -- bounced, it
catches nothing; missed (met something, him after a bounce, or ran out), the tongue goes numb, limp, its pink turning red and purple --
one piece, one outline, from its root in the mouth out through its lips, lying on the ground with
all its thickness above it -- and must be swallowed back in chews, as with Leon's RT. The base interval is 0.2 s at the configured five chews per second, with jitter from `monstermunch_numb_chew_jitter` and a minimum interval of `monstermunch_numb_chew_time`. Once its tongue is back it can throw again. Leon standing
on its face, it has him at once -- unless it is swallowing its numb tongue: then after
`monstermunch_stand_time` (0.24 s), going on with that as it eats him. It drags
him into its mouth -- a V cut into its face from the open side, empty -- see-through -- the suit cut in two by it, each half
turned with its side, teeth inside it along both sides, staggered like a saw's, each row hidden where it crosses the other side, its back always on the far border -- and munches him six times in one second with the current settings (he shows in
it till the first munch), blood cubes popping out of its middle -- seen through the mouth, behind
its face and teeth -- and saliva flying, both falling back into its place, to be seen only in the
blank of its mouth, between the teeth (its drool too, chewing a numb tongue); he can do nothing. His tongue,
out as it caught him: in flight, gone at once; numb, it goes down with him, a share at each munch, all of it by
the last; stuck, it holds against the first two munches -- pulled taut -- then gives way, whips aside, numb,
and goes down in the munches left. A fly in his belly goes down with him too, and `monstermunch_frees_flies_delay` after the last munch flies out of its
mouth -- a colour fly home to its pole, a lamp's golden fly to the nearest lamp (`monstermunch_frees_flies`; False: let go
as he is caught) -- slowly, wandering, an active Brownian particle (`monstermunch_freed_fly_speed`, `_noise`). A colour
fly in him, the Monstermunch's white (its face and teeth, not its eyes) takes the fly's colour over the munches, whole
by the last, and fades back to white once the fly gets out (`monstermunch_color_fade_time`; 0: at once). Dead, he is chewed on a little, drooling, for two
seconds; the screen fades out and he comes back at his last checkpoint (`monstermunch_*` in
`params.txt`) -- a fly out of it still wandering then goes straight home with the black (put there if far from it).
The cameleon level (`workshop/specs and corrections/level - cameleon.jpg`; laid out as seen with gravity up, as in the purple key's room -- in
the map, above the colour room, the map grown 56 rows at its top for it): from the colour room's corner (bottom left on the
screen), a corridor under the ice room's, then stairs down into a room 36 x 11 -- its floor dark matter, poles of the three
colours on little platforms (a balloon's and a fly's each), an upright bar, a Monstermunch in its wall and three in its
ceiling --; out at its bottom left, along to a shaft down -- a gravity block (down) at its top, three Monstermunchs in its
wall, ledges topped with dark matter, a yellow fly's pole out of its wall -- into a diamond 15 tiles wide and high
(Monstermunchs at its corner and on its lower sides, a yellow balloon pole in its bottom corner), the purple door (`q`: the
purple key opens it -- "Purple key !" said only with one in the level, as the others) at its right corner; on through two
U-shaped tunnels (a Monstermunch over each, a balloon pole hanging from it, fly poles at their feet), a heart-shaped room,
an oval room lined with dark matter (gravity blocks framed in ice, an ice bar and an ice block spiked on one side, a yellow
balloon pole), and the end room (`workshop/specs and corrections/level - cam mode - end move.jpg`): up its left leg (a Monstermunch under the way
in, a yellow balloon pole beside it, and past it a little room with a yellow fly's pole, to be reached with the tongue off
the ice above), along under ice (a Monstermunch in it; a yellow balloon pole on the ice below), down its right leg
(gravity blocks at its top and on its wall), back along a floor of dark matter; there, a pit 7 tiles deep (ice at its bottom), a corridor
35 tiles long -- a shaft over it, ice down one side, a magenta fly's pole out of the other, a Monstermunch at its top, a
magenta balloon pole under it -- and the violet flag in a room 7 x 7. The flag up, the music fades out (`music_fade_out`)
and stays silent: on from that room, a corridor 10 tiles long to the golden key -- gravity blocks down from the key
on, floor and ceiling, to the corner under the shaft, the floor under the shaft and 4 tiles up its far wall too, so
that whatever his gravity on the way it is down there, and the shaft after the key goes up -- (`o`: yellow going
orange, a golden halo
round it breathing from half its length to one and a half, `gold_key_glow_period`; held, it glows on, top left -- no mode,
no clock), 4 tiles more, and a shaft up through the top of the level, into a hall: 8 tiles left and right of the
shaft's mouth, two rooms 5 tiles high inside -- the left one 20 wide, the right one 55 --, walls a tile thick, the hall's roof 3 tiles above theirs (its walls raised to
it), each with a golden
door (`O`) in the wall facing the shaft: the golden key opens one, whichever it meets first -- then it is gone, and
the other stays shut. Near one, its name -- the left one "Love door", the right one "Solo door" -- fading in by where
Leon is: none 5 tiles from the door (`golden_door_name_distance`, any way), whole 3 tiles nearer
(`golden_door_name_fade`); fixed in the air over the third tile before it on the hall's side (as high as over his hat
there), each letter in the title's rolling rainbow, a wave swelling them one after the other: a gaussian bump up to
`golden_door_name_zoom` (1.5) times their size, `golden_door_name_wave_width` letters wide, crossing it in
`golden_door_name_wave_time` (1 s), then `golden_door_name_wave_pause` (1 s) at rest. A golden door open (either), both names
fade out over `golden_door_name_fade_out` (2 s), for good (golden doors never close: till the game starts over). Through a golden door, open (his centre in its doorway, either way), Leon takes its colour, changing to it over `leon_color_change_time` (3 s): the Love door, pastel pink (`leon_color_love`) in a regular game or 33% grey (RGB 85, 85, 85) in Solo, his whole hat taking the same colour; the Solo door, gold (`leon_color_solo`), his hat as ever (white and the key's colour) -- his own colour from then on (no glow, unlike GOD's gold), a colour fly in him tinting him as ever. Nothing else changes it -- a golden run's start, a death: kept -- till the game starts over. The left room's far wall is open from the ground 4 tiles up: outside, for the first time -- open
air over the whole level (29 rows of it at the map's top, walled at its two ends). On the hall's roof, a pyramid --
hollow, its two sides stairs of single tiles corner to corner, its top six tiles wide -- and on that top a tower, four
tiles wide (a tile's ledge each side), thirteen high, up to the map's top row. In the right room, 8 tiles short of
the pit, the black flag (`b`) -- black no more: a rainbow flag, six upright bars as wide each, pole to tip round the
rainbow, rolling along it once up (`title_scores_rainbow_speed`) --, furled: the golden run's end -- touched, the golden run's highscores show by it, under
its name, no name typed (not a checkpoint -- till a golden run ends by it). At the far end of the right room's
floor, 3 tiles short of its wall, a pit 28 tiles deep: the snail -- a square spiral, 21 x 21, of corridors a tile wide, walled with ice gravity
platforms each pointing the way its corridor runs (a corner's end wall: the next corridor's way).
The first right turn at the foot of the shaft knocks off Leon's or Anna's hat. It bounces loose and can be picked up again, as after a Monstermunch catches them. Leon falls round and
round without a stop, the world turning at each corner, into its heart, 7 x 9: a snail too, its corridors winding in (right, up, left,
down, right, up as the screen shows it, gravity up), walled with ice gravity the same way -- no rock in the snail --, its last one
ending on a dark light, the three tiles over it pointing on (nothing turns him there): landing in
it, he fades out in a fifth of a second (`dark_light_fade`) and is gone.
The golden key taken -- the first time -- the modes' clocks fade out (`golden_clocks_fade_out`, 2 s).
Then the golden run: the screen goes black (`golden_fade_out`, 1 s); the world is put back as at the very start --
every key in its place, every door shut but the golden one the key went into (the next golden key can only open the
other), the fruits back -- but bare of help: no flags but the black one (furled again), no hat anywhere (not on him, not at its spot), no golden fly
at the lamps (the dark is dark), no banana, kaki or double cherries, and the double jump's and the tongue's fruits
say nothing -- and comes back on the start (`golden_fade_in`, 1 s). Over it, a dark light (`golden_light_height`,
3 tiles up): Leon, facing right -- his colour kept (gold: the Solo door was his way in) --, shows in it (`golden_leon_fade_in`, 3 s) as it shows (`golden_light_fade_in`, 2 s),
then drops, and his clock -- SOLO RUN, the only one, its letters in the title's rolling rainbow -- fades in
(`golden_clock_fade_in`, 1 s); landed, the light fades out (`golden_light_fade_out`, 2 s) and, his shape settled
(`golden_countdown_wait`, 1 s), 3, 2, 1, GO counts, as after Shift+Ctrl+R -- but his eyes wide open, not shut. No control of his from the black to GO.
From GO, the clock runs and the start is his one checkpoint (no pineapple shows): keys still open their doors, and only the Solo clock and highscore count. The golden key fades the music out. Opening the Love door during Solo, or crossing it already open, keeps the tower route silent. The tower is empty and can be crossed freely. Reaching it leaves the player in control and Solo continues.
Before the black flag, dying, falling off the map, or reaching the time limit starts a fresh Solo attempt with the same character and the 3, 2, 1, GO countdown. The golden key is only a key in it (the left golden door's): the run goes on, up the shaft, through
the right golden door -- open still -- along the room, to the black flag. Touched, it goes up and stops the clock:
`SOLO RUN COMPLETED !` appears at the top of the screen and the current character’s five Solo times appear at the bottom while near the flag. A qualifying time in an eligible run opens initials entry there. The raised flag becomes the checkpoint. (Into
the snail again: another golden run, the black flag furled again till its end -- each one done, its time against the
highscores, as many as are run before the game is closed; none in debug mode, as ever.)
The regular game's ending (`workshop/specs and corrections/ending (draft).png`), through the Love room, up the tower: on its top -- the level's highest
tiles, the map's side walls aside -- Anna waits on its last tile, his love and an almighty sorceress: a round blue body
(`anna_color`), his eyes, following him wherever he is, a tall cone of a sorceress's hat (its ink finer than the brim's: 0.7 of the 3 pixels it was) on a brim of ink curled up at both ends (`anna_brim_size`, 0.8), over her head by the outline's
thickness -- and no mouth. Over the top's second tile, an invisible ice wall, `ending_wall_height` (40) tiles up (above the map: never
drawn). Touched, Leon is no longer his to move -- no run, jump, tongue, look -- and falls where he is, onto the
top, facing her; his tongue, out (stuck, flying, limp), is swallowed at once, reeled in as the tongue key does it, and
on the top, the tongue in, he turns to her, standing as ever. Landed, face to face, their bodies glow -- from their own colours to the ones they would have were they
see-through, `ending_glow_dim` (0.2) of the paper showing (opaque all the same: nothing behind shows), and back, every
`ending_glow_period` (2 s); their hats as ever. `ending_smile_wait` (1 s) on, the two smile: each smile shown from its middle out, not
stretched, over `ending_smile_time` (2 s). Then hearts (each one outline, its sides straight from the point into the round lobes, unbroken -- a true heart --:
it fades evenly, as do the Game Over's two), between the two, for good -- `ending_heart_rate` (6) a second,
each an active Brownian particle (`ending_heart_speed` its way, its heading wandering by `ending_heart_turn`, drifting
up at `ending_heart_rise`), fading out from `ending_heart_fade_from` (2) to `ending_heart_fade_to` (3) times the
distance between the two, from their middle. `ending_happy_wait` (2 s) on, both happy: eyes ^^, swaying left and right
together -- `ending_sway_amplitude` (12 degrees) each way, `ending_sway_frequency` (0.375) times a second, about their
bodies' middles, lowered just enough that no leg has to stretch, their feet planted -- flat on the ground, held still, not turned with the sway --, their knees giving, both towards the other --
and their hats' brims a party horn: uncurling in `ending_brim_uncurl_time` (1/6 s; flat, Anna's spirals a long straight
line each side), curling back in `ending_brim_curl_time` (0.5 s), at rest `ending_brim_pause` (1/3 s), again -- 1 s in all, two beats of the Love room's music (120 a minute) --, each
move speeding up and slowing down over `ending_brim_ease` (a fifth) of its time; with `ending_brim_music_sync` (True), each uncurling
starts on a bass note of the piano -- `ending_brim_music_beat` (0.975 s) into the music, then every second: its time read
from the music playing (smoothed over the mixer's 10 ms steps); the first cycle waits for one; the music off, by the clock. `ending_pan_wait` (5 s) on, the scene slides left as the right stick looking right
would, over `ending_pan_time` (2 s), till Leon is `ending_pan_leon_from_left` (3) tiles from the screen's left. Then
the credits roll up from the bottom of the screen over its right two thirds, each line centred there
(`ending_credits_speed`; down held -- the left stick, Down --, `ending_credits_fast` (4) times as fast, getting there
and back over `ending_credits_fast_ramp` (0.5 s)): `assets/endcredits.txt`, read as the game starts -- `# ` a title, `## ` a subtitle, any
other line normal, an empty one a gap; `**bold**`, `*italic*`, `***both***`, `{pink}a colour{/}` (by name, or
`{#rrggbb}`); `^^` a red heart as tall as a capital (`ending_heart_color`, as the ending's hearts; `\^^` the marks themselves); `![0.5](assets/making-of/level - easy hard.png)` a picture (PNG) on a line of its own, centred, that share of the two thirds'
width, as tall as its proportions make it; `<!-- ... -->` left out. Gone, `ending_credits_wait` (3 s), the scene scrolls away left,
on and on, as the right stick looking right would with no bounds -- `ending_scroll_speed` (8 tiles a second), reached
over `ending_scroll_ramp` (0.5 s); the tower, Anna (her brim flat and all) and every heart within the screen's height past
its left edge, the screen fades to black (`ending_fade_time`), scrolling still -- the music on -- and, as the questions show, "Game Over" between two red hearts as tall as its
capitals, under it, smaller, "Go to menu (Joypad A or Keyboard <space>)", its commands in colour as a lesson's: A or
Space, the title. (A bug report keeps where the ending is: `ending:` -- and the seconds the credits gained, when the scene went off.)

### Anna

The game always starts with Leon's. His game finished -- its Game Over reached --, the title becomes Anna's:
GRAVITY ANNA, ANNA's letters a rainbow rolling through them (`title_anna_rainbow_speed`), Anna where Leon stood,
and her game is played: Leon's in every way -- the level, the keys, the modes, the doors, the colours (white; the
Solo door's gold, the Love door's pink or Solo grey), the checkpoints, the clocks -- but for her drawing, her hat, her dash and
her start; in the regular game Leon waits on the tower's top, in Anna's place -- blue as she is there (`anna_color`), his hat too --, and does there all she did (his eyes on her, the glow,
the smile, the hearts, ^^, the sway, his brim a party horn), she all he did. Hers finished, the end reads "GAME
100% OVER" between two hearts a side; then the title shows the two face to face where he starts, smiling, as on
the tower's top -- each as when played: white, his hat banded, hers a checkerboard (no grape there) --, its name
GRAVITY LEON and GRAVITY ANNA in turn, `title_name_switch_time` (4 s) each, the one morphing into the other over
`title_name_morph_time` (0.5 s) -- letter by letter, left to right, each fading into the other's, squeezed a
little, the title's size going from the one's to the other's --, and "Press Y/<Y> for Leon, X/<X> for Anna" --
the pad's Y in yellow, its X in blue, each in its
circle, the keyboard's keys in blue: Y starts Leon's game, X Anna's, A nothing. From then on any game finished
reads "GAME OVER (AGAIN)" between three hearts a side, and goes back to that title. What has been finished counts
till the game is closed -- launched again, it starts with Leon's game (a game finished in debug mode counts too).
- **Her start**: her hat on, her tongue and her double jump from the start -- no fruit to eat for them, nor the
  lessons' (the banana, the kaki, the cherries): none of them in her game, the hat's spot empty. Instead, 4 tiles
  left of where she starts, a grape (a purple bunch). Eating it shows the dash lesson. She can already dash from the start. The grape is absent during a Solo Run.
- **Her drawing**: the tower's Anna, played -- white as he is (gold through the Solo door, pink through the Love
  door in a regular game and grey there in Solo), her round body in the cube's place (36 × 27 pixels for the drawn cube, a shared 30 × 38 pixel hitbox for movement), her
  face and hat at 0.82 of the first drawing's size, her thinner legs (their thighs reaching up into her round
  bottom: no gap as her knees bend) and smaller feet; walking, jumping,
  pedalling, knocked out (an egg rolling, eyes crossed), getting up, her mouth split along the tongue as his (her body `anna_mouth_body_scale`, 0.9, of its size while it is
  open -- easing in --, the cut `anna_mouth_height`, 0.4, of her height from her bottom: a small chin; each eye on
  its own jaw, moved off the mouth's line if it is too near, wholly on her body), her
  lids straining at a block -- all as his. Waiting on the tower's top in his game, she is the same Anna, the same
  size, in her own colour (`anna_color`), her hat too, all of it.
- **Her hat**: the cone, its four bands a checkerboard -- each split down its middle, the last key's colour (black
  before any) on one side, white on the other, the other way round from band to band; through the Love door,
  pink all over in a regular game or grey all over in Solo. It is his hat in all it does (worn,
  knocked off, fetched with the tongue), its four bands folding as his bellows does: from the top down, each band
  all the way before the next, to `anna_hat_squashed_height` (0.15) of its height; held folded
  (`hat_squashed_time`), then popping back up one by one from the lowest folded, `anna_hat_pop_delay` (0.25 s)
  apart, each in `anna_hat_pop_time` (0.1 s), overshooting a little. Under a ceiling -- a ledge she walks under,
  a low corridor, a jump up -- it bends over, back from where she goes (swinging over as she turns), as far as it
  must to fit, up to 75 degrees at its tip; past that, its top bands fold, down to a line. Its curls are pressed
  flat (on a bump too, standing up again as his brim springs back); against a wall aside, the brim's end bends
  up along it as far as it must. At respawn the game measures the walls and ceiling at the checkpoint before the countdown is drawn.
- **Her dash** (RB, LB or releasing Shift) follows the direction she faces along the ground, as drawn in `workshop/specs and corrections/Anna dash.jpg`. Using Shift for another game shortcut cancels the pending keyboard dash.
  Wound up first, still where she is, over `anna_dash_windup` (0.1 s), through three poses: her knees bent, her
  body tilted back 15 degrees (nose up), the cone tipped forwards, the brim behind flattening, the one in front curling; the cone whipped back, her body
  stretched, her legs going back; long and flat, the cone lying behind her, its front curl tight, her eyes at her
  front, her legs trailing. Then off, so -- a little cloud left where she was, on the ground --,
  `anna_dash_distance` (4) tiles in `anna_dash_time` (0.10 s). Then stopping, over `anna_dash_recover` (0.35 s):
  the cone thrown forwards, her feet ahead, skidding (a little dust behind, on the ground); the cone bent flat
  forwards, her knees bent, looking down; the cone springing back up, a squat; as ever. Gravity held off from
  the wind-up to the end of the dash, nothing hurts her meanwhile, no dark matter, no Monstermunch;
  standing, or `anna_dash_air` (1) in the air before she stands again; `anna_dash_cooldown` (0.10 s) between two;
  not knocked out, not in the ending, her hat not on her tongue; the tongue out, it is swallowed meanwhile, quickly,
  as she winds up (stuck, flying, limp). Out of it, running. Into a wall, it stops -- not knocked out, but her
  eyes shut 0.5 s, open 0.2 s, shut 0.1 s, open again; into dark matter as into a wall, and once the dash is over, it
  is as deadly as ever. She is blurred along her way, `anna_dash_blur_length` (30 px) behind her, the blur
  coming in over `anna_dash_blur_in` (0.05 s) and going over `anna_dash_blur_out` (0.08 s) after the end. Her
  eyes narrowed all through it, wind-up and stop too -- as wide, `anna_dash_eyes` (0.6) of their height.
  (Bent or tipped, her cone's base stays flat on its brim: no corner sunk into her, no gap under it.)
- **The highscores**: hers apart, in `data/highscores_anna.txt`, kept for good (the flags' tables in her game
  are hers) -- but shown on the title only once Anna is in the game. On Anna's title and the two's, LEON'S HIGHSCORE and ANNA'S HIGHSCORE in turn -- `title_scores_switch_time` (6 s)
  each, the one scrolling away left as the other comes in over `title_scores_scroll_time` (0.5 s), easing off
  and in --, the one playing's first; each with its SOLO RUN once that one has completed a golden run. Leon's
  title, before Anna's game, is as it was: HIGHSCORE, his alone.
- **Debug mode**, the two not both finished: on the title, Shift+Enter (or LB and RB together) switches between
  Leon's game and Anna's -- who shows on the title, who plays --, out to black and in again over
  `title_switch_fade_time` (0.6 s).
- A bug report records who plays (`player: anna` -- none in older reports: Leon).

### The engraving

`look = engraving` (or Shift+S, at any time): the world drawn whole, then cut into an engraver's lines on paper
(after Gustave Doré) -- the fruits, the keys, the doors, the flags, the golden flies and the colour clouds keep
their colours as drawn; the counters (stars, keys, clocks) straight on it, no paper under them. Leon or Anna -- tongue, hat, on the head or off --, the one waiting on the tower's top (and
the two on the title) and the Monstermunchs are engraved as the rest, but hand-coloured, as an old print: the same
lines, a little lighter, their own colours washed in under them. Melted into a colour cloud, Leon or Anna is the
cloud's colour where it covers him, as in the sketch -- his eyes only, in a cloud of the colour he glows --, and
engraved, hand-coloured, wherever it does not.
Its sky's clouds are the workshop's cumulus (`workshop/specs and corrections/nuages-2.jpg`), redrawn as an engraving -- its ink
cleaned and made a burin's, paper inside, level hatching in its shade, closer and heavier low down, crossed where
darkest -- by `tools/make_clouds.py` into `assets/clouds/`; in three layers, far to near, sliding with the camera
and drifting in the wind (`engraving_cloud_wind`), mirrored or not, with much open sky between them. (The tool's
`--all` also makes the two other drawings' -- a billowing cloud, a streaked one --, taken out of the game.)
`engraving_cloud_drawings = False`: the former clouds, engraved by the sky's shader.

### Flags and highscores

A flag goes up only if its mode ran -- its key taken in the level (not given by a debug key):
touched otherwise, it stays furled, a checkpoint all the same, and its mode's highscores show by it
under the mode's name.
Each flag up says so by it -- EASY, HARD, DARK, HARDER, CAMELEON MODE COMPLETED, its letters riding
a wave (`goal_text_wave_amplitude` letter heights, `goal_text_wave_period`) -- with that part's five
best times at the bottom of the screen, under HIGHSCORE for the current character, all of it in the
title's rolling rainbow. A time good enough goes in, in bold: type
three letters on the keyboard while the game goes on (the letter keys write the name meanwhile;
the pad, the arrows and Space still play), Backspace takes one back, Enter keeps it. The next
highscore of the same game offers that name again: Enter (or nothing) keeps it, a letter typed
starts another. Enter or walking away saves the time and fills missing letters with `?`. Esc saves the time as `???`. Returning to the flag during the same run reopens the score for editing if it is still in that character’s table.
None with a `start_with_*` on, `start_level` past easy mode, or after a debug key: the tables still show at the flags, no name is asked
-- and DEBUG MODE shows in the bottom right corner, in a rainbow rolling along its letters; in the
bottom left, the game displays FPS twice a second. Native builds also display CPU usage as a percentage of one core and memory in MB (physical footprint on macOS, resident memory on Linux).
The title shows each character’s mode tables side by side. Their sixth column, SOLO RUN, appears when that character’s table contains a `golden` entry or they have completed a Solo Run during the current session. A debug completion reveals the column for that session. The rainbow rolls down the rows at `title_scores_rainbow_speed`.
Left alone for 3 seconds, an arrow at the edge of the screen points the way: the next checkpoint
along the level's path (lit or not: the yellow flag, then the red key among them) -- in hard mode,
the pineapple it keeps (by the tongue fruit, to be eaten again), then the red flag -- never easy mode's, done with.
The keys' colours -- their doors' and their lessons' too -- are `key_color_red`, `_blue`, `_green`, `_violet`, `_golden` (R G B A): red brick, grey blue, camouflage green, violet, golden (the flags keep their own). The stage keys are red for hard mode, green for dark, blue for harder and purple for cameleon. Each stage has its own checkpoints: a pineapple the start reaches, every
door shut, is easy mode's; one beyond a door, the first stage whose way (its key to its flag) passes it. From the green
key on, a pineapple of an earlier stage never lit is gone -- never shown, never pointed to, not to be eaten -- and the
arrow follows the current stage’s pineapples in order of path distance from its key, then its flag. Dark and harder modes also point to the next stage key. Cameleon’s route ends at the violet flag. Taking the golden key hides the arrow, as does entering a Solo Run.
Back at a checkpoint, Leon faces where the arrow points (where there is none, as he was) -- at the golden key, right: the way on.
Blocks are dragged by sticking the tongue on them and walking away, or pushed by walking into them. Dark matter is deadly.
Back at his checkpoint with a block where he stands (dragged or pushed there), it is lifted two tiles up -- his way -- and
falls back (onto him: it tips off); no room up there, it goes two tiles to his right, else to his left; none, back home.
Out of the map (12 tiles past its edge, whichever way), Leon stops there, the world fades to black (`death_world_fade`)
and he is back at his checkpoint, as after a death -- no pieces (an unfinished Solo Run starts again with a fresh clock and countdown; a finished Solo Run returns to its raised black flag).
His pieces -- and a Monstermunch's blood cubes -- bounce and come to rest on their sides, their
whole thickness above the ground.
Back after a death, the screen fades in (`death_fade_in`); dead again before it is over, the fade in
plays out first -- his pieces flying meanwhile -- then the new death fades out as usual.
Touching a black gravity platform (its arrows running slowly the way they point, an endless stream fading
in and out at its ends: `gravity_arrow_speed`, `gravity_arrow_border`) turns gravity towards its arrows, and the screen turns with it;
the down arrows put things back. Shift+R restarts the right way up. A quarter turn begun on the ground (or landing on it): his
box turns at once, narrower than tall, and the swinging pull presses it up to 4 px nearer that ground, while he is drawn
turning later -- so he is drawn lifted back by as much, less as he turns, his feet on the ground's line, never sunk in;
and drawn standing on it till he turns, his legs straight -- his speed along it (a run, a jump's), rising or falling in the
new frame, neither tucks them nor swings them yet: landing on a gravity tile, his feet meet it.

## Files

- `data/highscores.txt`, `data/highscores_anna.txt`: Leon's highscores, Anna's.
- `assets/clouds/cloud-cumulus.png` is the engraving cloud used by the game. From the project root, run `python3 tools/make_clouds.py` to regenerate it from the photographs in `workshop/specs and corrections/`. The script requires NumPy and Pillow. Use `python3 tools/make_clouds.py --all DIR` to write the cumulus, billow and streak PNGs to a chosen directory.
- `data/params.txt` holds tuning for running, jumping, the camera, audio and other systems. The local executable checks for edits every half second. `start_level` takes effect when a run starts or restarts. EASY MODE starts at the normal spawn. HARD, DARK, HARDER and CAMELEON MODE start at their stage key with the earlier powers, doors and music set for that point.
- `levels/level 01.txt` is the shipped map. The `level` setting selects a filename in `levels/` without the `.txt` extension. `#` is solid ground, `=` a thin platform (Down + jump allows passage when `drop_through` is enabled), `P` the start and `*` a star,
  `>` `<` `^` `v` gravity platforms (one arrow direction each), `)` `(` `n` `u` ice gravity platforms (right, left, up, down: the
  round side shows the way -- gravity platforms the colours inverted, white with dark arrows, square-cornered as walls and without an outline of their own (the rock keeps its line along them), that no tongue sticks to: it bounces off them as off ice; Leon does not slide on them),
  `]` rightward ice gravity that knocks off a falling player's hat at the Solo pit's first bend,
  `@` a dark light (a light the other way round: darkness round it, blackest at its heart, `dark_light_radius`, `dark_light_depth`, wavering as a lamp's light; in it, only Leon's eyes show;
  his middle reaching its heart starts the `dark_light_fade` and the transition into a fresh Solo Run), `~` slippery (the tongue bounces off
  it, as a ball would; sticking farther on, it holds slack -- all it ran out -- till swallowed taut;
  holding nothing, it goes dead, limp, never swallowed by itself: RT/V a chew at a time, or the tongue key reels it in; Leon slides on it), `X` dark matter (deadly), `B` a block to drag with the tongue (its top-left tile, sized by `block_size`),
  `A` a pineapple: a checkpoint (touch it or catch it with the tongue; reached, it lights up and immediately
  starts fading out over 2 seconds -- gone from sight, still his checkpoint; `a`: the same, but back there he faces
  right, his right as the screen shows it, whatever the guide arrow), `D` / `T` the double-jump /
  tongue fruit (a level holding one starts without that ability), `F` a fly (a level with none and
  no lamps has `fly_count` anywhere), `E` the exit: the red key, floating at the middle of the `E`
  tiles (taken: hard mode), `H` the hat, `h` a spare-hat spot (his hat lost -- knocked off
  as he was eaten, with `hat_back_on_respawn` False --, still lost as he comes near one: it waits there), `C` a double cherry (eaten, it tells how the tongue is swallowed back or paid out),
  `Y` the yellow flag (the end of easy mode; a level without one ends it at the red key),
  `R` the red flag (the end of hard mode, the way back; a level without one ends that way back at the start),
  `L` a locked door (a column of `L` makes one door: a wall until Leon touches it holding the key),
  `.` dark air (open; going down it, the light fades -- `dark_depth` -- until only Leon's eyes show, round -- never the heavy lids of a block's weight;
  dead there, his pieces still show faintly -- `dark_pieces_light`),
  `I` a lamp post (a pole two tiles tall on the ground under it -- or, with only a ceiling over it,
  hanging from that; with neither, sticking out of the wall beside it; its golden fly circles the disk at its end and lights the dark, `lamp_light_radius`;
  swallowed -- glowing in his belly meanwhile, flickering, `lamp_belly_light` of its light (between a post's and a fly's; `lamp_belly_flicker_amplitude`, `_frequency`), for `lamp_belly_time` (moving does not cut it short) -- it circles Leon for `lamp_follow_time`, then settles on the nearest lamp, which it lights;
  when he dies -- whatever the death -- every lamp's fly goes back to the post it started on),
  `i` a lamp post with no fly yet,
  `1` `2` `3` a balloon pole (`workshop/specs and corrections/colorpole.jpg`; colours `color_1` .. `color_3`, colour-blind safe: blue, magenta, gold): a black pole two tiles tall,
  standing as a lamp post does, growing a balloon of its colour -- a real balloon's shape, longer than wide and rounder at the top, upright against
  gravity, paler as it swells, pushed over round its knot by a wall in its way (righting itself once free); a fresh one swings on its knot, fast
  and wide, slower (as the square of its width) and less as it swells (`color_balloon_swing_*`); about to burst, its ink line goes dashed (`color_balloon_dash_time`, `color_balloon_dashes`, `color_balloon_dash_thickness`) -- that bursts into a cloud of gas:
  disks of the colour, paler still, as one (inked round or not: `color_gas_contour`), thrown out of the balloon's middle all round -- BOOM: most of the way in a few tenths of a second (`color_gas_boom`) -- slowing down; one meeting a wall is slowed
  by it, the deeper in the harder, and its middle cannot go in -- the gas piles up on surfaces (the walls cut it off where drawn); then it
  evaporates, each puff falling with gravity as it fades (`color_gas_gravity`, a share of Leon's) and jiggling (`color_gas_brownian`), settling on the ground, the cloud clearing
  before it is gone (`color_gas_opacity`, `color_gas_fade`); where clouds of different colours cross, their colours mix as inks or coloured glass
  do (subtractive); and the pole grows another balloon, `color_balloon_regrow_delay` later (`color_balloon_*`, `color_gas_*`) -- or, with
  `color_balloon_sync`, the balloons burst by colours -- all the blue ones in the level at once, then all the magenta,
  then all the gold, round and round -- each colour as the last one's clouds start to evaporate (their first puffs:
  0.35 of `color_gas_time` after the burst), the regrow delay following from that. No fly at it. The balloons start
  their round afresh with cameleon mode -- the purple key taken -- and whenever he comes back at one of its checkpoints
  (its key's, its pineapples', its flag's) -- back at one listed in `color_balloon_respawn_first` (`2:magenta`: the
  checkpoint numbered 2 in map order, as a bug report's `checkpoint.pine` gives it), that colour's balloons grow first,
  the round going on from there; at any other, and at the game's and the mode's start, blue first.
  `4` `5` `6` a colour fly's pole (colours `color_1` .. `color_3`): the same pole, no balloon -- the dot on its top its fly's colour, inked round thin
  (`7` `8` `9`: the same, sticking out of the wall beside it even with ground under it or a ceiling over it);
  its colour fly circles that dot, as a lamp's fly its lamp: caught with the
  tongue, it is swallowed whole and stays in his belly `color_belly_time` (0: for ever) -- his body tinted its colour, his hat's white too, his tongue its colour instead of pink, glowing
  -- then he spits it out, it circles him `color_follow_time` (0: not at all), then flies home to its pole (another one swallowed: the first spat out; dead: home at once; his hat knocked off as he is eaten keeps its colour -- tint and clouds -- as long as
  the fly had left in him, then it is white again). In a cloud of another colour, he is seen through it, his colour and its mixed so. Where a cloud of the colour he glows covers him, Leon melts into it, near-invisible (`color_hide`) -- tongue and all -- and the
  Monstermunchs cannot see him (none wakes, an awake one loses him and dozes off, none throws; standing on its face, he is had all the same)
  -- till the cloud has been clearing `color_gas_hide_time` (then, not hidden enough, he is seen); the cloud
  shows in his place, never what is behind him -- but the ground's ink line under his feet, in front of the clouds, shows through them (not notched by them); the rest of him whole, tinted, his eyes whole -- and so is his hat (`color_hide_hat`), on his head or in such a cloud off it: knocked off, on his tongue, at its spot; with no
  colour fly in him, where a cloud of any colour covers him, he shows `color_hide_plain` (his hat as much); where a cloud -- any colour -- covers a pole, the pole shows `color_hide_pole` of itself (the cloud in its place, the pole back as it evaporates); a balloon growing in a cloud (the last one's not gone yet) shows `color_hide_balloon` of itself, as much as the cloud covers it; his glow fades as the middle of his
  cube goes in,
  `U` a banana (eaten, it says "Swallow the fly !"), `Z` a kaki (eaten, it
  says "Look down !" -- Right Stick or Alt+Down -- a little before the first cliff of level 01),
  `K` a blue key and `l` a blue door (only the blue key opens a blue door, only the red one a red
  door; taken: harder mode), `k` a green key and `g` a green door (the same, in green; taken: dark mode), `p` a purple key and `q` a purple door,
  `o` the golden key and `O` a golden door (the key glows; it starts no mode and no clock, and opens one golden door, the first it meets),
  `M` a Monstermunch (its top-left tile; 2x2 tiles, solid, dug in the ground -- a floor, a ceiling, a wall: it faces the open side),
  `G` the green flag (the end of dark mode), `J` the blue flag (the end of harder mode; the music
  changes), `V` the violet flag (the end of cameleon mode, which the purple key starts). A flag goes
  up when touched if its mode ran, and every flag and key is a checkpoint -- but `b`, the black flag (a rainbow flag, the end of
  the golden run: up when touched in one, and his checkpoint then; its highscores by it; no checkpoint otherwise).
  Blocks never move before Leon has touched them (his body or the tongue). Press Shift+R to reload it.
- `assets/music/03 - tno - Tartopom.mod`, then `08 - tno - World Of Pistou II.mod` from the red flag up and
  `18 - tno - Dying Respect.mod` from the blue flag up (each change: fade out, silence, fade in --
  `music_fade_out`, `music_gap`, `music_fade_in`), and none from the violet flag up (it fades out) -- nor once a golden key is taken (it fades out
  with it); then, a golden door opening, its own: the left one (the Love door) `niko - palindrome au piano.MP3` in a regular game, silence in Solo,
  the right one (the Solo door: the solo room) `nocomp - Power Plant (Ultimate Fan Mix).mp3` (the latest of these events wins: a golden key taken, silence again);
  till the game starts over. The music, looping (the modules played by the game's own `.mod` player,
  `src/modplayer.cpp` -- raylib's loader hangs on modules that loop back on themselves; the MP3s by raylib's). The sound
  effects are synthesised at startup (`src/audio.cpp`): no sound files. His steps: a soft thud, very short (dull noise and a low knock), quiet. `music` / `sound` and their
  volumes are in `params.txt`.
- `Reports/`: bug reports filed with Shift+B -- `<date>_<time>_[N_]open.txt` (the text, then the
  game's state and a copy of `params.txt`) and `.png` (the picture). See `CLAUDE.md`;
  `Reports/guide_for_AI.txt` explains the folder to an AI assistant starting from scratch.
- `assets/gamecontrollerdb.txt`: SDL pad mappings (an unmapped pad stays frozen under GLFW): the
  Xbox Wireless Controller. (The Xbox Series controller on its cable sends only Microsoft's own
  protocol, which no mapping covers: on macOS the game reads that one itself.) A pad's GUID holds its
  firmware version: the game logs any connected pad with no line ("INPUT: pad ... has no line"), and
  `./gravityleon --pad-check [SECONDS]` prints each pad's GUID, then, live, what it sends.

## Smoke run

`./gravityleon --smoke-name NAME [--smoke-time S] [--spawn X,Y] [--smoke-double] [--smoke-jumps N] [--trace]` plays a
scripted run (run right, hop; `--smoke-double` hops again in mid-air), saves `screenshots/YYYYMMDD_HHMM_NAME.png` and quits. `--spawn` takes tile coordinates;
`--smoke-jumps N` stops the hopping after N jumps; `--smoke-run -1|0|1` sets which way it runs;
`--smoke-tongue T,DX,DY` presses the tongue at T aiming (DX,DY), and can repeat; `--smoke-god T` presses Shift+G (GOD on / off) at T, and can repeat; `--smoke-chew T` pulls RT; `--smoke-pay T0,T1` holds LT;
`--smoke-jump-at T` taps jump; `--smoke-jump-hold T0,T1` presses it at T0 and holds it until T1;
`--smoke-suicide T` holds the suicide combo from T; `--smoke-countdown T` presses Shift+Ctrl+R at T; `--show-title` holds the run on the title, `--smoke-start T` presses A there at T; `--smoke-esc T` presses ESC at T; `--start-level NAME` is `start_level` for that run (`5`: at the golden key, as cheat key P);
`--scores-file FILE` selects Leon’s score file. Anna’s adds `_anna` before its extension. A smoke run saves scores earned through play only when a score file is specified. `--test-score PART,SECONDS` injects a result directly into the score queue and can write to the usual player tables. Pair it with a temporary score path, for example `--scores-file /tmp/gravityleon-test-scores.txt --test-score 5,42 --test-name TST`. Parts 0–5 are easy, hard, dark, harder, cameleon and Solo respectively.

`--fly-at X,Y` puts a fly on a tile; `--level FILE` plays another map; `--abilities` starts with the double jump and the tongue; `--sketch` (or `--decor-off`) runs in the sketch look; `--engraving` in the engraving; `--from-report FILE` puts the game back in the state a report
recorded; `--reports-dir DIR`, `--test-report TEXT` and `--show-report-box TEXT` test the reports; `--grav F` starts in gravity
frame F (0 down, 1 left, 2 up, 3 right); `--spawn-px X,Y` starts with his feet at a world pixel; `--trace` prints each change of the knocked-out state.
`--anna` / `--leon` plays that one whatever has been finished (at the two's title, `--smoke-start` starts that one's game);
`--stage N` starts as if Leon's game (1), or both games (2), had been finished this session;
`--smoke-dash T` dashes at T (the way she faces); `--smoke-switch T` presses the title's debug switch at T.
`--seed N` draws the random numbers from seed N, so that two runs match pixel for pixel (the ending's brims still follow the
music's clock: `ending_brim_music_sync = False` for comparable runs); `--frame-log FILE` writes each frame's start and its work
before the swap (ms) to FILE at the end; `--no-vsync` lets frames come as fast as they can (benchmarks).
`--smoke-fullscreen T` presses Shift+F at T (and can repeat); `--smoke-resize T,W,H` makes the window W x H at T;
`--smoke-level-at T,FILE` loads another map at T, as `level` changed in params.txt does it live (load, then restart);
`--bg-fallback` draws the background pattern stroke by stroke, as it is drawn without vertex arrays or past its budget;
`--bg-probe` logs each build of the pattern's buffer, its full size counted even past the budget (16 MiB: past it, the
pattern is drawn stroke by stroke -- `background_pattern_scale` near 0.1, `view_zoom` far out).
`--audio-check DIR` writes the sound effects and the first 20 seconds of each of the three tracker modules to DIR as WAV files.
`--audio-muted` runs a test with the audio running at zero volume. Scripted runs are otherwise silent.
