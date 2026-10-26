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

## D209.2 — Adaptive spawn coverage

The navigation-candidates diagnostic schema advances to 0.2.0 and preserves all original coarse candidates and stable one-based IDs.
For spawn entities without a point inside the configured XY/Z coverage window, the algorithm attempts bounded local XY probes at a 32-unit step using the existing heightfield collision checks.
The default budget is at most 256 additional XY locations per map; both the original heightfield budget and candidate limit remain enforced.
Extra accepted candidates are appended with `refined=true`, `source_spawn_id` and deterministic `sample_index`; their `heightfield_index` is null.
Coarse candidates retain their original `heightfield_index`, `refined=false` and null refinement provenance.
The separate `covered_64`, `covered_128` and `covered_256` counts use the same 96-unit vertical threshold; per-spawn fields contain matched candidate IDs or null.
These proximity counts are not proof of connectivity or engine-side reachability.

## D209.3 — Static brush collision support

Navigation candidate JSON advances to 0.3.0 and adds `support_kind`, `support_model_id` and `support_entity_id` provenance, with source model IDs referencing the unchanged `.aipbmap` partition.
Only `func_wall` is treated as fixed collision support; doors, breakables, rotating movers and triggers are excluded.
Brush-model coordinates are translated by their associated entity origin, with upward hull 0 and hull 1/3 contact traces and combined world/static-brush clearance.
Spawn coordinates are sampled first for each supported brush, followed by bounded 64-unit local XY sampling up to 4096 probes per map.
These extra contacts do not alter the D208 world-only heightfield; the navigation report separately counts static brush models, samples and emitted candidates.
Existing world candidates are checked against the static brushes for occupied stances and retained only when at least one stance remains free.
The reported surface support is provisional and does not prove collision equivalence for nontrivial entity transforms or live gameplay movement.

## D209.3.1 — Provenance and identity correction

Navigation-candidates diagnostics advance to schema v0.3.1.
World-heightfield candidates carry `support_kind=world`, `heightfield_index`, `source_column` and `source_row`; their sample index is null.
Spawn-refinement candidates carry `support_kind=spawn_refinement`, `source_spawn_id` and `sample_index`; their heightfield and grid indices are null.
Static brush candidates carry `support_kind=static_brush`, `support_model_id`, `support_entity_id` and `sample_index`; their heightfield and grid indices are null.
Surviving candidate IDs do not change when a brush blocks an earlier world candidate, and new brush IDs start above the original source candidate range.
IDs may therefore have gaps; consumers must look up `covered_candidate_id` by ID instead of treating it as a one-based array offset.

## D210.1 — Conditional breakable supports

`func_breakable` with bit 1 `Only Trigger` and without bits 2 or 256 is sampled as conditional initial geometry.
Other breakables, including pressure-sensitive models and models lacking spawnflags, block initial-world collision but do not supply navigation candidate support.
Every candidate has `support_condition`, with `until_trigger` on conditional breakable surfaces and `fixed` otherwise.
The JSON schema is `0.3.2`; no dynamic re-evaluation is performed.
