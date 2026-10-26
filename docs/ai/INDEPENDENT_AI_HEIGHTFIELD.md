# D208 — Multi-layer static collision heightfield

## Scope

Build reproducible static surface candidates from the world BSP collision partition without relying on render triangle meshes, YaPB runtime, game-engine headers, or learned behavior.
Store results only in a new diagnostic JSON file; retain `.aipbmap` v0.3.0 and navigation format unchanged.

## Sampling

Use the BSP world model's original bounds, a default 128-unit XY grid and a 32-unit vertical scan step.
The grid cell center is clipped to the world bounds, and each column is sampled separately with standing hull 1 and crouching hull 3.
When an interval begins outside `CONTENTS_SOLID` and ends in solid, perform an exact static downward segment trace.
Accept an impact only if it has a plane facing upward with `normal.z >= 0.7`.
Each candidate keeps the grid column/row, hull, contact position, impact normal and both actor hull occupancy results at that position.
Multiple isolated floors in one XY column remain distinct in descending order.
Counts include tested intervals, accepted impacts, rejected impacts, occupied columns and multi-level columns.
No mesh triangle is treated as direct proof of accessibility.

## Safety and interpretation

All queries are world-model-local and do not merge independently transformed brush models or dynamic doors/lifts.
A candidate is not a fully navigable surface; slope, step, jump, water, moving obstacles, engine trace tolerance and connected reachable area require later checks.
The static plane-boundary convention may put the exact impact position in an occupied region; the stored occupancy flags are diagnostic only.
Invalid dimensions, oversized grids and unreasonable scan depths fail explicitly rather than truncating results.
The default per-map budget is 16,384 XY columns and 512 vertical intervals.
No graph edges, NavModel mutations or in-game actions are created.

## CLI and verification

Run `aipb-map heightfield --input <map.aipbmap> --output <heightfield.json>` with an output path outside the source artifact.
The independent JSON schema is `aipb-heightfield-candidates` v0.1.0 and includes source and artifact digests.
Synthetic unit tests cover stacked floors, standing/crouching sampling, exact candidate ordering, deterministic JSON and invalid limits.
The 25 real CS 1.6 maps and engine behavior remain separate acceptance gates.
