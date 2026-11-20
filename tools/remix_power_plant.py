#!/usr/bin/env python3
"""Build Power Plant's four-channel remix from the workshop's original chip samples.

No audio or Python packages are needed to build the editable ProTracker module.
--preview additionally needs a C++17 compiler and ffmpeg, and renders through the
game's ModPlayer. The Power Plant MP3 is the musical reference, never a sample.
"""

import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
MODULES = ROOT / 'workshop' / 'modules'
TITLE = 'Power Plant (Factory Chip Remix)'
BPM = 170
ROW_SECONDS = 15 / BPM
PERIODS = (856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453)
PITCHES = {'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11}


def midi(note):
    if isinstance(note, int):
        return note
    return (int(note[-1]) + 1) * 12 + PITCHES[note[0]] + ('#' in note)


def period(note, cycle=32):
    # Tracker C-1 = concert C3 for a 32-byte chip waveform, C1 for 128 bytes.
    m = midi(note) + (24 if cycle == 128 else 0)
    return round(PERIODS[m % 12] * 2 ** ((48 - (m // 12) * 12) / 12))


def sample_from(filename, index, name, volume):
    data = (MODULES / filename).read_bytes()
    assert data[1080:1084] == b'M.K.'
    offset = 1084 + (max(data[952:1080]) + 1) * 1024
    for i in range(index - 1):
        offset += int.from_bytes(data[42 + i * 30:44 + i * 30], 'big') * 2
    header = bytearray(data[20 + (index - 1) * 30:50 + (index - 1) * 30])
    length = int.from_bytes(header[22:24], 'big') * 2
    header[:22] = name.encode('ascii')[:22].ljust(22, b'\0')
    header[25] = volume
    return bytes(header), data[offset:offset + length]


def empty_sample(label):
    return label.encode('ascii')[:22].ljust(22, b'\0') + bytes(8), b''


def event(smp=0, per=0, fx=0, arg=0):
    assert 0 <= smp <= 31 and 0 <= per <= 4095
    assert 0 <= fx < 16 and 0 <= arg <= 255
    return bytes(((smp & 240) | (per >> 8), per & 255, (smp << 4 & 240) | fx, arg))


# Eight bars, transcribed from the exposed lead near the end of the source.
# Durations are sixteenth notes; the backing's A/G pulses live on another voice.
THEME = [
    [('E5', 4), ('D5', 4), ('E5', 4), ('D5', 4)],
    [('D5', 8), ('E5', 4), ('D5', 4)],
    [('F5', 4), ('D5', 4), ('F5', 4), ('D5', 4)],
    [('F5', 4), ('D5', 4), ('F5', 4), ('D5', 4)],
    [('D5', 4), ('C5', 4), ('E5', 4), ('C5', 4)],
    [('C5', 8), ('E5', 4), ('C5', 4)],
    [('B4', 4), ('C5', 4), ('D5', 4), ('C5', 4)],
    [('C5', 8), ('D5', 4), ('E5', 4)],
]

# The busier middle passage, reduced to a singable line rather than stacked chords.
MIDDLE = [
    [('A4', 2), ('E5', 2), ('A4', 4), ('E5', 2), ('A4', 4), ('E5', 2)],
    [('A4', 2), ('E5', 2), ('A4', 4), ('D5', 2), ('C5', 2), ('E5', 4)],
    [('F5', 4), ('C5', 2), ('F5', 4), ('C5', 2), ('F5', 4)],
    [('F5', 4), ('C5', 4), ('F5', 4), ('E5', 2), ('D5', 2)],
    [('C5', 2), ('A4', 2), ('C5', 6), ('A4', 2), ('C5', 4)],
    [('C5', 4), ('F5', 2), ('C5', 2), ('B4', 2), ('A4', 2), ('C5', 4)],
    [('B4', 2), ('D5', 2), ('G4', 4), ('D5', 2), ('G4', 4), ('D5', 2)],
    [('G4', 2), ('D5', 2), ('G4', 4), ('C5', 2), ('B4', 2), ('D5', 4)],
]

# Am / Dm over F / C6 over G / G, two bars per bass root.
HARMONY = [
    ('A2', ['A3', 'C4', 'E4', 'C4'], 'E5', 'C5'),
    ('F2', ['A3', 'D4', 'F4', 'D4'], 'F5', 'D5'),
    ('G2', ['G3', 'C4', 'E4', 'A3'], 'C5', 'A4'),
    ('G2', ['G3', 'B3', 'D4', 'B3'], 'D5', 'B4'),
]

# 128 bars / 3:00.706. Contrasts and a quiet reprise keep the chip texture moving.
SECTIONS = [
    ('Ignition', 8, 'intro'),
    ('Power Plant - first theme', 16, 'theme'),
    ('Pulse drive', 16, 'drive'),
    ('Middle melody', 16, 'middle'),
    ('Chip dialogue', 8, 'dialogue'),
    ('Lights down', 8, 'break'),
    ('Reactor rising', 8, 'build'),
    ('Full power', 16, 'final'),
    ('Full power - answer', 16, 'answer'),
    ('Afterglow', 16, 'outro'),
]


def build():
    samples = [
        sample_from('08 - tno - World Of Pistou II.mod', 1, 'Pistou chip pluck', 64),
        sample_from('03 - tno - Tartopom.mod', 2, 'Tartopom pulse bass', 42),
        sample_from('03 - tno - Tartopom.mod', 3, 'Tartopom soft chip', 54),
        sample_from('08 - tno - World Of Pistou II.mod', 5, 'Pistou sawtooth', 30),
        sample_from('03 - tno - Tartopom.mod', 1, 'Tartopom drum bank', 54),
        sample_from('03 - tno - Tartopom.mod', 4, 'Tartopom noise chip', 20),
    ]
    credits = [
        'POWER PLANT', 'Factory Chip Remix', 'Music reference:',
        'nocomp - Power Plant', '(Ultimate Fan Mix)', 'Chip sound reference:',
        'tno / Factory modules', 'Samples kept verbatim:', 'Tartopom: 1,2,3,4',
        'World of Pistou II:1,5', '4 voices / 170 BPM', 'Lead, bass, arp, drums',
        'Power Plant melody;', 'Am / F / C over G / G', 'New tracker arrangement',
        'No MP3 audio embedded.', 'Loop -> first theme', 'Rebuild with:',
        'tools/', 'remix_power_plant.py', '--preview',
    ]
    samples += [empty_sample(s) for s in credits]
    samples += [empty_sample('') for _ in range(31 - len(samples))]
    score = []
    section_notes = []

    for section, bars, mode in SECTIONS:
        section_notes.append((len(score) * ROW_SECONDS, section))
        for local_bar in range(bars):
            phrase_bar = local_bar % 8
            root, arp, chord_top, chord_low = HARMONY[phrase_bar // 2]
            bar = [[event() for _ in range(4)] for _ in range(16)]
            quiet = mode in ('intro', 'break', 'outro')
            hot = mode in ('drive', 'final', 'answer')
            fade = 1.0
            if mode == 'intro':
                fade = .40 + .60 * local_bar / 7
            if mode == 'outro':
                fade = 1.0 - .70 * local_bar / (bars - 1)

            def put(row, ch, smp=0, note=None, fx=0, arg=0, cycle=32, per=None):
                bar[row][ch] = event(smp, per if per is not None else (period(note, cycle) if note is not None else 0), fx, arg)

            # Bouncy pulse bass, with sixteenth-note volume breathing as in Dying Respect.
            bass_volume = round((25 if quiet else 36) * fade)
            if mode == 'break':
                bass_rows = (0, 8)
            else:
                bass_rows = (0, 3, 6, 8, 11, 14) if hot else (0, 4, 8, 12)
            for r in bass_rows:
                n = midi(root) + (12 if r in (6, 14) and hot else 0)
                put(r, 1, 2, n, 12, bass_volume, cycle=128)
                if r + 1 < 16:
                    put(r + 1, 1, fx=12, arg=round(bass_volume * .55))
                if r + 2 < 16 and r + 2 not in bass_rows:
                    put(r + 2, 1, fx=12, arg=round(bass_volume * .25))

            # Rapid broken chords and soft, delayed replies: a signature of the references.
            arp_volume = round((20 if quiet else 27) * fade)
            if mode == 'intro':
                arp_volume = round((19 + local_bar) * fade)
            step = 2 if mode in ('intro', 'break', 'outro', 'middle') else 1
            for r in range(0, 16, step):
                n = midi(arp[(r // step) % 4]) + (12 if mode == 'break' else 0)
                instrument = 3 if quiet else 1
                put(r, 2, instrument, n, 12, max(1, arp_volume - (5 if r % 4 else 0)))
                if step == 2:
                    put(r + 1, 2, fx=10, arg=2)

            lead_smp = 3 if mode in ('intro', 'theme', 'break', 'build', 'outro') else 1
            lead_volume = round((49 if lead_smp == 3 else 62) * fade)
            melody = MIDDLE[phrase_bar] if mode in ('middle', 'dialogue') else THEME[phrase_bar]
            if mode == 'intro':
                melody = [(chord_top, 8), (chord_low, 8)]
                lead_volume = round(31 * fade)
            elif mode == 'break':
                melody = [(chord_low, 12), (chord_top, 4)]
                lead_volume = 32
            elif mode == 'build' and local_bar < 4:
                melody = [(chord_top, 8), (chord_low, 8)]
            elif mode == 'outro' and local_bar >= 8:
                melody = [(chord_top, 8), (chord_low, 8)]

            assert sum(d for _, d in melody) == 16
            r = 0
            for n, duration in melody:
                # The first theme uses the source's C-D-E opening; later repeats answer on E.
                if mode == 'theme' and local_bar == 0 and r == 0:
                    n = 'C5'
                put(r, 0, lead_smp, n, 12, lead_volume)
                for k in range(1, duration):
                    if lead_smp == 3:
                        put(r + k, 0, fx=4, arg=0x42 if k >= 2 else 0x00)
                    elif k > 1:
                        put(r + k, 0, fx=10, arg=2)
                # Low-volume one-row echoes borrow the accompaniment channel.
                if mode in ('theme', 'dialogue', 'answer', 'outro') and duration >= 4:
                    er = r + 1
                    put(er, 2, 1, n, 12, round(lead_volume * .20))
                    if er + 1 < 16 and step == 2:
                        put(er + 1, 2, 3, arp[((er + 1) // 2) % 4], 12, arp_volume)
                r += duration

            if mode == 'dialogue' and local_bar % 2:
                # Give the saw a brief reply; retain the source line's pitch and rhythm.
                for r in range(16):
                    raw = bytearray(bar[r][0])
                    if raw[0] & 15 or raw[1]:
                        raw[2] = (4 << 4) | 12
                        raw[3] = 27
                        bar[r][0] = bytes(raw)

            # Kick/snare/hats share the very same 1,978-byte composite drum sample.
            drums = mode not in ('intro', 'break', 'outro') or (mode == 'intro' and local_bar >= 4) or (mode == 'outro' and local_bar < 8)
            if drums:
                for r in range(0, 16, 2):
                    if r in (0, 8) or (hot and r == 14 and local_bar % 4 == 2):
                        put(r, 3, 5, fx=12, arg=round(49 * fade), per=428)
                    elif r in (4, 12):
                        put(r, 3, 5, fx=9, arg=3, per=214)
                    else:
                        put(r, 3, 5, fx=9, arg=5, per=214)
                    if r + 1 < 16:
                        put(r + 1, 3, fx=12, arg=round((28 if r in (4, 12) else 17) * fade))
                if local_bar % 8 == 7 and mode not in ('intro', 'outro'):
                    for r in (12, 13, 14, 15):
                        put(r, 3, 5, fx=9, arg=3 if r % 2 == 0 else 5, per=214 if r < 15 else 190)
                if mode == 'build' and local_bar >= 6:
                    for r in range(16):
                        put(r, 3, 5, fx=9, arg=3 if r % 4 == 0 else 5, per=214)
            elif mode == 'break' and local_bar % 2 == 0:
                put(0, 3, 5, fx=12, arg=27, per=428)

            score.extend(bar)

    # Set tempo and speed explicitly; eliminate lingering state when the song loops.
    score[0][3] = event(fx=15, arg=BPM)
    score[0][1] = event(2, period('A2', 128), 15, 6)
    # The intro is eight bars. Subsequent passes return straight to the first theme.
    score[-1][3] = event(fx=11, arg=2)
    assert len(score) == 2048 and len(score) % 64 == 0
    patterns = [b''.join(e for row in score[i:i + 64] for e in row) for i in range(0, len(score), 64)]
    header = TITLE.encode('ascii')[:20].ljust(20, b'\0')
    header += b''.join(s[0] for s in samples)
    header += bytes((len(patterns), 2))
    header += bytes(range(len(patterns))).ljust(128, b'\0') + b'M.K.'
    assert len(header) == 1084
    data = header + b''.join(patterns) + b''.join(s[1] for s in samples)
    return data, len(score) * ROW_SECONDS, section_notes


RENDER_CPP = r'''
#include "modplayer.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
int main(int argc, char** argv) {
    if (argc != 4) return 2;
    ModPlayer p; if (!p.load(argv[1])) return 3;
    FILE* f = fopen(argv[2], "wb"); if (!f) return 4;
    int remain = (int)(atof(argv[3]) * p.rate());
    std::vector<short> b(8192);
    while (remain > 0) {
        int n = remain < 4096 ? remain : 4096;
        p.render(b.data(), n);
        if (fwrite(b.data(), sizeof(short) * 2, n, f) != (size_t)n) return 5;
        remain -= n;
    }
    return fclose(f);
}
'''


def preview(path, duration):
    for command in ('c++', 'ffmpeg'):
        if not shutil.which(command):
            raise SystemExit(f'Preview needs {command}; the .mod has already been built.')
    with tempfile.TemporaryDirectory(prefix='power-plant-') as work:
        work = Path(work)
        (work / 'render.cpp').write_text(RENDER_CPP)
        subprocess.run(['c++', '-O2', '-std=c++17', '-I' + str(ROOT / 'src'),
                        str(work / 'render.cpp'), str(ROOT / 'src' / 'modplayer.cpp'),
                        '-o', str(work / 'render')], check=True)
        # Include a short continuation into the loop, then fade the listening copy.
        seconds = duration + 2.5
        subprocess.run([str(work / 'render'), str(path), str(work / 'mix.s16'), str(seconds)], check=True)
        subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-y',
                        '-f', 's16le', '-ar', '44100', '-ac', '2', '-i', str(work / 'mix.s16'),
                        '-af', f'highpass=f=25,lowpass=f=10500,loudnorm=I=-16:TP=-1.5:LRA=9,afade=t=out:st={duration}:d=2.5',
                        '-ar', '44100', '-codec:a', 'libmp3lame', '-b:a', '192k',
                        '-metadata', f'title={TITLE}', '-metadata', 'artist=Power Plant / Factory-style arrangement',
                        '-metadata', 'comment=Four-channel tracker remix; samples from tno/Factory workshop modules.',
                        str(path.with_suffix('.mp3'))], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--preview', action='store_true', help='also render the MP3 through the game module player')
    parser.add_argument('--output', type=Path, default=MODULES / (TITLE + '.mod'))
    args = parser.parse_args()
    data, duration, sections = build()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(data)
    print(f'{args.output}: {len(data):,} bytes, {duration:.3f} s, 4 channels, {BPM} BPM')
    for t, label in sections:
        print(f'{int(t)//60}:{t%60:05.2f}  {label}')
    if args.preview:
        preview(args.output.resolve(), duration)
        print(args.output.with_suffix('.mp3'))


if __name__ == '__main__':
    main()
