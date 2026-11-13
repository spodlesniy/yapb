# D339 — Blocked-forward memory coverage

## Scope

D338 automatic Training tools CI passed at commit `9e8db83`, but the user-provided D338 evaluation reported `matched_pairs=0/3` due to 16.607, 16.378 and 45.979-unit initial-position mismatches.
Those runs cannot establish a policy ranking.
Earlier `memory` traces mostly had mask 7, so they did not exercise the policy's response to blocked forward movement.
D339 adds a separate read-only coverage tool for one existing D337 `move-memory` trace and deterministic Python/TCP tests.
No new gameplay commands, policy scoring changes, DLL changes, wire fields, or automatic execution are introduced.

The D339 coverage report counts completed actions with forward available, forward blocked with both side directions available (mask 6), and forward blocked with exactly one safe side direction (mask 2 or 4).
For blocked-forward actions it reports the selected allowed side and 3D displacement between the recorded start and confirmed feedback observation.
A blocked action with less than 24 units of observed displacement is insufficient progress evidence, even if the terminal feedback says Completed.
The trace must contain the full requested 4–8 completed actions and at least one forward-available control, one mask-6 case and one mask-2/4 case.
The CLI reports each missing category, returning nonzero when coverage is incomplete.
The existing strict D337 trace parser rejects invalid masks, mismatched directions, identities, unconfirmed or malformed records before analysis.
The 24-unit threshold is a provisional diagnostic gate, not a GoldSrc movement capability limit.
A completed action and its displacement do not establish that the route is optimal, or that obstacle avoidance is effective across arbitrary maps.

## Operator validation, deferred

Once ready to perform detailed physical tests, choose a verified safe location on `de_dust2` where forward can become blocked and the engine can observe both masks 6 and 2/4 over 4–8 bounded actions.
The operator, not this script, must place the bot and verify actual game conditions.
Start the local Python server from the repository root:

```powershell
python -m tools.aipb_training.modelio_test_server --port 8765 --mode move-memory --actions 8 --max-connections 16 --trace-file memory_blocked.json
```

In the game console:

```text
aipb modelio memory 1 8765 8 30
```

After the Python process finishes and writes eight confirmed actions:

```powershell
python -m tools.aipb_training.modelio_blocked_memory --memory memory_blocked.json
```

Expected diagnostic status when the physical masks cover all categories: `status=BLOCKED_MEMORY_COVERED`.
If the location does not generate both blocked-forward categories, `INSUFFICIENT_BLOCKED_MEMORY_EVIDENCE` is the correct result, not a regression.
Always verify `sequence leg=8/8 PASS`, `recovered=1` and all five runtime recovery flags; the JSON trace alone does not prove restoration.
A separate comparative D338 experiment still requires matching the original position and yaw before each trial, and remains open.

## Automated tests

```powershell
python -m unittest tools.aipb_training.tests.test_modelio_blocked_memory
```

The automatic Training tools CI discovers this file via `test_*.py`.
The tests cover both blocked-side cases, forced safe alternatives, missing evidence, lack of physical progress, invalid input, and four correlated actions through a real localhost TCP subprocess with controlled mask sequence 7 → 6 → 4 → 2.
The subprocess is a deterministic wire fixture, not a substitute for actual HLDS physics.
Automatic CI and operator-run HLDS acceptance remain separate gates; only the user starts manual Windows x86 and full builds.
