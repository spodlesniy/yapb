# AiPB Map Tool

Independent native C++20 tool for offline map import, navigation generation, comparison and export.
D203 provides engine-independent `MapModel` and `NavigationModel`, validation and deterministic `.aipbmap`/`.aipbnav` JSONL artifacts with SHA-256 fingerprints.
D204 adds a separate GoldSrc BSP v30 importer and the `aipb-map import` CLI command.
D205 adds collision planes, render nodes, clipnodes, leaves and hull references in map artifacts v0.2.0.
D206.6 writes map artifacts v0.3.0 with losslessly ordered entity properties, retaining read support for v0.1.0 and v0.2.0.
D206 adds read-only map inspection metrics and optional deterministic JSON reports without altering imported map artifacts.
Navigation builders, physical traversal validation and YaPB `.graph` export remain future work.

## Build and test

Requirements: CMake 3.21+, Ninja and a C++20 compiler (GCC, Clang or MSVC).
No crlib, linkage, ONNX Runtime, YaPB/GoldSrc SDK headers or game installation is needed.

From `projects/aipb-map/`:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
./build/debug/aipb-map --help
```

On Windows, use `build\debug\aipb-map.exe` in place of `./build/debug/aipb-map`.
The `release` preset is also available; generated output is ignored under `build/`.

## Import GoldSrc BSP v30

```sh
./build/debug/aipb-map import --format bsp \
  --input /path/to/cstrike/maps/de_dust2.bsp \
  --output ./results/de_dust2.aipbmap
```

The output directory must not exist.
The importer reads the 15 BSP lump descriptors, builds a separate mesh for the world and each brush model with faces, parses entity key/value pairs, records their brush-model references and origin, and writes a versioned map artifact.
The source SHA-256 is computed from the original BSP byte content.
The output contains render face geometry and collision partition trees for hulls 0–3 when provided by the source BSP.
Imported collision data is structural and is **not yet** suitable for proving player reachability.

Face textures, visibility, lighting, runtime collision traces and dynamic object simulation are intentionally deferred.
Repeated entity keys are retained in ordered `sourceProperties` while the existing `properties` map holds the last occurrence for each key.
Entity strings must be valid UTF-8 for the D203 JSON writer; legacy non-UTF-8 encodings require a future explicit import policy.
Only GoldSrc BSP version 30 is supported; malformed source files fail with an error rather than producing a partial artifact.
See [D204 BSP importer notes](../../docs/ai/INDEPENDENT_AI_BSP_IMPORT.md) and [D205 collision format](../../docs/ai/INDEPENDENT_AI_COLLISION.md).

## Inspect a map artifact (D206)

```sh
./build/debug/aipb-map inspect \
  --input ./results/de_dust2.aipbmap \
  --output ./results/de_dust2-inspection.json
```

`--output` is optional; summary and diagnostic findings are always printed to stdout.
The JSON report must be outside the `.aipbmap` directory, and existing reports are not overwritten.
Inspection reads validated `.aipbmap` artifacts (v0.1.0, v0.2.0 and v0.3.0), measures render geometry, collision shapes, entities and partition hull reachability and reports informational or warning findings.
It does not prove physical traversability, map coverage or correct GoldSrc runtime collision.
Actual de_dust2 import and inspection remain to be verified with a real BSP fixture.
See [D206 map-inspection specification](../../docs/ai/INDEPENDENT_AI_MAP_INSPECTION.md).

## Architecture

- `aipb_map_core`: STL-only geometry, map/navigation models, structural validation and shared SHA-256.
- `aipb_map_io`: JSON/JSONL serialization, artifact manifests and file integrity checks.
- `aipb_map_bsp`: GoldSrc BSP v30 reader and `IMapImporter` implementation.
- `aipb_map_analysis`: read-only geometry and collision-partition statistics with typed findings.
- `aipb_map_report`: deterministic JSON serialization of analysis reports; depends on the internal JSON codec but not on importer implementations.
- `aipb-map`: CLI integration, using the importer, artifact reader/writer and inspector.

There is no dependency on the repository root Meson/CMake build, the YaPB DLL, crlib or the GoldSrc SDK.
External constraints (such as YaPB waypoint/link limits) will be handled only by the future exporter.
Project-specific coding rules are in [AGENTS.md](AGENTS.md).

## Formatting and linting

C++ files under `projects/` inherit the shared [clang-format configuration](../.clang-format) and [clang-tidy configuration](../.clang-tidy), copied from `src/ai/`.
Run `clang-format -i` on C++ source and header files from the standalone project; use a version that understands all settings in the configuration (including newer braced-list and parentheses options).
The D206.1 initial formatting used clangd 17's compatible subset because its bundled formatter does not support all modern keys; the committed configurations remain identical to the originals.

D206.2 requires braces for all control-flow statements and meaningful blank-line separation.
See [shared style rules](../STYLE.md).

D206.3 adopts targeted modern C++20 patterns in the core and inspector, while allowing compact C++20 fixture and assertion idioms in tests.
See [shared C++20 conventions](../STYLE.md) for initialization, non-owning spans, ranges, ownership and test-specific rules.

D206.4 formats each designated initializer on its own line (including a single field) and each parameter of multi-parameter function signatures on a separate line.
These rules apply to production and test code; function-call argument formatting is unchanged.

To save detailed import diagnostics, append `--log-file results/de_dust2-import.log` to the import command.
The log is append-only with timestamps, import steps, source/artifact hashes and duplicate-key warnings without property values.

The optional `--log-file <path>` argument is supported by both `import` and `inspect`.
Import logs include timestamps, outcome, input/output paths, hashes, counts and duplicate entity-key warnings without key values.
Inspect logs also include the detailed JSON metrics report.
Logs are appended to the selected file, and normal console messages remain available.

BSP entities can reference models with no render faces or no usable render triangles; import preserves their collision-model links and logs a warning rather than inventing meshes.
D206.9 removes exact-zero-area render triangles before committing mesh vertices, preserving the winding and original collision partitions.
Import warnings identify BSP model/face indices and skipped triangle reasons, with totals in console output and optional detailed file logs.
Re-import an existing BSP into a new output directory to obtain cleaned geometry; old artifacts remain readable and report their original metrics.

## Static collision queries (D207)

Include `<aipb/map/collision/queries.h>` and construct `aipb::map::collision::CollisionQueries` with a validated, lifetime-stable `MapModel` containing a collision partition.
Use `pointContents(modelId, hull, position)` for BSP contents and `traceSegment(modelId, hull, start, end)` for static hull intersection details.
Use `canOccupy` and `canTraverse` with `PlayerStance::Standing` (GoldSrc clip hull 1) or `PlayerStance::Crouching` (clip hull 3).
Hull 0 is a point query through render-node leaves; hulls 1–3 use native clipnode contents.
These routines operate in each BSP model's local plane coordinate system, do not transform brush models or track dynamic door/lift state, and are not substitutes for actual GoldSrc gameplay collision and step/jump logic.
D207 keeps the existing `.aipbmap` v0.3.0 schema unchanged; queries are computed after loading.

## Offline collision validation (D207.1)

Run `aipb-map validate-collision --input ./results/de_dust2.aipbmap --output ./results/de_dust2-collision.json` after import.
The deterministic JSON report records source/artifact hashes, per-hull world/brush contents and trace statistics, spawn-origin probes and bounded consistency violations.
It refuses existing output and paths inside the input artifact, preserving `.aipbmap` v0.3.0 unchanged.
This samples static collision data and does not prove gameplay collision fidelity or walkability; dynamic brush actors remain separate.

D207.1.2 supports `aipb-map validate-collision --input <map.aipbmap> --output <report.json> [--log-file <log.txt>]` with append-only timestamped source/artifact digests, detailed world/brush hull metrics and violation diagnostics.
The `tools/import_all_bsp.bat` driver runs import, inspect and collision validation sequentially and writes sibling `collision-validation.json` and `collision-validation.log` after inspection succeeds.
Existing per-map result directories are skipped; use a fresh results directory or remove a previous map's results before rerunning the full pipeline.

## Spawn collision probes (D207.2)

The existing `validate-collision` command now includes a `spawn_probes` object in version 0.2.0 of its diagnostic JSON report.
It samples all detected spawn origins on the static world collision model in four horizontal directions (32 units) and both vertical directions (96 units) using hulls 0–3.
Results include per-hull horizontal clearance, floor/ceiling-oriented collision planes, standing/crouching occupancy differences and detailed per-entity fractions.
Older `.aipbmap` artifacts are still read unchanged; successful probes are not evidence of true in-game navigability.

## Multi-layer heightfield candidate sampling (D208)

Run `aipb-map heightfield --input ./results/de_dust2.aipbmap --output ./results/de_dust2-heightfield.json` for deterministic static world-only candidate diagnostics.
The sampler uses a bounded 128-unit XY grid, 32-unit vertical intervals, world BSP hulls 1 and 3 and upward-facing plane tests.
It records stacked floor candidates and clearance flags without modifying map artifacts or creating navigation links.
The fixed settings and sampling limitations are documented in [the D208 specification](../../docs/ai/INDEPENDENT_AI_HEIGHTFIELD.md).

## Independent Windows bulk stages (D208.1)

Run the following scripts from `projects/aipb-map/tools/` in order when building a fresh result set:

```bat
import_all_bsp.bat
inspect_all_bsp.bat
validate_collision_all_bsp.bat
heightfield_all_bsp.bat
```

`import_all_bsp.bat` discovers `build/debug/maps/*.bsp` recursively, imports each into `build/debug/results/<map>/<map>.aipbmap` and writes `import.log`.
The other scripts scan existing imported artifacts under `build/debug/results/<map>/` and respectively create `inspection.json` / `inspect.log`, `collision-validation.json` / `collision-validation.log`, or `heightfield.json`.
Each script independently skips already-generated output, reports per-map failures and exits nonzero on error.
Delete an individual report to regenerate only that stage; remove an artifact directory to re-import its BSP.
The heightfield CLI currently has no `--log-file` parameter; its standalone script uses the JSON report and console status.

## Heightfield candidate quality (D208.2)

Re-run `heightfield_all_bsp.bat` after removing old `heightfield.json` reports; the source `.aipbmap` artifacts do not require re-import.
Heightfield diagnostic schema v0.2.0 reports physical floor coordinates, merged standing/crouching `hull_mask`, point-hull support checks, thin-solid detections and rejection reasons.
The generator does not create navigation nodes, does not model dynamic BSP submodels and has not been validated against engine traces.

D208.3 updates the separate `heightfield.json` diagnostics to v0.3.0 with slope-aware point-hull confirmation, measured physical surface height, stance clearance rejection and per-reason counters.
Regenerate only the existing `heightfield.json` files using `heightfield_all_bsp.bat` (the script skips files that already exist); re-import is unnecessary.

## Navigation candidates (D209)

Run `aipb-map navigation-candidates --input ./results/de_dust2.aipbmap --output ./results/de_dust2-navigation-candidates.json` to produce a standalone deterministic `aipb-navigation-candidates` v0.1.0 report.
D209 regenerates the current heightfield from the checksummed artifact, assigns stable one-based candidate IDs and records heightfield source indices, XY cells, normals, hull masks, standing/crouching clearance and slope counts.
Spawn coverage uses horizontal proximity within 256 units and absolute floor-height difference within 96 units; it does **not** establish walkability or route connectivity.
No NavModel nodes/edges, `.aipbmap` data, game runtime or neural behavior are modified.

## Bulk navigation candidates (D209.1)

Run `projects/aipb-map/tools/navigation_candidates_all_bsp.bat` after compiling `aipb-map.exe` and importing BSP maps.
The script scans `build/debug/results/<map>/<map>.aipbmap`, saves `navigation-candidates.json` next to the other per-map reports, skips files that already exist and returns an error if any map fails.
It does not require an existing `heightfield.json` because the D209 CLI regenerates heightfield candidates from the imported artifact.
No import or previous processing stage is rerun.

## Adaptive spawn candidate coverage (D209.2)

The existing `navigation_candidates_all_bsp.bat` runs adaptive generation using the same CLI command.
It samples limited additional XY positions only near uncovered spawn origins, preserves provenance and emits `navigation-candidates.json` schema v0.2.0 with 64/128/256-unit coverage metrics.
Delete the existing `navigation-candidates.json` files before rerunning; no BSP re-import is necessary.

## Fixed brush navigation support (D209.3)

Regenerate `navigation-candidates.json` using `navigation_candidates_all_bsp.bat` to include static `func_wall` supports and per-candidate source model/entity provenance.
The output diagnostic schema is v0.3.0; `heightfield.json`, source `.aipbmap` and previous processing steps do not need regeneration.
Doors, breakables and moving brush entities are deliberately excluded from permanent navigation support.

D209.3.1 updates navigation-candidates.json to schema v0.3.1, fixes brush/sample provenance and preserves stable IDs across filtering; delete old reports before using `navigation_candidates_all_bsp.bat`.

## Preliminary connectivity (D210)

The independent CLI `aipb-map preliminary-connectivity --input <map.aipbmap> --output <preliminary-connectivity.json>` recomputes the D209.3.1 candidates and builds a bounded directed candidate adjacency report.
`projects/aipb-map/tools/preliminary_connectivity_all_bsp.bat` processes imported maps independently and skips existing reports.
The JSON schema `aipb-preliminary-connectivity` v0.1.0 retains map/source SHA-256, candidate IDs, directed edges, standing/crouching labels, rejected checks and weak-component diagnostics.
Default checks limit XY neighbor distance to 192 units, upward/downward steps to 18/36 units, and require clear static collision and intermediate support.
This is not an `.aipbnav` artifact or a certified GoldSrc path graph; in-game validation and dynamic obstacles remain out of scope.

## Breakable initial collision (D210.1)

The Map Tool now treats `func_breakable` with `spawnflags` bit 1 (Only Trigger) as conditional initial-state collision support, and all other breakables as initial obstacles without dependable support.
The two navigation stages share the classification, and Navigation Candidates v0.3.2 labels trigger-only support `until_trigger`.
Existing `.aipbmap` imports remain valid; regenerate both candidate and connectivity JSON reports to use the updated initial collision snapshot.
Actual round-time removal after a bomb/trigger remains outside this offline model.

## Separate preliminary jump transitions (D210.2)

The `preliminary-connectivity` CLI now emits JSON v0.2.0 with typed `walk`, `jump` and `crouch_jump` edges, candidate source SHA-256 binding and updated weak-component diagnostics.
Run `preliminary_connectivity_all_bsp.bat` after deleting old connectivity reports; BSP re-import or Navigation Candidates regeneration is unnecessary.
The 44/62 unit rise limits and arc checks are uncalibrated offline hypotheses, and actual movement and conditional brush disappearance need separate GoldSrc testing.

## Ballistic jump baseline (D210.3)

The independent jump transition checker now uses parameterized GoldSrc-style vertical ballistics and a conservative horizontal speed constraint rather than an invented parabola.
Preliminary Connectivity v0.3.0 records the profile and estimated flight/speed values; delete earlier connectivity reports and rerun `preliminary_connectivity_all_bsp.bat`.
Standing and crouched landing behavior above the ballistic apex must be calibrated in CS 1.6; no physics-equivalent claim is made.

## Airborne duck landings (D210.3.1)

Connectivity v0.4.0 evaluates the original airborne origin during the crouch hull transition and permits crouched landings at provisional floor rises up to 63 units.
Standing after landing is reported separately, not assumed.
Delete old `preliminary-connectivity.json` files and rerun the standalone connectivity batch; no BSP re-import or candidate regeneration is required.
