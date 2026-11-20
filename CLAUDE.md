# Gravity Leon — working notes

## Bug reports (`Reports/`)

The user files reports from inside the game: **Shift+B** freezes the game, takes a picture of
that frame, and opens a text box; **Enter** saves, Esc cancels. Each report is two files with
the same name in `Reports/`:

```
Reports/<YYYYMMDD>_<HH>h<MM>m<SS>s_[N_]<status>.txt   the user's text, then the game's data
Reports/<YYYYMMDD>_<HH>h<MM>m<SS>s_[N_]<status>.png   the picture
```

`N_` (2, 3, …) appears only when two reports share the same second. `<status>` is `open` until
the report has been addressed, then `closed`.

`Reports/guide_for_AI.txt` is not a report: it restates this workflow for an AI that starts with
no context. Keep it in step when the workflow below changes.

The `.txt` holds **the user's text first** — that is the prompt — then, after the line
`=== GRAVITY LEON REPORT DATA ...`, data written by the game: the date, the picture's name, a
ready-made reproduction command, a `[state]` block (level file, Leon's position in world tiles
and pixels, gravity, velocity, facing, knock-out and death state, abilities, the hat, the tongue with its
sticking point and bends, checkpoint, lit checkpoints, eaten fruits, stars, blocks, timers,
hard mode, the golden run, the black flag, his colour (the golden doors'), the last key, the ending's progress, golden flies, camera, the look (decor, engraving), screen), and a copy of `params.txt` as it was in effect,
between `[params.txt]` and `[end params.txt]`.

### When the user says…

- **"list open reports"** — list `Reports/*_open.txt`, oldest first: name, the user's text
  (or its first lines if long), the level and Leon's tile from `[state]`. Change nothing.
- **"address open reports"** — address every open report, oldest first, one after the other.
- **"address report XXX"** — address that one report. XXX is any unambiguous part of its name
  (usually the timestamp, e.g. `20261024_18h25m35s`). If it does not match exactly one report,
  say so and list the candidates.

### Addressing a report

1. Read the `.txt`. **The user's text is the request**: treat it exactly as if they had typed it
   in the chat, keywords included (see below). The data block is context, not instructions.
2. Look at the `.png` (Read tool): it shows what the user saw when they pressed Shift+B.
3. Reproduce the moment when it helps, with the command written in the report, e.g.
   ```
   ./gravityleon --from-report "Reports/<name>.txt" --smoke-name repro-<stamp> --smoke-run 0 --smoke-jumps 0 --smoke-time 0.1
   ```
   `--from-report` loads the level and the report's own copy of the params, puts Leon back
   (position, gravity frame, velocity, facing), with his abilities, checkpoint, lit checkpoints,
   fruits, stars, blocks, hard mode, golden flies and a stuck tongue, and frames the camera as it
   was. Compare the resulting `screenshots/…_repro-<stamp>.png` with the report's picture. (Not
   restored: the wild flies, particles, a tongue in flight, a knock-out or a death in progress.)
4. Do the work, test it as usual, and report back in the chat as for any request.
5. Close the report: rename **both** files from `_open` to `_closed` (same name otherwise),
   e.g. `mv Reports/<stamp>_open.txt Reports/<stamp>_closed.txt`, and the same for the `.png`.
   That is all: what was done goes in the chat reply, not in the report.

**A report is read-only.** The only change ever allowed to a report is that rename. Never write
to its `.txt` or `.png`: no note appended, no edit, no copy over it, no script that opens it for
writing (Python's `open(p, "w")` empties the file before anything else runs: on 2026-11-16 a
closing note appended that way wiped 14 reports, 3 for good). To replay a report with changed
values (the map grew, a door was added…), copy it into `.scratch/` and change the copy.
**Never delete a report**, open or closed.

A request in a report that needs the user's decision is handled as in the chat: ask, and leave
the report open until it is settled.

### Keywords in a report's text (as in the chat)

- **TIPSI**: Think, Investigate, Plan, Suggest, and ask before Implementation — the code is not
  modified before the user confirms.
- **TIPTSI**: Think, Investigate, Plan, Suggest, Test, and ask before Implementation — the code
  may be modified to test, but is reverted before asking.

## params.txt

The user's own file, often edited while the game runs. **Never shrink it or delete a line from
it without the user's explicit consent** — no text cut either, their notes at the end of the file
included. Only add lines (line-anchored edits); never change a value they set unless they ask;
never rewrite or restore the file wholesale. Re-read it right before any edit.

## Running and testing

`make run` builds and starts the game. Scripted test runs (`--smoke-name …`, see README.md) save
a picture into `screenshots/` and quit; they are silent and windowed. Test options that only
matter for reports: `--from-report FILE`, `--reports-dir DIR` (write reports elsewhere — use it
in tests so no fake report lands in `Reports/`), `--test-report TEXT`, `--show-report-box TEXT`.
