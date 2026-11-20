# Gravity Leon — four fixes: implementation report

18 November 2026.
- Machine: Apple M3, macOS 15.8.
- Libraries: raylib 6.0 (dynamic for the normal build, static for `make dist`).
- Compiler: Apple clang `-std=c++17 -O2 -Wall`, the Makefile's commands. Every build: 0 warnings.
- Tests:
  - smoke runs at 1280×720 with `--seed 1`, plus fullscreen at 2560×1440;
  - each run from its own folder under `.scratch/optim/`, so the player's params, levels, assets,
    scores, Reports/ and screenshots/ were not touched;
  - compared against **A**, the pre-optimization reference build (plus test hooks only).

## Release

- **`dist/GravityLeon`**: sha256 `22cd14d5cf079bd7b182d5c4427def0822611c313888af0fc52cf9b9efe86f56`. A copy alone in an empty
  folder has the same hash.
  - The build is from the real `make dist`, with the current `params.txt`, `levels/` and `assets/`.
  - `embed_gen.py` draws a new obfuscation key each build, so the next `make dist` will have another hash.
- **`gravityleon`** (normal build): sha256 `0c57a4f5fe2313ab…`, byte-identical to the build tested here.
- The previous `dist/GravityLeon` (built 18:00, mid-work) is kept in `.scratch/optim/dist-previous/`.

## 1. Ending music in the standalone build

- **Changed:**
  - `src/audio.cpp`: `loadMp3` (new). `Audio::changeMusic` now loads through it, and its log line
    gives the frame count.
  - `src/embed.h`: `embed_data` declared.
  - `tools/embed_gen.py`: `findRes` and `cachedRes` shared by `embed_fopen` and the new `embed_data`.
- **How it works:**
  - In the dist build, an MP3 is streamed with `LoadMusicStreamFromMemory(".mp3", …)` from the embedded
    file's own decoded bytes.
  - Those bytes are owned by the embed cache and kept for the whole run. So they stay valid for the
    track playing and the one waiting, and through a replacement, a failed load, a music toggle or the
    exit. No copy is made.
  - The normal build still streams from disk (`LoadMusicStream(path)`).
  - Fades, gap, looping and the music clock (`GetMusicTimePlayed`) are untouched.
- **Reproduction (before):** a single-file build of the reference source, run from a folder holding
  only the executable:
  - "AUDIO: could not load music …palindrome au piano.MP3" (Love ending);
  - the same for …Power Plant (Ultimate Fan Mix).mp3 (Solo room);
  - no music clock.
- **Acceptance (after):** the same paths (Love ending fixture, Solo-room report 20261117_23h29m08s),
  from an executable-only folder, on a scratch dist build and on the real release copy:
  - Palindrome "loaded (73 s, **3,199,104 frames**)", then "music now …palindrome".
  - Power Plant "loaded (240 s, **10,584,000 frames**)", then "music now …Power Plant".
  - The music clock advances in both.
  - The brims follow it. In a run without vsync, the game ran about 15× faster than real time (one
    music second every 15–18 game seconds). Still, at each whole music second the brims stood at the
    same curl (0.924–0.930); on the game clock those samples fall at scattered phases.
  - The normal build still plays Power Plant from disk.
- **Limitations:**
  - Playback ran with the audio device at zero volume (`--audio-muted`), so it was not judged by ear.
  - Replacing a waiting track, a failed load and toggling music were not exercised one by one. They hold
    by construction: the bytes are never freed during a run.

## 2. Background mesh: bounded, no slower

- **Changed:** `src/main.cpp`:
  - new: `kBgVertexBytes`, `kBgBudget`, `kBgBudgetVertices`, `kBgSlack`, `bgMotif`, and the test flags
    `--bg-fallback` (`gBgStrokesOnly`) and `--bg-probe` (`gBgProbe`);
  - changed: `gBgMesh` (64-bit cap, peak, sent), `bgSend`, `bgTriangle`, `bgPatternTiles`,
    `bgMeshBuild`, `drawBackground`.
  - Also: `inkLine` and `drawBgPattern` (see the finding below).
- **How it works:**
  - Counts, sizes and offsets are 64-bit and checked before rlgl's `int`s.
  - A **16 MiB** budget applies while tiles are collected, so the CPU tile list is bounded too.
  - Within it: the cached buffer as before, with a quarter of headroom. It is re-made smaller when it is
    over twice its need plus 65,536 vertices.
  - Past it, or when a buffer or the tile list can't be allocated: the buffer is released and the
    pattern is drawn stroke by stroke by the original routine, unchanged.
- **Reproduction (probe, no allocation)**, at the level 01 spawn, upright, seigaiha. The sizes are what
  the old code would have allocated, with a quarter more:

  | Configuration | Vertices | Old size | Now |
  |---|---|---|---|
  | Default | 237,000 | 3,555,000 B (3.39 MiB) | buffer, 3.39 MiB |
  | Scale 0.1 | 16,159,608 | 242,394,120 B | stroke by stroke |
  | Scale 0.1, zoom 0.2 | 213,116,088 | 3,196,741,320 B (overflowed int) | stroke by stroke |

- **Acceptance:**
  - **Pixels match the reference (0 differing pixels)** in each of these:
    - the default size;
    - scale 0.5, in budget, with a 10.79 MiB buffer;
    - scale 0.1;
    - scale 0.1 at zoom 0.2;
    - a two-pattern crossfade at scale 0.1, which goes from a 3.94 MiB buffer to stroke by stroke
      mid-fade and releases the buffer;
    - the default crossfade.
  - **Live return to defaults** (params rewritten in a running game): from scale 0.5, the 10.79 MiB
    buffer is re-made at 3.39 MiB; from scale 0.1, a 3.39 MiB buffer is made and nothing larger kept.
  - **Peak mesh bytes:** hard mode standing and running 2,669,760 B (2.55 MiB); clouds 459,360 B; never
    above 16 MiB.
  - **Full suite**, 46 frames on both the buffer path and the forced fallback. It covers camera motion,
    gravity turns, fractional positions, all patterns and looks, credits, title, and fullscreen and
    resize with canvases. Result: **45 of 46 frames identical**. The one difference is the credits line
    "Flo^^", now drawn with a heart: the `^^` mark added before this task, not a regression.
- **Finding, fixed:** these tests found two pre-existing differences.
  - **Thin lines.** The earlier `inkLine` stretched its texture coordinates over the shapes' white texel
    in the font atlas. On lines thinner than a pixel (zoomed far out) a few edge pixels read the font
    beside it: 1,669 pixels differed at scale 0.1, zoom 0.2. It now reads that texel's centre, a
    constant coordinate, and matches the reference exactly.
  - **The fallback.** Leaving out off-screen tiles in the stroke-by-stroke fallback moved one sample in
    one frame, for a reason not established. That culling was dropped; the fallback is the original
    routine, byte for byte.

### Frame times — fresh

Hard mode standing, hard mode running, clouds. Measured in rotation A / Bh (the previous optimized build)
/ C (this one), with the same seed, fixtures, 1280×720 window, flags and build command, 600 frames per
run, the first second left out.

Without vsync, 5 runs each, median (p95 / p99) in ms:

| Scene | Build | Work before swap | Frame interval |
|---|---|---|---|
| Hard, standing | A | 15.78 (16.60 / 16.97) | 16.67 (17.30 / 18.12) |
| | Bh | 0.56 (1.55 / 1.63) | 1.16 (3.88 / 5.29) |
| | C | 0.56 (1.53 / 1.59) | 1.16 (3.98 / 5.36) |
| Hard, running | A | 16.01 (17.01 / 17.24) | 16.71 (17.36 / 18.89) |
| | Bh | 1.46 (1.61 / 1.70) | 1.69 (3.67 / 4.38) |
| | C | 1.46 (1.60 / 1.68) | 1.69 (3.79 / 5.03) |
| Clouds | A | 6.90 (7.58 / 8.01) | 7.29 (9.62 / 9.92) |
| | Bh | 3.92 (4.80 / 5.04) | 4.33 (6.79 / 7.38) |
| | C | 3.92 (4.83 / 5.03) | 4.34 (6.76 / 7.43) |

With vsync, 3 runs each. Intervals alternate about 4 and 12.7 ms, so the median is meaningless; this is
the mean interval:

| Scene | A | Bh | C |
|---|---|---|---|
| Hard, standing | 16.81 ms (59.5 fps) | 8.35 ms (119.8 fps) | 8.37 ms (119.4 fps) |
| Hard, running | 16.97 ms (58.9 fps) | 8.34 ms (119.9 fps) | 8.62 ms (116.0 fps) |
| Clouds | 8.44 ms (118.5 fps) | 8.34 ms (119.9 fps) | 8.36 ms (119.6 fps) |

C's 8.62 ms in hard mode running came from one 2-second episode in one run. Ten further interleaved runs
gave Bh and C 8.33–8.35 ms each (120 fps), with no frame over 14 ms.

### Archived (earlier today, `.scratch/optim/bench-final.csv`)

Reference A against the first optimized build, without vsync, 5 runs, work medians: hard standing
15.67 → 0.64 ms, hard running 16.27 → 1.45 ms, clouds 6.88 → 3.79 ms.

## 3. Stale background cover after a level reload

- **Changed:** `src/main.cpp`:
  - `Level::generation` (new), incremented on every `Level::load`;
  - `drawBackground` keys the cache on it, in place of width, height and slab count;
  - the `LEVEL:` log gives the slab count;
  - test hook `--smoke-level-at T,FILE` (Level::load and reset, as a live `level` switch does).
- **Coverage:** startup and live switching load the map, so the counter moves. Restart doesn't touch the
  ground, so the cache stays valid. The per-frame check costs one float more.
- **Fixture:** `.scratch/optim/fix3/map-a.txt` and `map-b.txt`. Both are 40×20 with 5 slabs and the same
  spawn and camera; one 6×6 slab is moved. Warm on one map, switch at frame 120, and compare that frame
  with the other map's cold first frame, both ways.
- **Before (previous build):** 20,141 and 13,479 differing pixels. Most are one block each (304×320 and
  224×320 px) where the newly uncovered pattern is missing.
- **After:** that block is gone. 349 and 498 pixels still differ, all of them Leon's eyes and one or two
  flies, the same in both builds: `reset()` leaves those as they were rather than as a cold start sets
  them. They are not the background.

## 4. The release artifact

- `make` and `make dist` were run; the hashes are above. The tested copy was alone in an empty folder.
- **Credits:** "39 lines, 2182 px". The three enabled pictures load (3 textures of 1600×900) and appear.
  The text and its hearts (Myr♥, Ris♥) render.
- **Music:** both ending tracks play (issue 1).
- **Startup and runs:** no warning in any log.
- **Fullscreen and resize with canvases** (clouds into fullscreen at 2560×1440 and back; dark at
  1000×563): the canvases are released and re-made at each size, colour-only. Every frame is identical
  to the normal build on the same data, and in the A/C suite identical to the reference.

## Not verified

- **Linux:** no Linux machine was available, so the Linux target was neither built nor run. The
  fallback path was exercised on macOS with `--bg-fallback`.
- **Other hardware:** only Apple M3 with OpenGL on Metal. No Intel Mac, no other GPU.
- **Resolution:** frame times are at 1280×720. 2560×1440 was covered for pixels, not timed.
- **Audio:** not heard (zero volume).
- **Keyboard input during tests:** the smoke windows read the real keyboard for the idle guide arrow. In
  one round, input on the machine moved that arrow in 3 frames; re-runs were identical.
