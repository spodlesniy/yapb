# D206 — Read-only Map Inspector and Metrics

## Contract and separation

The standalone `aipb_map_analysis` library accepts a `const MapModel&` and returns a `MapReport` value.
It depends only on `aipb_map_core`, not on the importer, serializer, GoldSrc SDK or YaPB runtime.
`aipb_map_report` serializes these values using the existing internal JSON codec; neither report nor diagnostics are stored inside `.aipbmap` or `.aipbnav`.
Inspections must not change input artifacts or their content SHA-256.
The report's `artifact_sha256` records the exact map artifact loaded from disk, while `source_sha256` identifies the original source bytes.

## CLI

```sh
aipb-map inspect --input results/de_dust2.aipbmap --output results/de_dust2-inspection.json
```

`--output` is optional; the CLI always prints a human-readable summary and findings.
The JSON report must be outside the input artifact directory; existing output files are never silently overwritten.
The CLI supports previously saved map artifacts v0.1.0 (without collision partitions) and v0.2.0 (with optional partitions), and relies on the existing integrity-checked `loadMap()`.
The inspection report uses independent `format: "aipb-map-inspection"`, `schema_version: "0.1.0"` and deterministic sorted JSON field keys.
The report JSON is **not** a new map schema and does not modify the source map fingerprint.

## Metrics

- Map/source/artifact identity, coordinate units, total render meshes/vertices/triangles and per-mesh counts.
- Render coordinate bounds from each mesh's raw vertices, and triangle collision bounds from explicit collision-shape vertices.
- Per-mesh unreferenced vertices and **exactly** zero-area/collinear triangles; this deliberately does not classify near-degenerate surfaces using an arbitrary tolerance.
- Total entities and deterministic entity-class frequency distribution.
- Presence and record counts of partition planes, hull-0 render branches, clip branches, leaves and collision models.
- For each collision model and each of hulls 0–3: signed headnode, unique reachable branches, distinct terminal references and longest branch depth.
- Counts of partition nodes and leaves not reachable from **any model hull root** and histogram of source leaf contents.
- Raw model-bounds union stored as `partition.source_model_bounds`, kept **separate** from collision-shape bounds because brush transforms and coordinate frames are not resolved.

Hull analysis uses iterative traversal and memoized longest paths, so deep acyclic trees do not require the C++ call stack.
Reachability is strictly graph reachability within imported partition arrays, **not** physical agent reachability.
Repeated node references are counted once per hull; distinct negative terminal references are counted by terminal value, not by each traversed edge.
Empty geometry bounds and absent partitions serialize as `null`, not invented zeros.

## Diagnostics and limits

Findings use explicit `severity`, `code` and `detail` fields.
Current codes are `zero_area_triangles` (warning), `unused_mesh_vertices` (info), `empty_render_geometry` (warning), `missing_collision_partition` (warning), and `unreferenced_partition_records` (info).
Unreferenced partition records can be legitimate leftovers or shared subtrees; the info finding does **not** declare the BSP corrupt.
Basic structural integrity and stream SHA checks remain the responsibilities of the existing validators and artifact reader; malformed artifacts are rejected rather than reported as suspicious valid maps.
This stage does not provide voxel coverage, walkability, texture semantics, physical traces, gameplay success rates, lighting or movement dynamics.
Raw render bounds can combine several model-local coordinate frames; they must not be interpreted as authoritative world-space actor collision bounds.

## Verification

The CTest suite checks deterministic JSON, id strings, counts, bounds, entity frequency, collinear triangles, unreferenced vertices/partition records, missing partitions, negative hull roots and 2048-node-deep collision trees.
An additional CLI integration test saves a checksummed fixture, invokes `inspect`, validates the JSON report, rejects report overwrites and output paths inside the artifact directory, and verifies that the input manifest remains unchanged.
GCC Debug/Release and Clang Debug local checks are expected; standalone Linux/Windows CI is the publication gate.
Inspection of an actual Counter-Strike `de_dust2.bsp` and comparison with in-game geometry remain outstanding evidence, not an asserted completion of map correctness.
