#!/usr/bin/env python3
"""Keep the fastest recorded time per mode in each character's distribution table."""
import argparse
from decimal import Decimal, InvalidOperation
import fcntl
import os
from pathlib import Path
import sys
import tempfile

from embed_gen import SCORE_NAMES


def read_table(path):
    try:
        return path.read_text(encoding="utf-8")
    except FileNotFoundError:
        return None


def best_rows(directory, name):
    path = directory / ("dist_" + name)
    original = read_table(path)
    best = {}
    # Visit the distribution table first so an equal time keeps its existing initials.
    for source in (path, directory / name, directory / name.replace("highscores", "highscore")):
        contents = original if source == path else read_table(source)
        for number, line in enumerate((contents or "").splitlines(), 1):
            if not line.strip():
                continue
            try:
                mode, _, elapsed = line.split()
                seconds = Decimal(elapsed)
                if not seconds.is_finite() or seconds < 0:
                    raise ValueError
            except (ValueError, InvalidOperation):
                raise ValueError(f"{source}:{number}: expected a mode, initials and a nonnegative time") from None
            if mode not in best or seconds < best[mode][0]:
                best[mode] = (seconds, line)
    lines = [line for _, line in best.values()]
    if original is not None and original.splitlines() == lines:
        return None
    return "".join(line + "\n" for line in lines)


def update(directory):
    directory.mkdir(parents=True, exist_ok=True)
    # Native and web releases can be built at the same time.
    lock = os.open(directory, os.O_RDONLY)
    try:
        fcntl.flock(lock, fcntl.LOCK_EX)
        updates = [(directory / ("dist_" + name), best_rows(directory, name)) for name in SCORE_NAMES]
        for path, contents in updates:
            if contents is None:
                continue
            with tempfile.NamedTemporaryFile(dir=directory, delete=False) as tmp:
                try:
                    tmp.write(contents.encode("utf-8"))
                    tmp.flush()
                    os.fchmod(tmp.fileno(), path.stat().st_mode & 0o777 if path.exists() else 0o644)
                    os.replace(tmp.name, path)
                finally:
                    if os.path.exists(tmp.name):
                        os.unlink(tmp.name)
            print("Updated", path)
    finally:
        os.close(lock)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path, nargs="?", default=Path(__file__).resolve().parents[1] / "data")
    try:
        update(parser.parse_args().directory)
    except (ValueError, OSError) as error:
        sys.exit(str(error))
