# D209 — Navigation Candidates

## Contract

The read-only `navigation-candidates` command computes the current D208.3 heightfield directly from a validated saved map artifact and produces an independent versioned diagnostic JSON.
Candidates are provisional static points, not navigation graph nodes, walkable regions or connected gameplay routes.
No inference, waypoint copying or original YaPB behavioral supervision is involved.

## Candidate provenance

Each candidate retains a deterministic one-based ID, zero-based heightfield source index, XY column and row, world-space point, floor normal, hull mask and independent standing/crouching clearance booleans.
Candidate order follows the deterministic heightfield ordering; no adjacency or post-processing connections are inferred.
The report includes map source SHA-256 and map artifact SHA-256, occupied XY columns, multi-layer columns and stance/slope counts.
Invalid nonfinite coordinates, blocked stances, invalid hull masks, inconsistent source hashes and unreasonable input sizes fail explicitly.

## Spawn coverage diagnostics

Identify world spawn classes `info_player_start`, `info_player_deathmatch` and `info_vip_start` in source entity order.
For each spawn retain ID, class, origin, nearest candidate by horizontal distance (breaking ties by vertical separation), planar and vertical distances, and a separate covered candidate ID.
The default diagnostic window is horizontal distance at most 256 units and absolute vertical separation at most 96 units.
The nearest XY point may be vertically distant and therefore uncovered; covered IDs are nullable.
The report records spawn count, spawns with a horizontally nearby candidate and spawns covered by both thresholds.
These metrics are provisional geometric proximity, not route reachability, static occupancy at spawn, collision in-between, or GoldSrc engine parity.

## Output and acceptance

The JSON report uses format `aipb-navigation-candidates` and schema v0.1.0 and is written only outside the original `.aipbmap` artifact.
Existing reports are never overwritten; no graph edges, `.aipbnav` exports or other source artifacts are produced.
Synthetic unit tests and CLI smoke tests cover provenance, stance counts, stacked levels, height-separated spawns, deterministic output, invalid input and preservation of the input artifact.
Before graph construction, compare reports from all 25 real maps and inspect weak spawn coverage and sparse surfaces separately.
