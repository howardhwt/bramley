# Launch film: camera movement and demo library

Draft, 2026-11-04. Covers the camera in every shot of `spikes/tour-video/blender/film.py` and two fixes to the demo library in `spikes/demo-library/make.sh`. Howard watched `animatic-v3.mp4` (42.8 s, 30 fps draft) and found the camera movement subpar. No video work starts until Howard says so.

## Now

- **One move per shot.** Each shot keys the camera on a straight line from `c0` to `c1` and its aim from `t0` to `t1`, with a cubic ease in and out across the whole shot (`ease()`). s02 and s10 finish their move early and hold (`MOVE`). s05 and s10 also turn the device on a pivot (12 and 20 degrees).
- **Lens.** 50 mm on every shot. No depth of field and no motion blur.
- **Distances moved** (start to end):

| Shot | Content | Move | Sideways | Depth |
|------|---------|------|----------|-------|
| s01 | Splash, cold open | 12.1 cm | left | out |
| s02 | Home, reveal | 24.1 cm | left | out |
| s03 | Home, Continue Playing | 6.3 cm | left | in |
| s04 | Home, Doom | 3.2 cm | right | out |
| s05 | Game Boy library | 3.2 cm | left | in |
| s06 | Tobu Tobu Girl Deluxe | 2.0 cm | none | in |
| s07 | Now Playing | 3.2 cm | left | out |
| s08 | Settings, Activity | 4.2 cm | right | out |
| s09 | The back | 34.0 cm | left | in |
| s10 | End card | 2.0 cm | none | in |

## Camera decisions (review, 2026-11-04)

- **C1. Interface shots push in slowly and steadily.** s03 to s08 each push straight toward the screen at one shared speed, 6 to 8 cm over 4 s (at 85 mm, scaled from the new camera distance). The interface does the lateral motion; the camera never slides across it.
- **C2. Motion carries through every cut.** Each shot moves at constant speed from its first frame to its last. The only eases are into the film's first frame (s01) and out of moves that land a title on a still device (s02's reveal, s10's end card), using the existing `MOVE` hold. The cubic in-out across whole shots goes.
- **C3. One screen direction.** Every sideways or orbiting move travels with the camera heading left, so the device appears to turn left to right. Each interface shot's viewing angle is chosen so its push does not reverse that direction across a cut.
- **C4. 85 mm at f/4, focused on the screen.** Camera distance grows about 1.7 times to keep today's framing. Focus sits on the screen (the `target` empty). The lights that travel with the camera are re-placed so exposure and the rim light match today's look. Evidence: `lens-compare.jpg` (50, 85 and 105 mm on s04 and s08) in the session scratchpad.
- **C5. The product shots form one arc.** s01 pushes slowly into the waking screen. s02 pulls back while orbiting left to reveal the device. s09 makes the long orbit round the back. s10 orbits left about 25 degrees and settles into a still hold for the end card, answering the reveal.
- **C6. Natural motion blur** on the final render, with a film camera's shutter (half the frame time).
- **C7. Timing.** s09 gives two beats (1.015 s) to s10: s09 8.10 s to 7.09 s, s10 4.10 s to 5.12 s. Every cut stays on the beat and the film keeps its 42.8 s length, which already uses the music to its last frame (45.0 s with the 2.21 s skip).

## Demo library (approved by Howard, 2026-11-04)

- **D-LIB-1.** Drop the original Tobu Tobu Girl from the demo card; keep Tobu Tobu Girl Deluxe. Look for one more free Game Boy game with clearly licensed art to keep four tiles; three tiles is the fallback.
- **D-LIB-2.** Continue Playing shows no game that seems to vanish in the next shot: µCity (Game Boy Color) leaves Continue Playing and Adjustris (Game Boy) takes its place, so every game in the Continue Playing row of s03 and s04 is in the Game Boy library of s05 or another console the film never opens.

## NOT in scope

- The device model itself. Howard says it is out of date; that waits for his notes or a photo, and lands before any re-render so the film renders once.
- Titles, music and the dark stage, already decided in `film-look.md`.
- Shot order and which shots exist; Howard chose a camera-only review.

## What already exists

- `FILM_STILLS=1` renders the start, middle and end of each shot for checking before a full render.
- `MOVE` lets a shot finish its move early and hold for a title.
- The pivot lets the device turn under a still camera.
- The lights and rim light already travel with the camera, so they move with C4's new distance.

## Implementation Tasks

- [ ] **T1 (P1, human: ~3h / CC: ~30min)** — film.py — Camera system: constant-speed moves, one direction, 85 mm at f/4, motion blur on the final
  - Surfaced by: C1, C2, C3, C4, C6
  - Files: `spikes/tour-video/blender/film.py`
  - Verify: stills of every shot (`FILM_STILLS=1`) show matched framing and exposure to today; a contact strip of each cut's last and first frames shows no direction reversals
- [ ] **T2 (P1, human: ~2h / CC: ~20min)** — film.py, film_compose.py — Product-shot arc and the s09/s10 timing
  - Surfaced by: C5, C7
  - Files: `spikes/tour-video/blender/film.py`, `spikes/tour-video/film_compose.py`
  - Verify: s10's end card text sits on a still device for at least 2 s; the composed film is still 42.8 s and every cut lands on a beat
- [ ] **T3 (P1, human: ~1h / CC: ~15min)** — demo library — Drop Tobu Tobu Girl, replace µCity in Continue Playing, look for a fourth Game Boy game
  - Surfaced by: D-LIB-1, D-LIB-2
  - Files: `spikes/demo-library/make.sh`, `spikes/demo-library/CREDITS.md`
  - Verify: Home and the Game Boy library screenshots on the desktop build show no duplicate and no vanishing game
- [ ] **T4 (P1, human: — / CC: ~30min plus render)** — film — Re-record s03 to s05, render stills for Howard, then the draft
  - Surfaced by: T1 to T3; waits for Howard's go-ahead and the model update
  - Files: screen clips in the job folder
  - Verify: Howard approves the stills before the full draft render

## Design Review Completion Summary

```
  +====================================================================+
  |         DESIGN PLAN REVIEW — COMPLETION SUMMARY                    |
  +====================================================================+
  | System Audit         | No DESIGN.md; BRAND.md, film-look.md        |
  | Step 0               | 3/10, focus: camera only                   |
  | Pass 1  (Info Arch)  | not reviewed (camera-only focus)           |
  | Pass 2  (States)     | not reviewed (camera-only focus)           |
  | Pass 3  (Journey)    | 5/10 → 9/10: product shots form one arc    |
  | Pass 4  (AI Slop)    | 5/10 → 9/10: authored moves, not drift     |
  | Pass 5  (Design Sys) | 2/10 → 9/10: one camera system             |
  | Pass 6  (Responsive) | not applicable (one 1920x1080 film)        |
  | Pass 7  (Decisions)  | 7 resolved, 0 deferred                     |
  +--------------------------------------------------------------------+
  | NOT in scope         | written (3 items)                           |
  | What already exists  | written                                     |
  | TODOS.md updates     | 0 items proposed                            |
  | Approved Mockups     | 0 generated (designer key invalid); real lens stills |
  | Decisions made       | 7 added to plan                             |
  | Decisions deferred   | 0                                           |
  | Overall design score | 2/10 → 9/10 (reviewed passes)               |
  +====================================================================+
```

Not a 10: the exact orbit angles and push speeds are set from stills during T1 and T2, and the model update is still pending.

### Unresolved Decisions

None.

## GSTACK REVIEW REPORT

| Review | Trigger | Why | Runs | Status | Findings |
|--------|---------|-----|------|--------|----------|
| CEO Review | `/plan-ceo-review` | Scope & strategy | 0 | — | — |
| Outside Review | — | Independent 2nd opinion | 0 | skipped | declined in earlier reviews |
| Eng Review | `/plan-eng-review` | Architecture & tests (required) | 0 | — | — |
| Design Review | `/plan-design-review` | UI/UX gaps | 3 | clean | score: 2/10 → 9/10, 7 decisions |
| DX Review | `/plan-devex-review` | Developer experience gaps | 0 | — | — |

- **OUTSIDE COVERAGE:** not run; Howard declined outside voices in earlier reviews; the mockup generator is unavailable (OpenAI key rejected, 401), so real renders were used.
- **VERDICT:** DESIGN CLEARED for the film's camera. Eng review not needed for a spike.

NO UNRESOLVED DECISIONS
