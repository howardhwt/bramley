# Square library tiles and a better demo game

Draft, 2026-11-03. Two changes Howard asked for after reviewing the launch film's stills.

## 1. A better game for the film's gameplay shot

**Before.** Shot s06 ("Press A. You're in.") showed Libbet and the Magic Floor, recorded with `spikes/tour-video/retro_capture.py` and gambatte. The recording sits on a tutorial screen: grey walls, a block of instruction text, a small character. It reads as a puzzle menu, not as play.

**Goal.** Four seconds of gameplay that read at a glance on a 3D-rendered screen in a 1080p film: bright colour, a large moving character, constant motion, no text.

**Hard constraints.**
- Freely licensed for public use in a film (the film is posted publicly).
- Runs in a core we can drive from `retro_capture.py` on the Mac.
- Already part of the demo library shown elsewhere in the film, so the story holds: the library shot (s05) shows the game, A is pressed, s06 plays it.

**Candidates.**

| Game | System | Licence | For | Against |
|---|---|---|---|---|
| Tobu Tobu Girl Deluxe | Game Boy Color | Code MIT, assets CC BY 4.0 | Bright colour, a big character jumping upward through the whole shot, already on Home and in Continue Playing | None found yet |
| Libbet and the Magic Floor (now) | Game Boy | zlib | Already recorded | Tutorial text, small sprite, little motion |
| µCity | Game Boy Color | GPL 3 | Already in the film | A city map that barely moves |
| Freedoom (PrBoom) | Doom | BSD | Fast 3D motion | Grey palette, combat, a core we have not run on the Mac |

**Decision (review, 2026-11-03): Tobu Tobu Girl Deluxe.**
- Record it with `retro_capture.py` and gambatte, scripted past the title and menus, so the shot opens mid-level with the girl already jumping. Four seconds of play with no text on screen.
- Story across the cut: the library shot (s05) ends with focus on Tobu Tobu Girl Deluxe, as the current recording already does, and s06 opens on its gameplay. Library, A, playing.
- The library shot is re-recorded on the desktop build after the square tiles land, so the film shows the new library.
- Check the licence text in the game's repository (code MIT, assets CC BY 4.0) and credit it in the film's description.

## 2. Square, iOS-like tiles in the game library

**Now.** `open_library()` in `workspace/all/launcher/launcher.c` lays games out five across at 150 x 200 (3:4 box art), 40 px gaps, the focused cover lifted with a spring and its title shown under it only. The Music app uses the same grid at 150 x 150. Howard no longer likes the tall tiles (2026-10-29).

**Goal.** Square tiles in the spirit of iOS: rounded square artwork, a title under every tile, the same focus lift and parallax as Home.

**Reuse.**
- `lgfx_draw_art()` already shows art whose shape is far from the tile's whole, centred on a blurred, darkened copy of itself.
- `card_colors()` and the styled title card already cover games without art.
- The Music grid already draws square covers, so games and albums would share one tile shape.

**Layout (decided in review, 2026-11-03).**
- Five tiles across, 150 x 150 px, 40 px gaps, centred: 910 px of grid and 57 px margins on the 1024 px screen. The same column count as today and as the Music grid.
- A name under every tile: one line of Inter (LFONT_LABEL), white, centred under the square, ending in an ellipsis when it does not fit the tile's width. The focused tile's name moves with its lift.
- Row pitch: 150 px tile, 10 px gap, about 24 px name, 30 px to the next row, so about 214 px per row. Three full rows fit under the header (132 px top), about 15 games on screen.
- Order of attention: the focused tile (lifted, scaled, shine), then its name, then the console name and count in the header, then the other tiles and names, then the button hints.

```
+----------------------------------------------------------------------+
| Game Boy Color  12 games                     ... 15:23 100% [batt]   |
|                                     A Play   L1 R1 A-Z   B Back      |
|                                                                      |
|   +------+    +------+    +========+    +------+    +------+         |
|   | art  |    | art  |    | FOCUS  |    | art  |    | art  |         |
|   |      |    |      |    | lifted |    |      |    |      |         |
|   +------+    +------+    +========+    +------+    +------+         |
|   µCity      Libbet a…    Tobu Tobu…    Dangan      Deadeus          |
|                                                                      |
|   +------+    +------+    +------+      +------+    +------+         |
|   ...                                                                |
|   (third row)                                                        |
+----------------------------------------------------------------------+
```

**Corners (decided in review, 2026-11-03).** Square tiles use a 34 px radius, about 22 percent of the width, like an iOS app icon. It scales with the focus lift (34 x 1.12 when focused). Home tiles stay at 16 px, unchanged. Compared in mocks: `lib-r16.png`, `lib-r24.png`, `lib-r34.png` and `home-radius-compare.png` in the session scratchpad.

**Music app (decided in review, 2026-11-03).** The album and audiobook grid follows the same rules, because it is the same grid code: 34 px corners and the album or book name under every cover. The artist leaves the grid and stays on the album page and in Now Playing. Albums without a cover keep their per-album hue (`album_hue()`) with a white music symbol instead of the console symbol.

**Tile states (decided in review, 2026-11-03).**

| State | What the player sees |
|---|---|
| Art, any shape | The whole cover on a blurred, darkened copy of itself that fills the square, through `lgfx_draw_art()` as on Home. Nothing is cropped. Art already near square (ratio 0.8 to 1.25) fills the tile directly. |
| No art | The game's own hue gradient (`card_colors()`) with the console's white symbol centred, as on the Shortcuts-style Home tiles. The name is only under the tile, never inside it. |
| Art still loading | A neutral grey glass square with no symbol. When the cover arrives it fades in over 150 ms. A game whose art check comes back empty switches to the no-art tile once, with the same fade. |
| Art fails to load (bad file) | Treated as no art. |
| Empty folder | Unchanged: the library does not open and the toast "No games in this folder" shows. |

## NOT in scope

- The iOS-style Home and its Home style row in System. They come later, in the same visual language.
- Home tile corners. They stay at 16 px (Howard, 2026-11-03).
- A second art source per game (title screens or snaps). Covers are shown whole on their blur instead.
- The default wallpaper's violet and magenta glow, a common AI look. It is Howard's chosen wallpaper and is unchanged here.

## What already exists

- `lgfx_draw_art()`: whole art on a blurred, darkened copy of itself, already used on Home.
- `card_colors()` and `album_hue()`: per-game and per-album hues for tiles without art.
- The console symbols in `res/brand/consoles/` (Material Symbols plus Bramley's handheld glyph).
- The focus spring (`FOCUS_SCALE` 1.12), shadow and shine, and the library's dark shade and header band.
- `lib_move()`, `jump_letter()` and `lib_scroll_target()`: d-pad movement, A to Z jumps and scrolling, unchanged apart from the new row pitch.

## TODOS.md updates

None. Every approved change is a task below, and nothing was deferred.

## Implementation Tasks

Synthesized from this review's findings. Each task derives from a specific finding above.

- [ ] **T1 (P1, human: ~4h / CC: ~30min)** — library grid — Square tiles, 34 px corners, a name under every tile
  - Surfaced by: Pass 1 (D3, D4), Pass 5 (D9)
  - Files: `workspace/all/launcher/launcher.c` (`open_library`, `cover_pos`, `draw_cover`, `draw_library`, `lib_scroll_target`, `lib_load_next`)
  - Verify: desktop build, a Game Boy folder of 15 games shows three full rows with names; long names end in an ellipsis
- [ ] **T2 (P1, human: ~2h / CC: ~15min)** — library grid — Art drawn whole on its blur through `lgfx_draw_art()`
  - Surfaced by: Pass 2 (D5)
  - Files: `workspace/all/launcher/launcher.c`
  - Verify: wide art (RHDE, Freedoom) and tall box art are centred on their blur, near-square art fills the tile
- [ ] **T3 (P1, human: ~2h / CC: ~15min)** — library grid — No-art tile as hue gradient plus the console's white symbol
  - Surfaced by: Pass 2 (D6)
  - Files: `workspace/all/launcher/launcher.c`, `res/brand/consoles/`
  - Verify: a game with no `.media` image shows its colour and symbol, its name only below
- [ ] **T4 (P2, human: ~2h / CC: ~15min)** — library grid — Grey placeholder while art loads, 150 ms fade to art or to the no-art tile
  - Surfaced by: Pass 2 (D7)
  - Files: `workspace/all/launcher/launcher.c` (per-tile fade time on `Card` or a parallel array)
  - Verify: fast scroll through a 200-game folder on the device shows no flash of wrong colour
- [ ] **T5 (P2, human: ~1h / CC: ~10min)** — Music app — Same grid rules for albums and audiobooks
  - Surfaced by: Pass 5 (D11)
  - Files: `workspace/all/launcher/launcher.c` (`open_music`, `lib_update_caption`)
  - Verify: Music shows names under every cover with 34 px corners; the album page still shows the artist
- [ ] **T6 (P1, human: ~2h / CC: ~20min)** — launch film — Record Tobu Tobu Girl Deluxe gameplay for s06
  - Surfaced by: Pass 3 (D8)
  - Files: `spikes/tour-video/retro_capture.py` (button script), the screens folder `s6`
  - Verify: four seconds open mid-level with the girl jumping and no text on screen; licence checked and credited
- [ ] **T7 (P2, human: ~1h / CC: ~15min)** — launch film — Re-record the library shot (s05) on the new square tiles, ending on Tobu Tobu Girl Deluxe
  - Surfaced by: Pass 3 (D8)
  - Files: screens folder `s5`, `spikes/tour-video/blender/film.py` (range for s5)
  - Verify: stills of s05 and s06 cut from the focused tile to its gameplay

## Design Review Completion Summary

```
  +====================================================================+
  |         DESIGN PLAN REVIEW — COMPLETION SUMMARY                    |
  +====================================================================+
  | System Audit         | No DESIGN.md; BRAND.md and TDD 12.6 used    |
  | Step 0               | 3/10, all 7 passes                          |
  | Pass 1  (Info Arch)  | 3/10 → 9/10 after fixes                    |
  | Pass 2  (States)     | 3/10 → 9/10 after fixes                    |
  | Pass 3  (Journey)    | 5/10 → 9/10 after fixes                    |
  | Pass 4  (AI Slop)    | 8/10 → 8/10 after fixes                    |
  | Pass 5  (Design Sys) | 4/10 → 9/10 after fixes                    |
  | Pass 6  (Responsive) | 8/10 → 8/10 after fixes                    |
  | Pass 7  (Decisions)  | 9 resolved, 0 deferred                     |
  +--------------------------------------------------------------------+
  | NOT in scope         | written (4 items)                           |
  | What already exists  | written                                     |
  | TODOS.md updates     | 0 items proposed                            |
  | Approved Mockups     | 0 generated (designer key invalid); 4 hand mocks |
  | Decisions made       | 9 added to plan                             |
  | Decisions deferred   | 0                                           |
  | Overall design score | 3/10 → 8/10                                 |
  +====================================================================+
```

Decisions: D3 names under every tile, D4 five across at 150 px, D5 whole art on blur, D6 colour plus console symbol, D7 grey placeholder with fade, D8 Tobu Tobu Girl Deluxe, D9 34 px square corners, D10 Home unchanged at 16 px, D11 Music follows the same rules.

### Unresolved Decisions

None.

## GSTACK REVIEW REPORT

| Review | Trigger | Why | Runs | Status | Findings |
|--------|---------|-----|------|--------|----------|
| CEO Review | `/plan-ceo-review` | Scope & strategy | 0 | — | — |
| Outside Review | — | Independent 2nd opinion | 0 | skipped | Howard chose to proceed without |
| Eng Review | `/plan-eng-review` | Architecture & tests (required) | 0 | — | — |
| Design Review | `/plan-design-review` | UI/UX gaps | 1 | clean | score: 3/10 → 8/10, 9 decisions |
| DX Review | `/plan-devex-review` | Developer experience gaps | 0 | — | — |

- **OUTSIDE COVERAGE:** design outside voices skipped by choice; mockup generator unavailable (OpenAI key rejected, 401).
- **VERDICT:** DESIGN CLEARED. Eng review required.

NO UNRESOLVED DECISIONS
