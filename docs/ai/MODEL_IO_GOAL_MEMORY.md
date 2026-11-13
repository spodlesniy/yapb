# D341 — Confirmed-position memory and local-goal stall detection

## Scope

D340's automatic CI passed for commit `f18943c` (run 43440505187).
The operator supplied four physical `de_dust2` tests: unobstructed goal success after 5/8 legs and 10 requests, blocked-forward detour goal success after 7/8 legs and 14 requests, an unreachable target stopping safely after 6/8 legs with `InvalidAction`, and manual `stop` returning `Requested` and all five recovery flags true.
The invalid remote-coordinate and invalid numeric-coordinate cases were rejected before arming.
In the unreachable run the side directions alternated `left,right,left,right,left` before `forward`, demonstrating an unproductive oscillation that the D340 memory-free nearest-candidate rule cannot prevent.
D341 adds a fixed-capacity, engine-neutral memory of confirmed ground positions **only** to the existing operator-armed `goal` mode.
It does not add a map graph, global route planner, autonomous goal selection, Python dependency, new wire frame, or new operator command.
All other Model I/O trial modes retain their current selection algorithm and controls.

## Policy and stop reasons

A new bounded `ModelIOGoalHistory` stores the starting observation and at most eight subsequent physically completed, terminal-feedback-correlated and fully recovered leg observations.
Candidate directions remain strictly limited to the current engine `TraceHull` allowlist.
Before each goal leg the algorithm rejects candidates whose target falls within 36 units of any recorded location, unless that target is inside the 16-unit goal radius.
It then chooses the remaining authorized destination with lowest direct distance to the goal, breaking ties by forward/left/right order.
This is a *filter* on current safe directions, not permission to walk through an obstacle.

At the completed-leg boundary, a confirmed return within 36 units of an **older** recorded position is a `GoalCycle`.
The immediately preceding departure point is not counted as an older revisit.
If the bot has not improved its best-to-date Euclidean goal distance by more than 12 units in three consecutive completed steps, it stops with `GoalNoProgress`.
A leg that improves the best distance beyond that threshold resets the stall counter; the current best distance remains monotonic.
The last step may be shorter than 12 units only if the bot reaches the original 16-unit goal radius, which takes precedence over a stall outcome.
If engine traces admit no direction the terminal reason is `GoalDeadEnd`.
If traces offer directions but all lead back into recently visited regions it is `GoalNoNovelDirection`.
These outcomes are all reported as **failures to reach the goal**, not `goal PASS`.

History survives the internal cleanup between confirmed goal legs but is fully cleared on a new operator command, final successful/failed session, external stop, disconnection or map reset.
It never updates on uncorrelated or failed feedback, on speculative candidate targets, or on a bare accepted action.
The existing 4–8-action, 5–30-second budget, 768-XY/8-Z goal admission bounds, collision checks, runtime owner gates and five-component restoration verification are unchanged.
A permanently clear but unproductive path can still fail to approach a destination; this is explicit uncertainty, not a claim of complete obstacle avoidance.

## Manual Windows x86 / HLDS tests

Build the DLL from D341 and run the existing local-goal command on `de_dust2` (no external Python server):

```text
aipb modelio goal 1 8 30 <x> <y> <z>
aipb modelio status
```

Test A — revisit previously documented unreachable scene, with bot near `(-1210,2288,36)` and operator goal `(-1516,2424,36)` if the environment and start conditions are independently verified.
The earlier D340 run oscillated through lateral steps.
D341 should now prefer novel trace-authorized directions or stop with `GoalNoNovelDirection`, `GoalCycle`, `GoalNoProgress` or `GoalDeadEnd` rather than repeatedly alternating prior locations.
Check the actual per-leg `from=` coordinates against the previous confirmed positions, and never accept a false `goal PASS`.

Test B — repeat successful open-ground target `(1449,1800,36)` from its previously verified area, ensuring `goal PASS; reached=1` occurs after a real observed arrival and all five recovery flags are 1.

Test C — repeat blocked-forward reachable detour `(1200,1050,36)` from its previously verified area.
Expected: safely traced side movement, continued progress to the destination where a novel route remains, and full recovery.
A safe terminal failure is acceptable for the bounded local heuristic but means that scene's target-reach regression is not accepted.

Test D — operator `aipb modelio stop` during an active leg and verify `stop_reason=Requested`, `Idle` and all five recovery fields; also test a goal beyond 768 units and invalid coordinate to ensure pre-arm rejection is unchanged.

Automatic C++ unit tests in `tests/unit/ai_model_io_directions_tests.cpp` and `tests/unit/ai_model_io_recovery_tests.cpp` cover confirmed-only storage, recent-position exclusions, revisits, stalled paths, goal-radius exceptions, reset, invalid input, and named failure reasons.
The existing unit suite and include-order check should run in automatic CI.
Manual Windows x86 and `full` GitHub Actions jobs are dispatched only by the user.
Acceptance requires successful automatic CI and the later explicit operator-run game tests; publication alone is not acceptance.
