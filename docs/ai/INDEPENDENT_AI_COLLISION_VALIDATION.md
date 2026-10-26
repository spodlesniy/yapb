# D207.1 — Offline collision query validation

## Contract

The `aipb-map validate-collision --input <map.aipbmap> --output <report.json>` command runs read-only deterministic tests against a checksummed saved map.
The validation uses existing D207 `CollisionQueries` without modifying its interpretation of BSP partitions, map data or artifact format.
The report is independent JSON with `format: aipb-collision-validation` and `schema_version: 0.1.0`; identifiers remain tied to source and artifact SHA-256.

## Sampling

Each world and brush collision model contributes its model-bounds midpoint, minimum, maximum and four X/Y face midpoints, in fixed order.
Each position is checked with `pointContents` and a stationary `traceSegment` for hulls 0–3.
Adjacent positions are joined by six straight-line probes, recorded per hull.
Spawn origins of `info_player_start`, `info_player_deathmatch` and `info_vip_start` are also sampled on the world model, without assuming they are safe places to stand.
Per-hull counters separate world and brush model scopes; a brush-model query does not imply the brush is actually present in world space at that position.
The report records contents (solid, empty, other), point and segment totals, solid starts, clear traces and reported impacts.
Consistency checks cover stationary contents against start-solid/all-solid trace state, player occupancy, bounded finite trace results and traversal agreement.
At most 20 example violations appear in the report while the total count remains complete.

## Limits

This is an internal, finite, repeatable smoke check rather than a path planner, geometry coverage proof or independent GoldSrc reference.
The sample positions do not cover all walkable coordinates, blocked passages, dynamic doors, ladders, water semantics, engine collision tolerances or step/jump behavior.
A zero-violation report alone does not establish a correct trace algorithm: independent engine comparisons and representative hand-selected map probes are still required.
Validation is non-mutating, refuses existing output files and rejects a report path inside the input artifact directory.
Missing/invalid collision partitions fail explicitly.
The report contains no modifications to `.aipbmap` v0.3.0 or `.aipbnav`.

## Reproduction

Generate the map artifacts through the ordinary `aipb-map import` command.
For each map run `aipb-map validate-collision --input <map.aipbmap> --output <map-collision-validation.json>`.
Keep the generated JSON files and source/artifact SHA-256 fingerprints for comparing the 25 standard maps.
Run CMake/CTest on GCC, Clang and MSVC before interpreting gameplay collision results.

## D207.1.2 — Optional log file and bulk driver

`validate-collision` accepts optional `--log-file <path>` in any order after the subcommand alongside required `--input` and `--output`.
The append-only timestamped log contains the validation start, input and report paths, source/artifact SHA-256, full validation JSON, world/brush hull counts, bounded violation examples, summary and final success or violation status.
The console remains concise and `collision-validation.json` remains deterministic and independent of the log.
Log and JSON report paths must be outside the input artifact; duplicate options and empty paths are rejected.
The Windows `tools/import_all_bsp.bat` script runs `validate-collision` only after successful `inspect`, producing `collision-validation.json` and `collision-validation.log` per map.
Existing result directories are still skipped, so users need new output directories for a fresh end-to-end run.
