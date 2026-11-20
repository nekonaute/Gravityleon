# Power Plant — Factory Chip Remix

An arrangement of `nocomp - Power Plant (Ultimate Fan Mix).mp3` in the chip-module
language of the tno/Factory tracks in this folder.

- **Listen:** `Power Plant (Factory Chip Remix).mp3` — 3:03, stereo, 192 kbps.
- **Edit/play:** `Power Plant (Factory Chip Remix).mod` — ProTracker `M.K.`, four
  channels, 32 patterns, 170 BPM. First pass: 3:00.706; subsequent passes return
  to the first theme at 0:11.294. The MP3 includes a short faded continuation.

The Power Plant lead phrases and A/F/G bass progression are recast as chip leads,
rapid broken chords, pulsing bass, vibrato, short melodic replies and composite
sample percussion. This is a condensed new arrangement with a breakdown and
reprise, rather than a bar-for-bar conversion of the four-minute recording.

The sample audio is copied verbatim from the local references:

| Voice | Instrument source |
| --- | --- |
| Bright lead and broken chords | World Of Pistou II, sample 1 |
| Pulse bass | Tartopom, sample 2 |
| Soft lead and broken chords | Tartopom, sample 3 |
| Brief sawtooth replies | World Of Pistou II, sample 5 |
| Kick, snare and hats, selected with sample offsets | Tartopom, sample 1 |

Tartopom's noise chip (sample 4) is also retained as an unused instrument for
editing. The original MP3 supplies the musical reference; no part of its audio
is embedded in the module. The source songs and the game's music selection are
unchanged.

| Time | Section |
| --- | --- |
| 0:00 | Ignition |
| 0:11 | Power Plant — first theme |
| 0:34 | Pulse drive |
| 0:56 | Middle melody |
| 1:19 | Chip dialogue |
| 1:30 | Lights down |
| 1:42 | Reactor rising |
| 1:53 | Full power |
| 2:16 | Full power — answer |
| 2:38 | Afterglow |

Rebuild from the project root:

```sh
python3 tools/remix_power_plant.py --preview
```

Building the module uses Python's standard library. The optional MP3 export needs
`c++` and `ffmpeg`; it renders through the game's existing `src/modplayer.cpp`.
The preview applies DC removal, a gentle treble roll-off, loudness normalization
and an ending fade. The module retains its original sample bytes and tracker mix.
