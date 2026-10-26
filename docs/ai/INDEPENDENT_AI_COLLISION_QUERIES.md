# D207 — Static BSP collision queries

## Scope

Implement a read-only, STL-only collision-query module in the independent Map Tool with no changes to saved map/navigation artifact formats.
`CollisionQueries` validates the supplied `MapModel` once, then keeps a non-owning reference to its `CollisionPartition`.
Callers must keep the map and its collision data alive and unchanged for the lifetime of the query object.

## Contents queries

`pointContents(modelId, hull, point)` selects one BSP model by its existing 1-based object ID and selects hull 0, 1, 2 or 3.
Hull 0 uses `renderNodes` and interprets negative node references as `-(leafIndex + 1)`, then reads `CollisionLeaf.contents`.
Hulls 1–3 use `clipNodes` and interpret negative references directly as signed GoldSrc `CONTENTS_*` values.
The front half-space includes points exactly on the splitting plane.
A missing partition, model or hull and nonfinite coordinates are errors rather than silently classified as empty.
All queries are expressed in the stored model's plane coordinate space; no entity-origin subtraction or dynamic model transform is implicit.

## Segment tracing

`traceSegment(modelId, hull, start, end)` traverses BSP partition intervals iteratively in start-to-end order.
The first solid interval reached after an initially nonsolid start yields `fraction`, `endPosition` and an impact plane normal pointing from solid toward the entering side.
If the segment starts in solid, `startSolid` is true, the fraction is zero, and the position is the input start.
`allSolid` indicates that no nonsolid interval was visited.
`inOpen` and `inLiquid` report encountering empty and water/slime/lava/current contents along the segment, respectively.
No arbitrary distance epsilon is introduced; boundary splits use the signed source partition plane and a fraction in [0,1].
Fractions refer to the segment parameter, not to elapsed time or a player velocity model.
A bounded traversal budget prevents pathological partition expansion from running indefinitely; exceeding it raises an error.

## Static player hull probes

`canOccupy` and `canTraverse` choose GoldSrc precomputed hull 1 for `Standing` and hull 3 for `Crouching`.
They test model-space center/origin coordinates against the corresponding pre-expanded clip hull and treat `CONTENTS_SOLID` as blocked.
Other signed contents are returned by the lower-level APIs so callers can decide how to treat liquids, ladders and specialized contents.
These helpers do not combine collisions across multiple BSP models or apply brush-entity transforms, do not process moving doors/lifts, and do not simulate acceleration, step-up, slopes, jump arcs or gameplay physics.
A clear hull segment is therefore only a necessary static collision test, **not proof of traversability**.

## Verification

The CTest fixture exercises hull-0 leaf references, clipnode terminals, axial and nonaxial planes, boundary conditions, solid, empty and liquid contents, first impact, reversed impact normals, solid starts, wholly solid paths, standing/crouching probes, invalid inputs, a 4096-node depth and artifact round-trip.
The new module requires no YaPB runtime or additional dependency; strict compiler warnings remain enabled.
Future steps must validate on real CS 1.6 BSP maps and compare selected queries to game-engine movement before committing navigation links.
