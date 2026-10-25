# AiPB Map Tool

Independent native C++20 tool for offline map import, navigation generation, comparison and export.
D203 provides engine-independent `MapModel` and `NavigationModel`, validation and deterministic `.aipbmap`/`.aipbnav` JSONL artifacts with SHA-256 fingerprints.
D204 adds a separate GoldSrc BSP v30 importer and the `aipb-map import` CLI command.
D205 adds collision planes, render nodes, clipnodes, leaves and hull references, stored in map artifacts v0.2.0 (v0.1.0 map loading remains supported).
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
Entity properties with duplicate keys are rejected rather than silently discarded because `MapModel` currently stores a string-keyed map.
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
Inspection reads validated `.aipbmap` artifacts (v0.1.0 and v0.2.0), measures render geometry, collision shapes, entities and partition hull reachability and reports informational or warning findings.
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
