# Gravity Leon — optimization results

18 November 2026. Work order: *Gravity Leon - Performance Optimization Guide.pdf* (this folder), with
the earlier review in `.scratch/perf-review/`. Machine: Apple M3, macOS 15.8, raylib 6.0, Apple clang,
`-std=c++17 -O2` (the Makefile's command).

## In short

- **Faster.** The CPU work per frame drops by 1.7× to 25×, depending on the scene.
  - **Hard mode** was held at **60 fps by its background pattern** (16 ms of work per frame). It now
    takes 0.6–1.5 ms per frame and reaches the screen's refresh rate (835 fps with vsync off).
  - The **engraving look** goes from 13.2 to 4.2 ms per frame.
  - The **Monstermunch / cloud scenes** go from 6.9 to 3.8 ms per frame.
- **Leaner.** Every off-screen canvas loses a depth buffer it never used.
  - Measured at 720p: −27 MB in a cloud scene, −17 MB in the engraving, −14 MB in the dark.
  - Nominally it saves 84 MB at 2560×1440, where 6 canvases × 14 MB used to be wasted.
  - Hard and easy mode use about the same memory as before (within 1–2 MB).
- **The same game.** Every change was checked against the original, frame for frame, with the same
  scripted inputs and random seed:
  - **0 differing pixels in every comparison**: 36 frames after each of the 5 steps (180
    comparisons), plus 5 extra frames;
  - every look, all six background patterns, crossfades, gravity turns, Monstermunchs, clouds,
    darkness, death, the title, the ending and the credits;
  - a single-file (dist-style) build matches too.

## What changed

| # | Change | Where (src/main.cpp) | Why the picture cannot change |
|---|---|---|---|
| S1 | **The background pattern goes into one GPU vertex buffer**, instead of up to 193,000 `DrawLineEx` calls a frame (seigaiha). It is built only from the tiles that can show: those on the screen and not under the opaque ground. It is rebuilt only when what it shows changes. | `bgPatternTiles`, `bgPatternAdd`, `bgMeshBuild`, `bgMeshDraw`, `drawBackground` | Same triangles, same order, same colours, same shader, same matrix: <ul><li>the corners are computed the way raylib 6.0's `DrawLineEx` does, down to the fused multiply-add I found in its library;</li><li>the batch's own matrix is used, by flushing a zero-size triangle;</li><li>tiles are skipped only when wholly off screen or wholly under an opaque slab drawn later.</li></ul> |
| S2 | **`inkLine` instead of `DrawLineEx`**, at all 105 call sites. A line is submitted as one of the batch's quads, so it no longer cuts raylib's batch between each line and each round cap. Before, that made hundreds of draw calls per Monstermunch. | `lineCorners`, `inkLine` | The quad (p2 p0 p1 p3) is split by raylib into exactly `DrawLineEx`'s two triangles, with the same corners and winding. The order of all draws is unchanged. |
| S3 | **Tighter culling of the level.** Tiles more than one tile off the actual (turned) screen are skipped, and so are slabs wholly off it. | `drawLevel`, `gSeen` (set in `main`) | Only geometry that cannot reach the screen is dropped. Everything drawn is computed exactly as before, still clipped to the old square. |
| M1 | **Colour-only canvases.** `LoadRenderTexture` adds a 32-bit depth buffer that nothing uses, since there is no depth test in the game. | `loadCanvas`, at the 5 canvas creations | Same colour texture, format, filter and use. Falls back to raylib's own target if a driver refuses. |
| T | **Test flags** (README, test section): `--seed N`, `--frame-log FILE` and `--no-vsync`. | `main` | Only active when passed. |

The original pattern routine stays as a fallback for an OpenGL without vertex arrays.

## How it was checked

1. **Reference.** The untouched sources, binary and data are kept in `.scratch/optim/reference/`. A
   rebuild of that source gives the project's binary byte for byte (sha256 `01f43790…`).
2. **A and B.**
   - A is the reference plus the three test flags, and nothing else.
   - B is the candidate, built with the same command.
   - Each has its own copy of the same `params.txt`, `levels/` and `assets/`.
   - The shipped `gravityleon` is byte-identical to the final B (`9eb198db…`).
3. **A against A first.** With `--seed`, two runs of A gave identical frames in all scenes, with two
   exceptions handled as the guide advises:
   - the debug overlay's live FPS/CPU/MEM values are masked, in the debug-mode scenes only (a
     242×67 box);
   - the ending's brims follow the audio clock, so the two ending fixtures set
     `ending_brim_music_sync = False` (in the fixture copies only).
4. **A against B after each change.** 36 frames each time:
   - hard mode, standing, running, and through its crossfade from easy;
   - easy mode running;
   - two gravity turns caught mid-rotation;
   - the clouds-and-tongues replay, standing and with a suicide (pieces, fade);
   - the plain cameleon corridor;
   - two dark scenes;
   - the engraving, standing and running;
   - the 8-bit look;
   - the title;
   - all six patterns (`--start-level` DARK, HARDER, CAMELEON, 5);
   - the credits and the ending.

   Plus Leon's thrown tongue, and Monstermunchs waking, throwing and eating him.

   **Every comparison: 0 differing pixels** (`.scratch/optim/ab-*.txt`).
5. **Dist-style build.** The same commands as `make dist` were run into `.scratch/` (not `dist/`):
   static raylib, embedded files. It runs from an empty folder and its clouds frame matches B's
   exactly.

## Speed

Smoke runs at 1280×720, vsync off, 600 frames each. Five A/B repeats, interleaved. "Work" is the CPU
time per frame before the swap (`--frame-log`), over all frames after the first second. The spread is
the range of the five runs' medians.

| Scene | A work, median (p95 / p99) | B work, median (p95 / p99) | Gain | A fps → B fps (no vsync) |
|---|---|---|---|---|
| Hard mode, standing (seigaiha) | 15.67 ms (16.52 / 16.69) | 0.64 ms (1.60 / 1.72) | ×24.6 | 60 → 835 |
| Hard mode, running | 16.27 (17.24 / 17.48) | 1.45 (1.68 / 1.78) | ×11.3 | 59 → 589 |
| Easy mode, running | 1.00 (1.10 / 1.16) | 0.57 (0.88 / 1.03) | ×1.8 | 816 → 948 |
| Gravity turn | 0.93 (1.01 / 1.10) | 0.53 (0.80 / 0.98) | ×1.7 | 871 → 965 |
| Clouds + Monstermunch tongues | 6.88 (7.76 / 8.39) | 3.79 (4.95 / 5.19) | ×1.8 | 135 → 236 |
| Monstermunchs awake, eating | 2.80 (3.21 / 3.33) | 1.22 (1.62 / 1.75) | ×2.3 | 322 → 672 |
| Plain cameleon corridor | 2.21 (2.65 / 3.93) | 1.04 (2.38 / 3.37) | ×2.1 | 394 → 667 |
| Dark mode, lamps | 1.57 (2.85 / 3.09) | 1.50 (2.67 / 2.87) | ×1.05 | 462 → 453 |
| Engraving look | 13.23 (14.13 / 14.37) | 4.20 (4.56 / 4.89) | ×3.1 | 73 → 220 |
| 8-bit look | 6.37 (7.12 / 7.61) | 3.43 (4.42 / 4.86) | ×1.9 | 146 → 265 |
| Credits rolling | 2.99 (3.74 / 3.81) | 0.63 (1.10 / 1.29) | ×4.7 | 306 → 919 |
| Title | 1.05 (1.19 / 1.37) | 0.60 (0.88 / 0.99) | ×1.8 | 770 → 750 |

The five runs' medians stay within ±0.1 ms in every scene (`.scratch/optim/bench-final.csv`).

- With vsync on, as you play, hard mode went from **60 fps (CPU 100 %) to the screen's cap**.
- The dark scene and the title don't change: their cost is not in what was optimized. The dark scene
  is GPU work at full screen.
- The p95 in B is likely the frames where the pattern is rebuilt (camera moving): 1.5–2.5 ms.

## Memory

macOS physical footprint (what Activity Monitor shows), measured with `vmmap` 7 s into each run, three
A/B repeats interleaved, at 1280×720 with vsync on (`.scratch/optim/mem-final.csv`). "Graphics" is the
GPU-memory part of the footprint.

| Scene | A footprint, mean (min–max) | B footprint, mean (min–max) | Change | Graphics A → B |
|---|---|---|---|---|
| Clouds + Monstermunchs (6 canvases) | 318.9 MB (318.0–320.7) | 291.9 MB (291.7–292.1) | **−27.0 MB** | 258.9 → 232.2 |
| Engraving look | 316.9 (312.0–319.4) | 300.0 (299.4–300.7) | **−16.9** | 260.7 → 241.2 |
| Dark mode, lamps | 291.2 (289.9–291.9) | 276.8 (275.0–278.4) | **−14.4** | 230.4 → 215.9 |
| 8-bit look | 275.6 (274.6–277.6) | 271.1 (270.8–271.7) | −4.5 | 217.7 → 213.3 |
| Title | 240.1 (237.1–243.4) | 238.9 (233.4–245.9) | −1.2 (noise) | 184.5 → 179.6 |
| Hard mode | 241.8 (241.5–242.1) | 242.2 (241.5–242.8) | +0.4 (same) | 191.2 → 179.2 |
| Easy mode, running | 227.9 (225.0–229.8) | 236.1 (235.1–238.1) | +8.2, see below | 176.2 → 176.2 |

- **What the savings are.**
  - Mostly the canvases' depth buffers: 3.5 MB each at 720p.
  - Also less vertex traffic. In hard mode, graphics memory falls by 12 MB with no canvas at all,
    since the driver no longer holds copies of 1.2 million vertices re-uploaded every frame.
- **At 2560×1440** each canvas's depth buffer is 14.1 MB. The nominal saving is −84 MB in a cloud
  scene and −14 to −28 MB in dark ones. Since canvases are kept once made, that is −84 to −140 MB by
  the end of a session that has seen the clouds, the dark and the engraving. Your overlay showed
  ~700 MB.
- **Easy mode** was broken down over 4 more repeats.
  - B's own cost is the pattern buffer: about +1 MB (A 228.7–233.7 MB, B 231.3–231.4 MB when the
    window kept the same buffers).
  - In half of the runs, macOS gave B's window one more drawable (+3.5 MB of IOSurface at 720p). That
    is the system's frame pipelining reacting to faster frames, not the game's own memory, and it
    goes both ways: in hard mode it was A, at 60 fps, that held one fewer.
- **The pattern buffer** is sized to what can show (on screen, not under the ground). Built a piece
  at a time, it keeps no full copy in RAM. In the scenes measured it stays within 1–3 MB.

## Not done, and why

| Item (guide) | Measured | Decision |
|---|---|---|
| **Credits pictures loaded at the ending, not at startup** | The snapshot's 7 pictures cost ~18 MB. With your current `endcredits.txt` (3 pictures) it is ~8 MB. | Not done. It needs a worker thread and, to stay exact, the PNG bytes kept from startup (~6 MB), so the net gain is small, and a load during the ending risks a stutter. Easy to add if you want the ~8–18 MB. |
| **Freeing or sharing idle canvases** | Re-creating the 6 cloud canvases costs ~4 ms in one frame at 720p, more at 2560×1440. | Not done: it trades memory for a possible hitch when Leon re-enters a cloud. With M1 each canvas is already half its size. |
| **Checkpoint route grids** (`Level::orderPines`) | 8.7 MB, only while a level loads, freed right after. | Not done: no effect while playing, and the checkpoint order must not change. |
| **Exact replica of raylib's circles** (cached sines) | `DrawCircleSector` is now the largest drawing cost (~0.8 ms in the clouds scene). | Not done: ~0.5 ms to gain, for another copy of raylib's internals. |
| **Reordering off-screen passes (GPU)** | Only measurable full-screen at 2560×1440. | Not done. The dark scenes' 80–91 fps on your monitor are GPU fill, and this is the next thing to try there. |
| `make dist`'s file cache, the JPGs in `assets/` | Dist only. | Not done; noted for the next `make dist`. |

## Not verified

- **Full screen at 2560×1440, with audio and a pad, in real play.** All measurements are 720p smoke
  runs. Please glance at the debug overlay (FPS / MEM) in a few familiar spots.
  - Hard mode should now sit at the cap.
  - MEM should be lower in clouds and dark areas.
- **Resizing the window or toggling full screen** while canvases exist. This is the same creation path
  as at startup, but not run in a test.
- **The Linux build.** It uses only raylib/rlgl calls that exist there (GL 3.3), but was not compiled.
- **Another raylib version.** `lineCorners` copies raylib 6.0's arithmetic. A different raylib would
  still draw correct lines, but not necessarily bit-identical to its own `DrawLineEx`.

## Files

- `.scratch/optim/`:
  - `reference/` (untouched copy), `A/`, `B/`;
  - `fixtures/` (rig copies);
  - `ab.py` (scenes, pixel diff, bench), `bench_all.py`, `mem.py`, `mem_all.py`, `build.py`;
  - results: `ab-*.txt`, `bench-final.csv`, `mem-final.csv`, `mem/`.
- To rerun a comparison: `python3 .scratch/optim/ab.py diff A B` (after `build.py` on a variant folder).
