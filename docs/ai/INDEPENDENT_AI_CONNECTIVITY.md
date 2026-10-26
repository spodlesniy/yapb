# D210 — Preliminary Navigation Connectivity

## Scope

Build a read-only connectivity report for the D209.3.1 independent map candidates.
D210 generates no game-runtime behavior, no learned model, no waypoint imitation and no persistent `.aipbnav` artifact.
Edges are directed hypotheses rather than guaranteed walking moves.

## Neighbor sampling and budgets

Group candidate positions in XY buckets of the configured maximum 192-unit planar distance.
Each candidate considers buckets within one neighbor cell, testing closer candidates first, then ascending stable candidate ID.
The default minimum XY separation is 1 unit.
Constrain each source to 32 neighbor trials, at most eight accepted outgoing edges, up to 1,000,000 directed trials and 150,000 edges per map.
All limits and derived counts appear in the deterministic `aipb-preliminary-connectivity` v0.1.0 report.
Candidate IDs are preserved, including intentional gaps after D209.3.1 filtering.

## Conservative static movement validation

Reject height rises above 18 units or drops beyond 36 units.
Require both endpoints to permit the same stance, prefer standing when possible, then fall back to crouching.
Trace the corresponding pre-expanded collision hull against worldspawn and fixed `func_wall` brush models.
For brush queries translate world positions to model-local coordinates using the associated entity origin.
Reject solid starts or impacts in static geometry.
Check support at 25%, 50% and 75% of the straight segment with downward traces in the matching player hull.
The support tolerance is 10 units beneath the corresponding stance's lower extent.
These heuristic tests deliberately exclude jump, ladder, lift, movement acceleration and dynamic state.

## Output and diagnostics

Write a separate JSON report including map name, source and artifact hashes, candidate count, accepted edges, reason-coded rejections, stance counts, weak components, largest component and isolated candidate count.
An undirected union of accepted directed links is used only for the **weak** component statistic, not directed path reachability.
CLI output is strictly outside the map artifact and existing reports are never overwritten.
Tests cover directed links, candidate IDs with gaps, rejected height differences, deterministic output and no input mutation.
The automatic GCC/Clang/MSVC Map Tool CI checks the independent implementation; real-map and in-engine evaluation remain separate acceptance gates.

## D210.1 — Initial breakable collision

The candidate builder and connectivity checker share the same func_breakable spawnflags policy.
All breakables are initial collision obstacles; trigger-only models can supply conditional support until a trigger destroys them.
Player-breakable and pressure-sensitive models are never considered reliable intermediate support.
This static snapshot can become outdated during a round; collision state will need an explicit runtime version in a later step.

## D210.2 — Jump and crouch-jump hypotheses

The preliminary connectivity report advances to v0.2.0, adding `traversal=walk|jump|crouch_jump`, `conditional_support`, jump rejection counters and a `candidates_sha256` of the canonical versioned Navigation Candidates JSON.
Directed jump edges are generated only for height increases above the normal 18-unit step limit, within 160 XY units, with separate maximum rises of 44 for ordinary jump and 62 for crouch-jump.
The airborne sweep uses 12 bounded hull-traced arc segments; crouch-jump changes from standing hull 1 to crouching hull 3 during the middle portion then returns to standing for landing.
Each landing must have upward-facing hull support beneath the candidate from world or nonmutable static brush; player-breakable brushes do not provide reliable support.
The arc parameters are conservative engineering hypotheses, not verified GoldSrc movement constants or a claim that ducking increases maximum ballistic apex.
Jump edges can bridge previously weakly disconnected pieces; component and isolated-candidate metrics are recomputed for all accepted edges and conditional-source components are counted separately.
No ladders, long falls, ledge grabs, moving platforms, air acceleration, friction or round-time destruction are simulated yet.
