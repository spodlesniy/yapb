# D340 — Bounded operator-specified local target movement

## Scope

D339 automatic CI and the user's `de_dust2` HLDS run passed: eight confirmed `memory` legs, masks 2/7/6/3/7/7/2/3, three blocked-forward advances and all five recovery fields true.
The prior comparative D338 gate remains open because independent `avoid` and `memory` runs did not share an identical start.
D340 adds a distinct server-side command for manually specified world-space coordinates, not a new trained policy or general-purpose pathfinder.
The command is strictly opt-in, requires a live Neural bot and the existing exclusive trial owner, and uses the existing engine-local deterministic Model I/O backend.
It does **not** use a Python TCP server or introduce any new Model I/O wire fields.
Existing `run`, `external`, `sequence`, `session`, `steer`, `avoid`, `memory` and `status` commands remain unchanged.

## Game command

```text
aipb modelio goal <bot-index> <actions:4-8> <seconds:5-30> <x> <y> <z>
```

All coordinates are absolute GoldSrc world coordinates, not offsets.
The goal must be finite, different from the bot's initial position by more than 16 units, within 768 horizontal units and within 8 units of initial bot height.
The operator must choose a supported, physically reachable, grounded destination.
The engine performs a new standing/crouching `TraceHull` for every candidate before every action.
Candidates comprise one forward step towards the destination and two lateral steps relative to that bearing.
Each is at most 96 units; a closer final goal yields a shortened step.
The controller selects the eligible candidate minimizing direct 3D distance to the operator goal, breaking ties in forward/left/right order.
A lateral detour is only a heuristic: the trial does not map walls, plan around corners, change altitude, or guarantee convergence.
On each leg the same existing deterministic model decision, `MoveToPosition` execution, correlated terminal feedback, physical target check and five-field recovery are required.
After a verified action, the controller checks the *observed* bot position against the goal within a 16-unit 3D radius.
It stops early with `goal PASS; reached=1` after a verified goal, or after the configured action cap with `goal NOT_REACHED; reached=0` and `GoalNotReached` stop reason.
Invalid input, no valid collision-traced direction, failed physical completion, height divergence, round/map/mode/session change, explicit stop and expired session budget all terminate without reporting goal success.
No speculative destination or future target is sent over the Model I/O peer.

## Manual Windows x86 / HLDS acceptance

1. On `de_dust2`, ensure a live bot in Neural mode standing on level ground, with no other active Mock/Explorer/Model I/O test.
2. Choose a reachable location on the same flat surface 150–300 units away; record its absolute world coordinates (do not enter arbitrary coordinates).
3. From the HLDS server console, for example if the verified destination is exactly `(1600, 2000, 36)`:

   ```text
   aipb modelio goal 1 8 30 1600 2000 36
   ```

4. Confirm `goal local direction` and each `sequence leg=N/8 PASS` print; a successful run stops at `goal PASS; reached=1; remaining<=16`, with five recovery fields true.
5. Test a target already within 16 units, an unreachable destination within local bounds, out-of-range coordinates, a nonfinite coordinate, a height mismatch, the eight-action cap, an explicit `aipb modelio stop`, and restoration after each scenario.
6. Test ordinary `memory` with its Python localhost peer to confirm the existing external path remains unaffected.

A denied or unreachable target is a valid bounded failure. It is **not** sufficient to observe `sequence leg=8/8 PASS` to claim goal reach.
The command is an experimental movement control, not safe autonomous navigation across unknown obstacles.

## Unit-test acceptance

The existing `ai_model_io_directions_tests.cpp` test target covers finite and bounded destinations, 16-unit termination radius, short final step, three goal-relative directions, forbidden candidates and trace-authorized selection.
The existing lifecycle/recovery contracts are reused without increasing session caps.
Automatic CI and user-run Windows x86 / HLDS acceptance are independent gates.
Only the user may dispatch `windows-x86` and `full` builds.
