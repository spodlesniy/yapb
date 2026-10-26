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

## D208.2 — Candidate-quality gates

The standalone diagnostic JSON advances to schema v0.2.0; input `.aipbmap` files remain unchanged.
GoldSrc hulls 1/3 sample expanded solid and report actor-origin contacts, not physical floor heights.
For an upward-facing plane normal `n`, convert each contact to an estimated world floor height by subtracting `(16(|nx|+|ny|)+height*nz)/nz`, where height is 36 for standing and 18 for crouching.
The offset corresponds to an axis-aligned ±16 XY GoldSrc player hull and ±height Z, and remains provisional for engine-specific contact tolerances.
Reject nonfinite or out-of-world floor heights, contacts outside known world render Z extents, and contacts whose lifted origin remains in clip-hull SOLID.
Perform an independent point-hull-0 short downward trace at the reconstructed surface and require a sufficiently upward-facing impact close to the expected floor height.
The test is intentionally conservative: false negatives are possible for ramps, BSP boundaries and world geometry absent from render meshes.
Trace all air-start vertical intervals, even where both endpoint contents are air, to expose thin solid spans that endpoint tests missed.
Coalesce contacts inside each XY cell when surface height difference is ≤1 GoldSrc unit; each candidate has `hull_mask` bit 1 for hull 1 and bit 3 for hull 3 (combined mask 10).
The old `hull` field is replaced with `hull_mask` and `position.z` now denotes the reconstructed physical floor, so clients must consume schema v0.2.0 explicitly.
Diagnostic counters include raw accepted floor hits, thin interval hits, merged contacts, and rejections by geometry/coordinate quality reason.
Candidates remain world-only, sampled at the centers of 128-unit XY cells and cannot be considered graph nodes without further reachability checks.

## D208.3 — Slope and surface validation

Heightfield diagnostic JSON v0.3.0 adds `surface_height_tolerance` (default 4 units), `slope_impacts`, `accepted_slope_contacts`, `corrected_point_contacts`, and separately classified point-hull and clearance rejections.
A point-hull trace starts above the accepted clip contact and continues below the estimated point-contact surface; an allowable height discrepancy includes the hull's 16-unit XY half-extents projected onto the contact slope.
The point-hull collision provides the final reported surface height and normal.
Standing/crouching clearance uses the recovered plane orientation and each GoldSrc hull's projected support distance, with a positive lift to avoid boundary ambiguity.
Candidates with neither available stance are rejected.
The checks remain conservative static estimates, do not model dynamic brush entities and cannot prove reachability or exact engine collision.
