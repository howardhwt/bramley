# TODOs

## Agent pager (docs/designs/agent-pager.md)

Deferred in the CEO review of 2026-11-05. Each waits for v1 of the pager and its battery test.

- **Agent glance in Control Center (EXP-2).** A row in the MENU panel showing each agent's state ("running", "waiting for you"), built from session start and stop hooks, greyed when stale over 10 minutes. The first step toward agent tiles on Home. Effort M.
- **Answer from the lock screen (EXP-3).** A request wakes the locked Brick to the lock screen with the card on it. Depends on Wi-Fi staying connected while locked (the design's open question 2); changing Wi-Fi power behaviour needs Howard's OK. Effort M, risk high.
- **Per-agent rumble patterns (EXP-4).** Map each project to single, double or triple pulse (api.h:558-560) so the asker is known without looking. Worth it once more than one agent uses the pager. Effort S.
