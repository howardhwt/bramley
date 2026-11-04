# Launch film: background and lighting

Draft, 2026-11-03. Covers the look of every shot in `spikes/tour-video/` (`blender/film.py` renders the device on transparency, `film_compose.py` adds the background, titles and music).

## Now

- **Light (until today).** Off-white background (#F5F5F7), dark grey titles (#1D1D1F), grey subtitles (#6E6E73), Apple blue link (#0071E3), a soft contact shadow under the device. The cold open (s01) is black and brightens to white as s02 pulls back. Howard asked for "dark grey on white" on 2026-10-31.
- **Dark (today, uncommitted).** `FILM_THEME=dark`: black (#000) on every shot, white titles (#F5F5F7), light grey subtitles (#A1A1A6), a brighter blue link (#2997FF), no shadow, the end fades to black. Howard asked for "dark background on all scenes" on 2026-11-03.

## What the stills show (draft renders, 2026-11-03)

- Dark: the screens glow and lead every shot; the back shot with its lit strips is strongest; the device's outer edge melts into black in the wide shots (s02 at 2 s, s10).
- Light: the device reads as a clean product object with a shadow; the screens look flatter; the cold open needs a black-to-white transition that the review flagged before (fix 3).

## Decision (review, 2026-11-03)

- **Black, rendered in Blender (D2).** Howard asked for the background to be rendered for real lighting rather than added as a flat colour. `film.py` now builds a stage (`FILM_STAGE=black`, the default): a 6 m floor, near-black (#010101) with a soft sheen (roughness 0.3), under a black world. Light linking keeps the studio lights on the device only, so the floor stays black and shows just the device's reflection and the shade under it.
- **Glossy floor and rim light (D3).** A 150 W disc light, 0.8 m across, sits behind the device facing the camera and travels with it (`FILM_RIM=1`, the default), so the dark grey outline reads against black in every shot.
- **Compose.** `film_compose.py` uses `FILM_THEME=dark` (the default): white titles (#F5F5F7), light grey subtitles (#A1A1A6), link #2997FF, no added shadow, the end fades to black. The rendered frames are opaque, so the background step changes nothing under them.
- Evidence: stills in the session scratchpad, `look-compare.png`, `look-compare2.png` and `stage3/stage.png`.

## NOT in scope

- Title placement, timing and the 18 earlier fixes: unchanged, already in `film.py` and `film_compose.py`.
- The light theme stays available (`FILM_STAGE=none FILM_THEME=light`) but is no longer the film's look.

## What already exists

- The studio lights that travel with the camera, the theme switch in `film_compose.py`, and the stills pipeline (`FILM_STILLS=1`).

## Implementation Tasks

- [x] **T1 (P1, human: ~3h / CC: ~20min)** — film.py — Rendered black stage with light linking and a rim light, as defaults
  - Surfaced by: D2, D3
  - Files: `spikes/tour-video/blender/film.py`
  - Verify: stills of s02, s06, s09, s10 show a black floor with the device's reflection and readable edges (done, `stage3/stage.png`)
- [x] **T2 (P1, human: ~1h / CC: ~10min)** — film_compose.py — Dark theme as default
  - Surfaced by: D2
  - Files: `spikes/tour-video/film_compose.py`
  - Verify: composed stills show white titles on black and a fade to black at the end (done)
- [ ] **T3 (P1, human: — / CC: ~1.5h render)** — film — Render the full draft with the stage, then the 60 fps final
  - Surfaced by: D3
  - Files: `spikes/tour-video/blender/film.py`, `spikes/tour-video/film_compose.py`
  - Verify: watch the draft end to end

## Design Review Completion Summary

```
  +====================================================================+
  |         DESIGN PLAN REVIEW — COMPLETION SUMMARY                    |
  +====================================================================+
  | System Audit         | No DESIGN.md; BRAND.md and Apple film style |
  | Step 0               | 4/10, focus: theme and edge lighting       |
  | Pass 1  (Info Arch)  | not reviewed (focus chosen in D1)          |
  | Pass 2  (States)     | not reviewed (focus chosen in D1)          |
  | Pass 3  (Journey)    | 7/10 → 9/10: black flows from the cold open|
  | Pass 4  (AI Slop)    | not reviewed (focus chosen in D1)          |
  | Pass 5  (Design Sys) | 4/10 → 9/10: one theme, tokens named       |
  | Pass 6  (Responsive) | not applicable (one 1920x1080 film)        |
  | Pass 7  (Decisions)  | 2 resolved, 0 deferred                     |
  +--------------------------------------------------------------------+
  | NOT in scope         | written (2 items)                           |
  | What already exists  | written                                     |
  | TODOS.md updates     | 0 items proposed                            |
  | Approved Mockups     | 0 generated (designer key invalid); real stills |
  | Decisions made       | 2 added to plan                             |
  | Decisions deferred   | 0                                           |
  | Overall design score | 4/10 → 9/10 (reviewed passes)               |
  +====================================================================+
```

### Unresolved Decisions

None.

## GSTACK REVIEW REPORT

| Review | Trigger | Why | Runs | Status | Findings |
|--------|---------|-----|------|--------|----------|
| CEO Review | `/plan-ceo-review` | Scope & strategy | 0 | — | — |
| Outside Review | — | Independent 2nd opinion | 0 | skipped | focused review, not offered |
| Eng Review | `/plan-eng-review` | Architecture & tests (required) | 0 | — | — |
| Design Review | `/plan-design-review` | UI/UX gaps | 1 | clean | score: 4/10 → 9/10, 2 decisions |
| DX Review | `/plan-devex-review` | Developer experience gaps | 0 | — | — |

- **OUTSIDE COVERAGE:** not run; mockup generator unavailable (OpenAI key rejected, 401), real renders used instead.
- **VERDICT:** DESIGN CLEARED for the film's look. Eng review not needed for a spike.

NO UNRESOLVED DECISIONS
