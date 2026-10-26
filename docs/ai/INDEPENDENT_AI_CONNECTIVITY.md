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
